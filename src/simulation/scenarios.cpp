#include <fincore/simulation/scenarios.hpp>

#include <cmath>
#include <cstddef>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace fincore {
namespace {

struct predefined_definition {
    std::string_view name;
    enum class transformation {
        crash,
        spike,
        volatility_crush,
        volatility_spike,
    } kind;
};

const predefined_definition& predefined_definition_for(
    const predefined_stress_scenario scenario) {
    static constexpr predefined_definition crash{"crash",
                                                  predefined_definition::transformation::crash};
    static constexpr predefined_definition spike{"spike",
                                                  predefined_definition::transformation::spike};
    static constexpr predefined_definition volatility_crush{
        "vol_crush", predefined_definition::transformation::volatility_crush};
    static constexpr predefined_definition volatility_spike{
        "vol_spike", predefined_definition::transformation::volatility_spike};

    switch (scenario) {
    case predefined_stress_scenario::crash:
        return crash;
    case predefined_stress_scenario::spike:
        return spike;
    case predefined_stress_scenario::volatility_crush:
        return volatility_crush;
    case predefined_stress_scenario::volatility_spike:
        return volatility_spike;
    }
    throw input_error{"unknown predefined stress scenario"};
}

void validate_returns(const dataframe& returns, const std::string_view column) {
    const auto values = require_double_column(returns, column);
    if (values.empty()) {
        throw input_error{"stress testing requires at least one return"};
    }
    for (const double value : values) {
        if (std::isnan(value)) {
            throw input_error{"stress testing does not accept NaN returns"};
        }
    }
}

void validate_scenarios(
    const std::span<const predefined_stress_scenario> scenarios,
    const std::span<const custom_stress_scenario> custom_scenarios) {
    std::set<std::string> names;
    for (const auto scenario : scenarios) {
        const auto& definition = predefined_definition_for(scenario);
        if (!names.emplace(definition.name).second) {
            throw input_error{"duplicate predefined stress scenario"};
        }
    }

    for (const auto& scenario : custom_scenarios) {
        if (scenario.name.empty()) {
            throw input_error{"custom stress scenario name must not be empty"};
        }
        if (!std::isfinite(scenario.return_shift) ||
            !std::isfinite(scenario.return_multiplier)) {
            throw input_error{"custom stress scenario parameters must be finite"};
        }
        if (scenario.name == "crash" || scenario.name == "spike" ||
            scenario.name == "vol_crush" || scenario.name == "vol_spike") {
            throw input_error{"custom stress scenario name is reserved"};
        }
        if (!names.emplace(scenario.name).second) {
            throw input_error{"duplicate or reserved stress scenario name"};
        }
    }
}

std::vector<double> transform_predefined(
    const std::span<const double> returns,
    const predefined_definition::transformation transformation) {
    std::vector<double> values{returns.begin(), returns.end()};
    switch (transformation) {
    case predefined_definition::transformation::crash:
        values[values.size() / 2U] = -0.20;
        break;
    case predefined_definition::transformation::spike:
        values[values.size() / 2U] = 0.10;
        break;
    case predefined_definition::transformation::volatility_crush:
        for (double& value : values) {
            value *= 0.5;
        }
        break;
    case predefined_definition::transformation::volatility_spike:
        for (double& value : values) {
            value *= 2.0;
        }
        break;
    }
    return values;
}

std::vector<double> transform_custom(
    const std::span<const double> returns,
    const custom_stress_scenario& scenario) {
    std::vector<double> values;
    values.reserve(returns.size());
    for (const double value : returns) {
        values.push_back((value + scenario.return_shift) *
                         scenario.return_multiplier);
    }
    return values;
}

stress_scenario_result make_result(const std::string_view name,
                                   const dataframe& returns,
                                   const std::string_view column,
                                   std::vector<double> values) {
    auto stressed_returns =
        make_series(returns.get_index(), column, std::move(values));
    const double cumulative = cumulative_return(stressed_returns, column);
    const double drawdown = max_drawdown(stressed_returns, column);
    const double volatility =
        annual_volatility(stressed_returns, daily_annualization, column);
    return {std::string{name}, std::move(stressed_returns), cumulative, drawdown,
            volatility, 1.0 + cumulative};
}

}  // namespace

std::vector<stress_scenario_result> stress_test(const dataframe& returns,
                                                const std::string_view column) {
    static constexpr predefined_stress_scenario default_scenarios[]{
        predefined_stress_scenario::crash,
        predefined_stress_scenario::spike,
        predefined_stress_scenario::volatility_crush,
        predefined_stress_scenario::volatility_spike,
    };
    return stress_test(returns, default_scenarios, {}, column);
}

std::vector<stress_scenario_result> stress_test(
    const dataframe& returns,
    const std::span<const predefined_stress_scenario> scenarios,
    const std::span<const custom_stress_scenario> custom_scenarios,
    const std::string_view column) {
    validate_returns(returns, column);
    validate_scenarios(scenarios, custom_scenarios);

    const auto values = require_double_column(returns, column);
    std::vector<stress_scenario_result> results;
    results.reserve(scenarios.size() + custom_scenarios.size());
    for (const auto scenario : scenarios) {
        const auto& definition = predefined_definition_for(scenario);
        results.push_back(make_result(
            definition.name, returns, column,
            transform_predefined(values, definition.kind)));
    }
    for (const auto& scenario : custom_scenarios) {
        results.push_back(make_result(scenario.name, returns, column,
                                      transform_custom(values, scenario)));
    }
    return results;
}

dataframe scenario_table(const std::span<const stress_scenario_result> results) {
    std::vector<timestamp> index;
    std::vector<std::string> names;
    std::vector<double> cumulative_returns;
    std::vector<double> maximum_drawdowns;
    std::vector<double> volatilities;
    std::vector<double> final_values;
    index.reserve(results.size());
    names.reserve(results.size());
    cumulative_returns.reserve(results.size());
    maximum_drawdowns.reserve(results.size());
    volatilities.reserve(results.size());
    final_values.reserve(results.size());

    for (std::size_t position = 0; position < results.size(); ++position) {
        const auto& result = results[position];
        index.push_back(static_cast<timestamp>(position));
        names.push_back(result.name);
        cumulative_returns.push_back(result.cumulative_return);
        maximum_drawdowns.push_back(result.max_drawdown);
        volatilities.push_back(result.volatility);
        final_values.push_back(result.final_value);
    }

    dataframe table;
    table.load_index(std::move(index));
    table.load_column<std::string>(stress_scenario_name_column.data(),
                                   std::move(names),
                                   hmdf::nan_policy::dont_pad_with_nans);
    table.load_column<double>(stress_cumulative_return_column.data(),
                              std::move(cumulative_returns),
                              hmdf::nan_policy::dont_pad_with_nans);
    table.load_column<double>(stress_max_drawdown_column.data(),
                              std::move(maximum_drawdowns),
                              hmdf::nan_policy::dont_pad_with_nans);
    table.load_column<double>(stress_volatility_column.data(),
                              std::move(volatilities),
                              hmdf::nan_policy::dont_pad_with_nans);
    table.load_column<double>(stress_final_value_column.data(),
                              std::move(final_values),
                              hmdf::nan_policy::dont_pad_with_nans);
    return table;
}

}  // namespace fincore
