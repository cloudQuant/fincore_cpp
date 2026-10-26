#include <fincore/simulation/result.hpp>

#include <fincore/detail/numeric.hpp>
#include <fincore/error.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <new>
#include <span>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace fincore {
namespace {

struct materialized_simulation_paths {
  dataframe paths;
  std::vector<double> terminal_values;
};

[[noreturn]] void throw_nonfinite_statistic() {
  throw input_error{"simulation terminal statistics are not finite"};
}

void require_finite_terminal(const double value) {
  if (!std::isfinite(value)) {
    throw input_error{"simulation terminal values must be finite"};
  }
}

// A simulation artifact reports an unbounded statistic as an error rather than as
// the NaN the metrics helpers return, because the caller here is producing a
// result object whose percentiles and standard deviation are expected to be
// numbers; a single path is the exception, where dispersion is genuinely
// undefined and NaN is the honest answer.
double checked_mean(const std::span<const double> values) {
  double total = 0.0;
  for (const double value : values) {
    total += value;
    if (!std::isfinite(total)) {
      throw_nonfinite_statistic();
    }
  }
  const double result = total / static_cast<double>(values.size());
  if (!std::isfinite(result)) {
    throw_nonfinite_statistic();
  }
  return result;
}

double checked_sample_standard_deviation(const std::span<const double> values,
                                         const double mean) {
  if (values.size() == 1U) {
    return std::numeric_limits<double>::quiet_NaN();
  }

  double sum_of_squared_deviations = 0.0;
  for (const double value : values) {
    const double deviation = value - mean;
    const double squared_deviation = deviation * deviation;
    if (!std::isfinite(deviation) || !std::isfinite(squared_deviation)) {
      throw_nonfinite_statistic();
    }
    sum_of_squared_deviations += squared_deviation;
    if (!std::isfinite(sum_of_squared_deviations)) {
      throw_nonfinite_statistic();
    }
  }
  const double result = std::sqrt(sum_of_squared_deviations /
                                  static_cast<double>(values.size() - 1U));
  if (!std::isfinite(result)) {
    throw_nonfinite_statistic();
  }
  return result;
}

double checked_percentile(const std::span<const double> values,
                          const double cutoff) {
  const double result = detail::linear_percentile(values, cutoff);
  if (!std::isfinite(result)) {
    throw_nonfinite_statistic();
  }
  return result;
}

simulation_terminal_statistics
make_statistics(const std::span<const double> terminal_values) {
  const double mean = checked_mean(terminal_values);
  const double sample_standard_deviation =
      checked_sample_standard_deviation(terminal_values, mean);
  // The seven cutoffs are the artifact's fixed vocabulary, evaluated in ascending
  // order over the same terminal values the mean and standard deviation used; the
  // median is hoisted into its own field because callers reach for it constantly
  // while the struct keeps the full set for whoever needs the tails.
  const simulation_terminal_percentiles percentiles{
      checked_percentile(terminal_values, 0.01),
      checked_percentile(terminal_values, 0.05),
      checked_percentile(terminal_values, 0.25),
      checked_percentile(terminal_values, 0.50),
      checked_percentile(terminal_values, 0.75),
      checked_percentile(terminal_values, 0.95),
      checked_percentile(terminal_values, 0.99),
  };
  return {mean, sample_standard_deviation, percentiles.percentile_50,
          percentiles};
}

materialized_simulation_paths
materialize_simulation_paths(const dataframe &paths) {
  const auto &index = paths.get_index();
  if (index.empty()) {
    throw input_error{"simulation paths must have a nonempty index"};
  }

  const auto shape = paths.shape();
  const auto double_columns = paths.get_columns_info<double>();
  if (shape.second == 0U || double_columns.size() != shape.second) {
    throw input_error{"simulation paths must contain only double columns"};
  }
  if (double_columns.size() > std::vector<double>{}.max_size()) {
    throw input_error{"simulation terminal distribution is not representable"};
  }

  std::vector<double> terminal_values;
  terminal_values.reserve(double_columns.size());
  for (const auto &info : double_columns) {
    const std::string name{std::get<0>(info).c_str()};
    const auto &values = paths.get_column<double>(name.c_str());
    if (values.size() != index.size()) {
      throw input_error{
          "simulation path columns must match the frame index length"};
    }
    require_finite_terminal(values.back());
    terminal_values.push_back(values.back());
  }

  return {dataframe{paths}, std::move(terminal_values)};
}

bool valid_alpha(const double alpha) {
  return std::isfinite(alpha) && alpha > 0.0 && alpha < 1.0;
}

} // namespace

simulation_result::simulation_result(
    dataframe paths, const simulation_terminal_statistics statistics,
    std::vector<double> terminal_values)
    : paths_{std::move(paths)}, statistics_{statistics},
      terminal_values_{std::move(terminal_values)} {}

const dataframe &simulation_result::paths() const noexcept { return paths_; }

const simulation_terminal_statistics &
simulation_result::statistics() const noexcept {
  return statistics_;
}

std::size_t simulation_result::path_count() const noexcept {
  return terminal_values_.size();
}

std::size_t simulation_result::observation_count() const noexcept {
  return paths_.get_index().size();
}

double simulation_result::terminal_value_at_risk(const double alpha) const {
  if (!valid_alpha(alpha)) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  return detail::linear_percentile(terminal_values_, alpha);
}

double simulation_result::terminal_conditional_value_at_risk(
    const double alpha) const {
  if (!valid_alpha(alpha)) {
    return std::numeric_limits<double>::quiet_NaN();
  }

  const double value_at_risk = terminal_value_at_risk(alpha);
  double total = 0.0;
  std::size_t count = 0U;
  for (const double value : terminal_values_) {
    if (value <= value_at_risk) {
      total += value;
      if (!std::isfinite(total)) {
        throw_nonfinite_statistic();
      }
      ++count;
    }
  }
  if (count == 0U) {
    throw input_error{"simulation terminal distribution has no CVaR tail"};
  }
  const double result = total / static_cast<double>(count);
  if (!std::isfinite(result)) {
    throw_nonfinite_statistic();
  }
  return result;
}

simulation_result analyze_simulation_paths(const dataframe &paths) {
  try {
    materialized_simulation_paths materialized =
        materialize_simulation_paths(paths);
    const simulation_terminal_statistics statistics =
        make_statistics(materialized.terminal_values);
    return {std::move(materialized.paths), statistics,
            std::move(materialized.terminal_values)};
  } catch (const std::bad_alloc &) {
    throw input_error{"simulation result allocation failed"};
  } catch (const std::length_error &) {
    throw input_error{"simulation result allocation is not representable"};
  }
}

} // namespace fincore
