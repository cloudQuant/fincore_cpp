#include <fincore/runtime/utilities.hpp>

#include <fincore/errors.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <limits>
#include <map>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>

namespace fincore {
namespace {

std::size_t checked_shape_size(const std::vector<std::size_t> &shape,
                               const std::string_view name) {
  if (shape.empty()) {
    throw input_error{std::string{name} + " must be an array, not a scalar"};
  }

  std::size_t product = 1U;
  for (const std::size_t dimension : shape) {
    if (dimension != 0U &&
        product > std::numeric_limits<std::size_t>::max() / dimension) {
      throw input_error{std::string{name} + " shape is too large"};
    }
    product *= dimension;
  }
  return product;
}

void validate_double_frame(const dataframe &frame) {
  const auto [row_count, column_count] = frame.shape();
  const auto columns = frame.get_columns_info<double>();
  if (columns.size() != column_count) {
    throw input_error{"time-series alignment supports only double columns"};
  }
  for (const auto &info : columns) {
    const std::string name{std::get<0>(info).c_str()};
    const auto &values = frame.get_column<double>(name.c_str());
    if (values.size() != row_count) {
      throw input_error{"time-series columns must match the index length"};
    }
  }
}

std::unordered_map<timestamp, std::size_t>
index_positions(const dataframe &frame) {
  std::unordered_map<timestamp, std::size_t> positions;
  const auto &index = frame.get_index();
  positions.reserve(index.size());
  for (std::size_t position = 0U; position < index.size(); ++position) {
    if (!positions.emplace(index[position], position).second) {
      throw input_error{"duplicate time-series labels are ambiguous"};
    }
  }
  return positions;
}

void validate_policy(const alignment_policy policy) {
  switch (policy) {
  case alignment_policy::strict:
  case alignment_policy::inner:
  case alignment_policy::outer_dropna:
    return;
  }
  throw input_error{"unknown time-series alignment policy"};
}

// NumPy 1.26 uses this recursive pairwise reduction for contiguous float64
// sums. Keeping the reduction tree matters because std() uses it once for the
// mean and again for squared deviations.
double numpy_pairwise_sum(const std::span<const double> values) {
  constexpr std::size_t block_size = 128U;
  if (values.size() < 8U) {
    double result = -0.0;
    for (const double value : values) {
      result += value;
    }
    return result;
  }

  if (values.size() <= block_size) {
    std::array<double, 8U> partial{};
    std::copy_n(values.begin(), partial.size(), partial.begin());
    const std::size_t aligned_size = values.size() - values.size() % 8U;
    std::size_t position = 8U;
    for (; position < aligned_size; position += 8U) {
      for (std::size_t lane = 0U; lane < partial.size(); ++lane) {
        partial[lane] += values[position + lane];
      }
    }

    double result = ((partial[0] + partial[1]) + (partial[2] + partial[3])) +
                    ((partial[4] + partial[5]) + (partial[6] + partial[7]));
    for (; position < values.size(); ++position) {
      result += values[position];
    }
    return result;
  }

  std::size_t middle = values.size() / 2U;
  middle -= middle % 8U;
  return numpy_pairwise_sum(values.first(middle)) +
         numpy_pairwise_sum(values.subspan(middle));
}

dataframe
select_rows(const dataframe &frame,
            const std::vector<timestamp> &selected_index,
            const std::unordered_map<timestamp, std::size_t> &positions) {
  dataframe result;
  result.load_index(std::vector<timestamp>(selected_index));
  for (const timestamp label : selected_index) {
    if (!positions.contains(label)) {
      throw input_error{"aligned timestamp is absent from a frame"};
    }
  }
  for (const auto &info : frame.get_columns_info<double>()) {
    const std::string name{std::get<0>(info).c_str()};
    const auto &source = frame.get_column<double>(name.c_str());
    std::vector<double> selected;
    selected.reserve(selected_index.size());
    for (const timestamp label : selected_index) {
      const auto found = positions.find(label);
      if (found == positions.end()) {
        throw input_error{"aligned timestamp is absent from a frame"};
      }
      selected.push_back(source[found->second]);
    }
    result.load_column<double>(name.c_str(), std::move(selected),
                               hmdf::nan_policy::dont_pad_with_nans);
  }
  return result;
}

void validate_positional_array(const dense_array &value,
                               const std::string_view name) {
  const std::size_t shape_size = checked_shape_size(value.shape, name);
  if (shape_size != value.values.size()) {
    throw input_error{std::string{name} + " shape does not match values"};
  }
}

} // namespace

dense_array validate_finite_array(const dense_array &value,
                                  const std::string_view name,
                                  const std::int64_t min_size,
                                  const std::optional<std::int64_t> ndim) {
  if (min_size < 0) {
    throw input_error{"min_size must be a non-negative integer"};
  }
  if (ndim.has_value() && *ndim < 0) {
    throw input_error{"ndim must be a non-negative integer or None"};
  }
  const std::size_t shape_size = checked_shape_size(value.shape, name);
  if (ndim.has_value() &&
      value.shape.size() != static_cast<std::size_t>(*ndim)) {
    const auto dimensions =
        *ndim == 1 ? "one-dimensional" : "dimensionality mismatch";
    throw input_error{std::string{name} + " must be " + dimensions};
  }
  if (shape_size != value.values.size()) {
    throw input_error{std::string{name} + " shape does not match values"};
  }
  if (value.values.size() < static_cast<std::size_t>(min_size)) {
    throw input_error{std::string{name} + " must contain at least " +
                      std::to_string(min_size) + " values"};
  }
  if (std::ranges::any_of(value.values, [](const double item) {
        return !std::isfinite(item);
      })) {
    throw input_error{std::string{name} + " must contain only finite values"};
  }
  return value;
}

dense_array
native_backend::as_float_array(const dense_array &value,
                               const std::string_view name,
                               const std::optional<std::int64_t> ndim) const {
  return validate_finite_array(value, name, 1, ndim);
}

dense_array native_backend::cumulative_product(const dense_array &value) const {
  auto result = as_float_array(value, "value", 1);
  double product = 1.0;
  for (double &item : result.values) {
    product *= item;
    item = product;
  }
  return result;
}

double
native_backend::sample_standard_deviation(const dense_array &value) const {
  const auto array = as_float_array(value, "value", 1);
  if (array.values.size() < 2U) {
    throw input_error{
        "value must contain at least 2 values for sample standard deviation"};
  }

  const double mean = numpy_pairwise_sum(array.values) /
                      static_cast<double>(array.values.size());
  std::vector<double> squared_deviations;
  squared_deviations.reserve(array.values.size());
  for (const double item : array.values) {
    const double deviation = item - mean;
    squared_deviations.push_back(deviation * deviation);
  }
  const double sum_squared_deviations = numpy_pairwise_sum(squared_deviations);
  return std::sqrt(sum_squared_deviations /
                   static_cast<double>(array.values.size() - 1U));
}

dataframe normalize_time_series_timezone(const dataframe &value,
                                         const std::string_view normalize_tz) {
  std::string normalized{normalize_tz};
  std::ranges::transform(normalized, normalized.begin(),
                         [](const unsigned char ch) {
                           return static_cast<char>(std::toupper(ch));
                         });
  if (normalized != "UTC") {
    throw input_error{"normalize_tz currently supports only 'UTC'"};
  }
  return value;
}

std::vector<dataframe> align_time_series(const std::vector<dataframe> &values,
                                         const alignment_policy policy) {
  validate_policy(policy);
  if (values.empty()) {
    return {};
  }

  std::vector<std::unordered_map<timestamp, std::size_t>> positions;
  positions.reserve(values.size());
  for (const auto &frame : values) {
    validate_double_frame(frame);
    positions.push_back(index_positions(frame));
  }

  std::vector<timestamp> aligned_index;
  if (policy == alignment_policy::strict) {
    aligned_index.assign(values.front().get_index().begin(),
                         values.front().get_index().end());
    for (std::size_t frame = 1U; frame < values.size(); ++frame) {
      const auto &index = values[frame].get_index();
      if (index.size() != aligned_index.size() ||
          !std::equal(index.begin(), index.end(), aligned_index.begin())) {
        throw input_error{"strict alignment requires identical indices"};
      }
    }
  } else {
    std::set<timestamp> candidates;
    for (const auto &frame : values) {
      const auto &index = frame.get_index();
      candidates.insert(index.begin(), index.end());
    }
    if (policy == alignment_policy::inner) {
      for (const timestamp label : candidates) {
        const bool present_everywhere =
            std::ranges::all_of(positions, [label](const auto &lookup) {
              return lookup.contains(label);
            });
        if (present_everywhere) {
          aligned_index.push_back(label);
        }
      }
    } else {
      for (const timestamp label : candidates) {
        bool complete = true;
        for (std::size_t frame = 0U; frame < values.size() && complete;
             ++frame) {
          const auto row = positions[frame].find(label);
          if (row == positions[frame].end()) {
            // A zero-column DataFrame contributes no missing cells to pandas'
            // outer concat. Its later .loc selection still requires the label.
            if (values[frame].get_columns_info<double>().empty()) {
              continue;
            }
            complete = false;
            break;
          }
          for (const auto &info : values[frame].get_columns_info<double>()) {
            const std::string name{std::get<0>(info).c_str()};
            const double item =
                values[frame].get_column<double>(name.c_str())[row->second];
            if (std::isnan(item)) {
              complete = false;
              break;
            }
          }
        }
        if (complete) {
          aligned_index.push_back(label);
        }
      }
    }
  }

  std::vector<dataframe> result;
  result.reserve(values.size());
  for (std::size_t frame = 0U; frame < values.size(); ++frame) {
    result.push_back(
        select_rows(values[frame], aligned_index, positions[frame]));
  }
  return result;
}

std::pair<dataframe, dataframe>
align_binary_metric_inputs(const dataframe &left, const dataframe &right,
                           const alignment_policy policy) {
  auto aligned = align_time_series({left, right}, policy);
  return {std::move(aligned[0]), std::move(aligned[1])};
}

std::pair<dense_array, dense_array>
align_binary_metric_inputs(const dense_array &left, const dense_array &right,
                           const alignment_policy policy) {
  validate_policy(policy);
  validate_positional_array(left, "left");
  validate_positional_array(right, "right");
  if (left.shape.front() != right.shape.front()) {
    throw input_error{
        "positional array inputs must have the same first-axis length"};
  }
  return {left, right};
}

void load_optional_module(const std::string_view module_name,
                          const bool available,
                          const std::string_view dependency,
                          const std::string_view extra,
                          const std::string_view message) {
  if (available) {
    return;
  }
  const std::string resolved_dependency =
      dependency.empty() ? std::string(module_name) : std::string(dependency);
  const std::string resolved_message =
      message.empty()
          ? "optional_dependency_missing: " + resolved_dependency +
                " is required"
          : std::string(message);
  throw fincore_error{dependency_error(resolved_message,
                                       resolved_dependency,
                                       std::string(extra))};
}

} // namespace fincore
