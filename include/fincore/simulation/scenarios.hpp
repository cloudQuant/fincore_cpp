#pragma once

#include <fincore/metrics/drawdown.hpp>

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fincore {

inline constexpr std::string_view stress_scenario_name_column{"scenario"};
inline constexpr std::string_view stress_cumulative_return_column{
    "cumulative_return"};
inline constexpr std::string_view stress_max_drawdown_column{"max_drawdown"};
inline constexpr std::string_view stress_volatility_column{"volatility"};
inline constexpr std::string_view stress_final_value_column{"final_value"};

enum class predefined_stress_scenario {
    crash,
    spike,
    volatility_crush,
    volatility_spike,
};

struct custom_stress_scenario {
    std::string name;
    double return_shift{0.0};
    double return_multiplier{1.0};
};

struct stress_scenario_result {
    std::string name;
    dataframe stressed_returns;
    double cumulative_return;
    double max_drawdown;
    double volatility;
    double final_value;
};

std::vector<stress_scenario_result> stress_test(
    const dataframe& returns,
    std::string_view column = returns_column);

std::vector<stress_scenario_result> stress_test(
    const dataframe& returns,
    std::span<const predefined_stress_scenario> scenarios,
    std::span<const custom_stress_scenario> custom_scenarios = {},
    std::string_view column = returns_column);

dataframe scenario_table(std::span<const stress_scenario_result> results);

}  // namespace fincore
