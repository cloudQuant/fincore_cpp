#pragma once

#include <fincore/metrics/returns.hpp>

#include <span>
#include <string_view>
#include <vector>

namespace fincore {

/// Validate and copy a return series.
///
/// The source rejects empty input and NaN values; the native contract also
/// rejects infinities so every returned value is finite. Contract failures
/// raise :class:`fincore_error` with a structured validation category/code.
[[nodiscard]] std::vector<double> validate_returns(
    std::span<const double> returns);

/// Validate and copy the named return column of a frame.
[[nodiscard]] std::vector<double> validate_returns(
    const dataframe& returns, std::string_view column = returns_column);

}  // namespace fincore
