#include <fincore/metrics/helpers.hpp>

#include <fincore/error.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fincore {
namespace {

// The NaT sentinel: the smallest representable timestamp stands for a missing
// observation, so both index validators reject it instead of letting it sort to
// the front of an index as if it were the earliest date.
constexpr timestamp nat_sentinel = std::numeric_limits<timestamp>::min();
constexpr std::int64_t nanoseconds_per_day = 86'400'000'000'000LL;

void require_nonempty_name(const std::string_view name,
                           const char* const message) {
    if (name.empty()) {
        throw input_error{message};
    }
    if (name.find('\0') != std::string_view::npos) {
        throw input_error{message};
    }
}

std::vector<timestamp> positional_index(const std::size_t count) {
    if (count > static_cast<std::size_t>(
                    std::numeric_limits<timestamp>::max())) {
        throw input_error{"dataframe positional index is not representable"};
    }
    if (count > std::vector<timestamp>{}.max_size()) {
        throw input_error{"dataframe positional index allocation is too large"};
    }

    std::vector<timestamp> index;
    index.reserve(count);
    for (std::size_t position = 0U; position < count; ++position) {
        index.push_back(static_cast<timestamp>(position));
    }
    return index;
}

timestamp timestamp_from_day_count(const std::int64_t day_count) {
    if (day_count > std::numeric_limits<timestamp>::max() /
                        nanoseconds_per_day ||
        day_count < std::numeric_limits<timestamp>::min() /
                        nanoseconds_per_day) {
        throw input_error{"synthetic datetime index is not representable"};
    }
    return static_cast<timestamp>(day_count) * nanoseconds_per_day;
}

timestamp timestamp_from_sys_days(const std::chrono::sys_days day) {
    return timestamp_from_day_count(
        static_cast<std::int64_t>(day.time_since_epoch().count()));
}

std::int64_t synthetic_year(const std::size_t period_index,
                            const std::size_t periods_per_year) {
    const std::size_t year_offset = period_index / periods_per_year;
    constexpr std::int64_t maximum_year = 32767;
    if (year_offset > static_cast<std::size_t>(maximum_year - 1970)) {
        throw input_error{
            "synthetic datetime index year is outside the supported range"};
    }
    return 1970 + static_cast<std::int64_t>(year_offset);
}

timestamp month_end_timestamp(const std::int64_t year_value,
                              const unsigned month_value) {
    if (year_value < static_cast<std::int64_t>(
                         static_cast<int>(std::chrono::year::min())) ||
        year_value > static_cast<std::int64_t>(
                         static_cast<int>(std::chrono::year::max()))) {
        throw input_error{
            "synthetic datetime index year is outside the supported range"};
    }

    const std::chrono::year_month_day_last last_day{
        std::chrono::year{static_cast<int>(year_value)},
        std::chrono::month_day_last{std::chrono::month{month_value}}};
    return timestamp_from_sys_days(std::chrono::sys_days{last_day});
}

// Uniqueness is judged on a sorted copy, so a frame stored out of order still
// passes here and keeps its own ordering; the strict variant below is what a
// caller uses when the stored order itself has to rise.
void validate_unique_datetime_index(const std::vector<timestamp>& index,
                                    const char* const context) {
    std::vector<timestamp> sorted{index.begin(), index.end()};
    for (const timestamp value : sorted) {
        if (value == nat_sentinel) {
            throw input_error{context};
        }
    }
    std::sort(sorted.begin(), sorted.end());
    const auto duplicate =
        std::adjacent_find(sorted.begin(), sorted.end());
    if (duplicate != sorted.end()) {
        throw input_error{context};
    }
}

void validate_strict_datetime_index(const std::vector<timestamp>& index,
                                    const char* const context) {
    for (std::size_t position = 0U; position < index.size(); ++position) {
        if (index[position] == nat_sentinel) {
            throw input_error{context};
        }
        if (position != 0U && index[position] <= index[position - 1U]) {
            throw input_error{context};
        }
    }
}

void validate_synthetic_frequency(const frequency period) {
    switch (period) {
    case frequency::daily:
    case frequency::weekly:
    case frequency::monthly:
    case frequency::quarterly:
    case frequency::yearly:
        return;
    }
    throw input_error{"unrecognized frequency"};
}

std::vector<timestamp> synthetic_datetime_index(
    const frequency period,
    const std::size_t count) {
    validate_synthetic_frequency(period);
    if (count > std::vector<timestamp>{}.max_size()) {
        throw input_error{"synthetic datetime index allocation is too large"};
    }

    std::vector<timestamp> index;
    index.reserve(count);
    constexpr timestamp epoch_day_count = 0;
    constexpr std::int64_t maximum_day_count =
        std::numeric_limits<timestamp>::max() / nanoseconds_per_day;

    switch (period) {
    case frequency::daily:
        for (std::size_t position = 0U; position < count; ++position) {
            if (position > static_cast<std::size_t>(maximum_day_count)) {
                throw input_error{
                    "synthetic datetime index is not representable"};
            }
            index.push_back(timestamp_from_day_count(
                epoch_day_count + static_cast<std::int64_t>(position)));
        }
        return index;
    case frequency::weekly: {
        // 1970-01-01 was a Thursday; the first W-SUN period end is
        // 1970-01-04, matching pd.date_range(start, freq="W").
        constexpr std::int64_t first_sunday_day_count = 3;
        for (std::size_t position = 0U; position < count; ++position) {
            if (position > static_cast<std::size_t>(
                               (maximum_day_count - first_sunday_day_count) /
                               7)) {
                throw input_error{
                    "synthetic datetime index is not representable"};
            }
            index.push_back(timestamp_from_day_count(
                first_sunday_day_count +
                static_cast<std::int64_t>(position) * 7));
        }
        return index;
    }
    // Calendar frequencies are labels of month ends, and the year advances only
    // every twelve, four or one positions, so the series always opens in
    // January 1970 rather than at a date the caller supplies.
    case frequency::monthly:
        for (std::size_t position = 0U; position < count; ++position) {
            const std::int64_t year_value = synthetic_year(position, 12U);
            const auto month_value =
                static_cast<unsigned>(position % 12U) + 1U;
            index.push_back(month_end_timestamp(year_value, month_value));
        }
        return index;
    case frequency::quarterly:
        for (std::size_t position = 0U; position < count; ++position) {
            const std::int64_t year_value = synthetic_year(position, 4U);
            const auto month_value = static_cast<unsigned>(position % 4U) * 3U + 3U;
            index.push_back(month_end_timestamp(year_value, month_value));
        }
        return index;
    case frequency::yearly:
        for (std::size_t position = 0U; position < count; ++position) {
            const std::int64_t year_value = synthetic_year(position, 1U);
            index.push_back(month_end_timestamp(year_value, 12U));
        }
        return index;
    }

    throw input_error{"unrecognized frequency"};
}

struct frame_columns {
    std::vector<std::string> names;
    std::vector<std::span<const double>> values;
};

// The values are spans into the frame's own storage, so the returned struct is
// only valid while the frame it came from lives on; the length check against the
// index happens here so the callers can index columns without re-checking.
frame_columns double_columns_or_throw(const dataframe& frame,
                                      const char* const message) {
    const auto shape = frame.shape();
    const auto info = frame.get_columns_info<double>();
    if (info.size() != shape.second) {
        throw input_error{message};
    }

    frame_columns columns;
    columns.names.reserve(info.size());
    columns.values.reserve(info.size());
    const auto& index = frame.get_index();
    for (const auto& entry : info) {
        const std::string name{std::get<0>(entry).c_str()};
        const auto& values = frame.get_column<double>(name.c_str());
        if (values.size() != index.size()) {
            throw input_error{message};
        }
        columns.values.emplace_back(values.data(), values.size());
        columns.names.push_back(name);
    }
    return columns;
}

std::pair<dataframe, dataframe> align_frames(const dataframe& left,
                                             const dataframe& right) {
    validate_unique_datetime_index(
        left.get_index(),
        "aligned_series requires unique left-frame timestamps");
    validate_unique_datetime_index(
        right.get_index(),
        "aligned_series requires unique right-frame timestamps");

    const auto left_columns = double_columns_or_throw(
        left, "aligned_series supports only equally sized double columns");
    const auto right_columns = double_columns_or_throw(
        right, "aligned_series supports only equally sized double columns");

    std::unordered_map<timestamp, std::size_t> right_rows;
    right_rows.reserve(right.get_index().size());
    for (std::size_t position = 0U; position < right.get_index().size();
         ++position) {
        right_rows.emplace(right.get_index()[position], position);
    }

    std::vector<timestamp> common_index;
    std::vector<std::vector<double>> left_values(left_columns.names.size());
    std::vector<std::vector<double>> right_values(right_columns.names.size());
    common_index.reserve(left.get_index().size());
    for (auto& column : left_values) {
        column.reserve(left.get_index().size());
    }
    for (auto& column : right_values) {
        column.reserve(left.get_index().size());
    }

    for (std::size_t position = 0U; position < left.get_index().size();
         ++position) {
        const auto match = right_rows.find(left.get_index()[position]);
        if (match == right_rows.end()) {
            continue;
        }
        common_index.push_back(left.get_index()[position]);
        for (std::size_t column = 0U; column < left_columns.values.size();
             ++column) {
            left_values[column].push_back(left_columns.values[column][position]);
        }
        for (std::size_t column = 0U; column < right_columns.values.size();
             ++column) {
            right_values[column].push_back(
                right_columns.values[column][match->second]);
        }
    }

    dataframe aligned_left;
    aligned_left.load_index(std::vector<timestamp>{common_index});
    for (std::size_t column = 0U; column < left_columns.names.size(); ++column) {
        aligned_left.load_column<double>(
            left_columns.names[column].data(), std::move(left_values[column]),
            hmdf::nan_policy::dont_pad_with_nans);
    }

    dataframe aligned_right;
    aligned_right.load_index(std::move(common_index));
    for (std::size_t column = 0U; column < right_columns.names.size();
         ++column) {
        aligned_right.load_column<double>(
            right_columns.names[column].data(), std::move(right_values[column]),
            hmdf::nan_policy::dont_pad_with_nans);
    }

    return {std::move(aligned_left), std::move(aligned_right)};
}

}  // namespace

matrix::matrix(const std::size_t row_count,
               const std::size_t column_count,
               std::vector<double> values)
    : row_count_{row_count},
      column_count_{column_count},
      values_{std::move(values)} {
    if (column_count_ != 0U &&
        row_count_ > std::vector<double>{}.max_size() / column_count_) {
        throw input_error{"matrix shape is not representable"};
    }
    if (row_count_ * column_count_ != values_.size()) {
        throw input_error{"matrix values must match its row-major shape"};
    }
}

std::size_t matrix::rows() const noexcept { return row_count_; }

std::size_t matrix::columns() const noexcept { return column_count_; }

std::span<const double> matrix::values() const noexcept {
    return {values_.data(), values_.size()};
}

std::span<const double> matrix::row(const std::size_t row_index) const {
    if (row_index >= row_count_) {
        throw input_error{"matrix row index is out of range"};
    }
    return std::span<const double>{values_}.subspan(
        row_index * column_count_, column_count_);
}

double matrix::at(const std::size_t row_index,
                  const std::size_t column_index) const {
    if (row_index >= row_count_ || column_index >= column_count_) {
        throw input_error{"matrix index is out of range"};
    }
    return values_[row_index * column_count_ + column_index];
}

double& matrix::at(const std::size_t row_index,
                   const std::size_t column_index) {
    if (row_index >= row_count_ || column_index >= column_count_) {
        throw input_error{"matrix index is out of range"};
    }
    return values_[row_index * column_count_ + column_index];
}

std::pair<dataframe, dataframe> aligned_series(
    const dataframe& left,
    const std::string_view left_column,
    const dataframe& right,
    const std::string_view right_column) {
    validate_unique_datetime_index(
        left.get_index(),
        "aligned_series requires unique left-frame timestamps");
    validate_unique_datetime_index(
        right.get_index(),
        "aligned_series requires unique right-frame timestamps");
    return align_on_common_index(left, left_column, right, right_column);
}

std::pair<dataframe, dataframe> aligned_series(const dataframe& left,
                                               const dataframe& right) {
    return align_frames(left, right);
}

dataframe ensure_datetime_index_series(const dataframe& frame,
                                       const frequency period) {
    const auto& index = frame.get_index();
    if (!index.empty()) {
        validate_strict_datetime_index(
            index,
            "datetime index must be strictly increasing, unique and not NaT");
        return frame;
    }

    const auto shape = frame.shape();
    if (shape.second == 0U) {
        return frame;
    }
    if (shape.second != 1U) {
        throw input_error{
            "synthetic datetime index requires a single double column"};
    }

    const auto info = frame.get_columns_info<double>();
    if (info.size() != 1U) {
        throw input_error{
            "synthetic datetime index requires a single double column"};
    }
    const std::string_view name{std::get<0>(info.front())};
    const auto& values = frame.get_column<double>(name.data());
    if (values.size() != std::get<1>(info.front())) {
        throw input_error{"frame column length is inconsistent"};
    }

    return make_series(synthetic_datetime_index(period, values.size()), name,
                       std::vector<double>{values.begin(), values.end()});
}

dataframe ensure_datetime_index_series(const std::span<const double> values,
                                       const frequency period,
                                       const std::string_view column) {
    require_nonempty_name(column, "datetime index column name must not be empty");
    if (values.empty()) {
        return make_series({}, column, {});
    }
    return make_series(synthetic_datetime_index(period, values.size()), column,
                       std::vector<double>{values.begin(), values.end()});
}

// Row-major: the outer loop walks rows, so a frame flattens interleaved by
// column rather than as one concatenated column per block, which is the layout
// a matrix already uses and lets the two overloads agree on the same shape.
std::vector<double> flatten(const dataframe& frame) {
    const auto shape = frame.shape();
    const auto columns = double_columns_or_throw(
        frame, "flatten supports only equally sized double columns");

    if (shape.first != 0U &&
        shape.second > std::vector<double>{}.max_size() / shape.first) {
        throw input_error{"flattened shape is not representable"};
    }

    std::vector<double> flattened;
    flattened.reserve(shape.first * shape.second);
    for (std::size_t row = 0U; row < shape.first; ++row) {
        for (const auto& column : columns.values) {
            flattened.push_back(column[row]);
        }
    }
    return flattened;
}

std::vector<double> flatten(const matrix& values) {
    return {values.values().begin(), values.values().end()};
}

dataframe to_dataframe(const std::span<const double> values,
                       const std::string_view column) {
    require_nonempty_name(column, "dataframe column name must not be empty");
    return make_series(positional_index(values.size()), column,
                       std::vector<double>{values.begin(), values.end()});
}

dataframe to_dataframe(const matrix& values,
                       const std::string_view column_prefix) {
    require_nonempty_name(column_prefix,
                          "dataframe column prefix must not be empty");

    dataframe result;
    result.load_index(positional_index(values.rows()));
    for (std::size_t column = 0U; column < values.columns(); ++column) {
        std::string name{column_prefix};
        name += '_';
        name += std::to_string(column);

        std::vector<double> column_values;
        column_values.reserve(values.rows());
        for (std::size_t row = 0U; row < values.rows(); ++row) {
            column_values.push_back(values.at(row, column));
        }
        result.load_column<double>(name.c_str(), std::move(column_values),
                                   hmdf::nan_policy::dont_pad_with_nans);
    }
    return result;
}

dataframe to_dataframe(const dataframe& frame) { return frame; }

}  // namespace fincore
