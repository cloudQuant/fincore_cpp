#pragma once

#include <fincore/error.hpp>

#include <DataFrame/DataFrame.h>

#include <cstdint>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace fincore {

/// Signed timestamp in UTC nanoseconds since the Unix epoch.
using timestamp = std::int64_t;

/// Columnar frame indexed by `timestamp`, backed by `hmdf::StdDataFrame`.
using dataframe = hmdf::StdDataFrame<timestamp>;

/// Returns a single-column frame built from an index and double values.
///
/// The column is loaded without NaN padding, so the index and column sizes
/// remain equal to the input sizes.
///
/// @param index Timestamps for the rows, in the caller's order.
/// @param column_name Name of the double column; must not be empty.
/// @param values Values parallel to `index`.
/// @return Frame whose index is `index` and whose only column is
///     `column_name`.
/// @throws input_error If `column_name` is empty or the two input vectors have
///     different sizes.
dataframe make_series(std::vector<timestamp> index,
                      std::string_view column_name,
                      std::vector<double> values);

/// Returns a view of the named double column in `frame`.
///
/// @param frame Frame to inspect.
/// @param column_name Name of the required double column.
/// @return View over the column values; it references the frame's storage.
/// @throws input_error If `column_name` is empty, the column is absent, the
///     column is not of type double, or its size differs from the index size.
std::span<const double> require_double_column(const dataframe& frame,
                                              std::string_view column_name);

/// Returns two single-column frames restricted to their common timestamps.
///
/// The result is an inner join on the timestamp index. Timestamps are taken
/// from `left` in its existing order, and values are matched against `right` by
/// timestamp; timestamps present in only one input are dropped.
///
/// @param left Frame supplying the aligned timestamps and the left column.
/// @param left_column Double column to keep from `left`.
/// @param right Frame supplying the right-hand values.
/// @param right_column Double column to keep from `right`.
/// @return Pair of frames with identical indexes holding the left and right
///     values for the common timestamps.
/// @throws input_error If either column is missing, is not a double column, or
///     does not match its frame index length.
std::pair<dataframe, dataframe> align_on_common_index(
    const dataframe& left,
    std::string_view left_column,
    const dataframe& right,
    std::string_view right_column);

}  // namespace fincore
