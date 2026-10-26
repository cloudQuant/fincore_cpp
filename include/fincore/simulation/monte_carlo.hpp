#pragma once

#include <fincore/simulation/paths.hpp>
#include <fincore/simulation/result.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace fincore {

/// Simulates a calibrated Monte Carlo path set from historical returns.
///
/// Annualized drift and volatility are estimated from the selected return
/// column (NaN values dropped), geometric Brownian motion is simulated from
/// 1.0 for ``horizon`` steps at 252 periods per year, and the resulting price
/// paths are converted to cumulative returns. When ``antithetic`` is true,
/// ``antithetic_variates`` is applied to those paths, so the stored path count
/// is doubled.
///
/// @param returns Frame containing the return column.
/// @param path_count Number of base paths.
/// @param horizon Number of simulated periods.
/// @param antithetic Whether to append antithetic mirror paths.
/// @param seed Optional deterministic seed; omitted seeds are not
///     reproducible.
/// @param column Double return column used for calibration.
/// @return A simulation result owning the path frame and terminal statistics.
/// @throws input_error If calibration or path generation rejects the input.
[[nodiscard]] simulation_result simulate_monte_carlo(
    const dataframe& returns,
    std::size_t path_count = 1000U,
    std::size_t horizon = 252U,
    bool antithetic = false,
    std::optional<std::uint64_t> seed = std::nullopt,
    std::string_view column = returns_column);

/// Simulates price paths directly from annualized parameters.
///
/// Annualized drift and volatility are converted to per-step values
/// (``drift / 252`` and ``volatility / sqrt(252)``), and ``horizon`` periods
/// are simulated with a ``1 / 252`` year step. The returned paths start at
/// ``initial_value`` and include the initial row.
///
/// @param annualized_drift Annualized drift of the price process.
/// @param annualized_volatility Annualized volatility of the price process.
/// @param initial_value Starting price of every path.
/// @param path_count Number of paths.
/// @param horizon Number of simulated periods.
/// @param seed Optional deterministic seed; omitted seeds are not
///     reproducible.
/// @return A simulation result owning the price paths and terminal statistics.
/// @throws input_error If an input is non-finite, ``path_count`` is zero, or
///     the requested index, names or output shape are unrepresentable.
[[nodiscard]] simulation_result simulate_monte_carlo_from_parameters(
    double annualized_drift,
    double annualized_volatility,
    double initial_value = 1.0,
    std::size_t path_count = 1000U,
    std::size_t horizon = 252U,
    std::optional<std::uint64_t> seed = std::nullopt);

}  // namespace fincore
