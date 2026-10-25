#pragma once

#include <fincore/metrics/returns.hpp>

#include <cstddef>
#include <string_view>

namespace fincore {

/// Annualizes a volatility-like value with the source's period factors.
///
/// Recognized periods are case-insensitive ``daily`` (252), ``weekly`` (52),
/// ``monthly`` (12) and ``yearly`` (1); any other value uses the daily factor.
/// The result is ``value * sqrt(factor)``.
///
/// @param value Volatility-like value to scale.
/// @param period Period label; defaults to ``daily``.
/// @return The scaled value.
double annualize(double value, std::string_view period = "daily");

/// Annualized drift and volatility estimated from periodic returns.
struct simulation_parameters {
    double drift;      ///< Annualized arithmetic mean return.
    double volatility; ///< Annualized sample standard deviation.
};

/// Estimates annualized drift and volatility from a return column.
///
/// NaN values are dropped and infinities are rejected; at least two retained
/// returns are required. Drift is the retained mean multiplied by
/// ``periods_per_year``; volatility is the sample standard deviation
/// (`n - 1`) multiplied by ``sqrt(periods_per_year)``. Timestamp order is not
/// used.
///
/// @param returns Frame containing the selected double return column.
/// @param periods_per_year Positive number of periods per year.
/// @param column Double return column name.
/// @return Annualized drift and sample volatility.
/// @throws input_error If the column is missing or not double,
///     ``periods_per_year`` is zero, fewer than two finite returns remain, a
///     value is infinite, or arithmetic overflows.
simulation_parameters estimate_simulation_parameters(
    const dataframe& returns,
    std::size_t periods_per_year = 252,
    std::string_view column = returns_column);

}  // namespace fincore
