#pragma once

#include <fincore/metrics/returns.hpp>

#include <span>
#include <string_view>
#include <vector>

namespace fincore {

/// Validates and copies a return series.
///
/// The source rejects empty input and NaN values; the native contract also
/// rejects infinities so every returned value is finite. Contract failures
/// raise ``fincore_error`` with a structured validation category/code.
///
/// @param returns Return values to validate.
/// @return A validated copy of ``returns``.
/// @throws fincore_error If ``returns`` is empty, contains NaN, or contains
///     infinity.
[[nodiscard]] std::vector<double> validate_returns(
    std::span<const double> returns);

/// Validates and copies the named return column of a frame.
///
/// @param returns Frame containing the selected double column.
/// @param column Name of the double return column; must not be empty.
/// @return A validated copy of the selected column.
/// @throws fincore_error If ``column`` is empty or missing, the column is not
///     a double column, or the selected values are empty, NaN or infinite.
[[nodiscard]] std::vector<double> validate_returns(
    const dataframe& returns, std::string_view column = returns_column);

}  // namespace fincore
