#pragma once

#include <fincore/metrics/drawdown.hpp>

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fincore {

/// Name of the scenario-name column in a scenario table.
inline constexpr std::string_view stress_scenario_name_column{"scenario"};
/// Name of the cumulative-return column in a scenario table.
inline constexpr std::string_view stress_cumulative_return_column{
    "cumulative_return"};
/// Name of the maximum-drawdown column in a scenario table.
inline constexpr std::string_view stress_max_drawdown_column{"max_drawdown"};
/// Name of the volatility column in a scenario table.
inline constexpr std::string_view stress_volatility_column{"volatility"};
/// Name of the final-value column in a scenario table.
inline constexpr std::string_view stress_final_value_column{"final_value"};

/// Predefined stress transformations.
enum class predefined_stress_scenario {
    crash,            ///< Sets the middle return to -0.20 (`crash`).
    spike,            ///< Sets the middle return to 0.10 (`spike`).
    volatility_crush, ///< Halves every return (`vol_crush`).
    volatility_spike, ///< Doubles every return (`vol_spike`).
};

/// User-defined stress transformation.
///
/// Each stressed return is ``(return + return_shift) * return_multiplier``.
/// Names must be non-empty, distinct and must not reuse a predefined name.
struct custom_stress_scenario {
    std::string name;                ///< Scenario name shown in results.
    double return_shift{0.0};        ///< Additive shift applied first.
    double return_multiplier{1.0};   ///< Multiplier applied after the shift.
};

/// Outcome of one stress scenario.
struct stress_scenario_result {
    std::string name;           ///< Scenario name.
    /// Stressed returns on the input index and column.
    dataframe stressed_returns;
    double cumulative_return;   ///< Compounded stressed return.
    double max_drawdown;        ///< Deepest drawdown as a negative fraction.
    /// Annualized volatility of the stressed returns.
    double volatility;
    double final_value;         ///< ``1 + cumulative_return``.
};

/// Runs the four predefined stress scenarios in source order.
///
/// Applies ``crash``, ``spike``, ``vol_crush`` and ``vol_spike`` in that
/// order.
///
/// @param returns Frame containing the return column.
/// @param column Double return column to stress.
/// @return One result per predefined scenario, in source order.
/// @throws input_error If the column is missing, not double, empty or contains
///     NaN.
std::vector<stress_scenario_result> stress_test(
    const dataframe& returns,
    std::string_view column = returns_column);

/// Runs selected predefined and custom stress scenarios.
///
/// Predefined results appear first in caller order, followed by custom results
/// in caller order. NaN returns are rejected, while infinite returns are
/// retained.
///
/// @param returns Frame containing the return column.
/// @param scenarios Predefined scenarios to apply; duplicates are rejected.
/// @param custom_scenarios Custom scenarios to apply; names must be non-empty,
///     distinct and not one of ``crash``, ``spike``, ``vol_crush`` or
///     ``vol_spike``.
/// @param column Double return column to stress.
/// @return One result per requested scenario, in the order described above.
/// @throws input_error If return validation fails, a predefined scenario is
///     unknown or duplicated, or a custom scenario has an empty, reserved or
///     duplicate name or non-finite parameters.
std::vector<stress_scenario_result> stress_test(
    const dataframe& returns,
    std::span<const predefined_stress_scenario> scenarios,
    std::span<const custom_stress_scenario> custom_scenarios = {},
    std::string_view column = returns_column);

/// Builds a tabular summary of stress scenario results.
///
/// The table has an ordinal index and one row per result, with the columns
/// ``scenario``, ``cumulative_return``, ``max_drawdown``, ``volatility`` and
/// ``final_value``. Empty input yields a typed empty table.
///
/// @param results Scenario results to tabulate.
/// @return A frame with one row per result.
dataframe scenario_table(std::span<const stress_scenario_result> results);

}  // namespace fincore
