#include <fincore/metrics/returns.hpp>

#include <cmath>
#include <limits>
#include <vector>

namespace fincore {
namespace {

double compound_returns(std::span<const double> returns) {
    double wealth = 1.0;
    for (const double value : returns) {
        wealth *= 1.0 + (std::isnan(value) ? 0.0 : value);
    }
    return wealth - 1.0;
}

}  // namespace

dataframe simple_returns(const dataframe& prices, std::string_view price_column) {
    const auto values = require_double_column(prices, price_column);
    const auto& index = prices.get_index();
    if (values.size() < 2) {
        return make_series({}, returns_column, {});
    }

    std::vector<timestamp> result_index(index.begin() + 1, index.end());
    std::vector<double> result_values;
    result_values.reserve(values.size() - 1);
    for (std::size_t position = 1; position < values.size(); ++position) {
        result_values.push_back(values[position] / values[position - 1] - 1.0);
    }
    return make_series(std::move(result_index), returns_column,
                       std::move(result_values));
}

dataframe cumulative_returns(const dataframe& returns, std::string_view column) {
    const auto values = require_double_column(returns, column);
    std::vector<double> result_values;
    result_values.reserve(values.size());

    double wealth = 1.0;
    for (const double value : values) {
        wealth *= 1.0 + (std::isnan(value) ? 0.0 : value);
        result_values.push_back(wealth - 1.0);
    }
    return make_series(returns.get_index(), returns_column,
                       std::move(result_values));
}

double cumulative_return(const dataframe& returns, std::string_view column) {
    const auto values = require_double_column(returns, column);
    if (values.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return compound_returns(values);
}

double annual_return(const dataframe& returns,
                     double annualization,
                     std::string_view column) {
    const auto values = require_double_column(returns, column);
    if (values.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    const double ending_wealth = compound_returns(values) + 1.0;
    if (ending_wealth <= 0.0) {
        return -1.0;
    }
    return std::pow(ending_wealth,
                    annualization / static_cast<double>(values.size())) -
           1.0;
}

double annual_volatility(const dataframe& returns,
                         double annualization,
                         std::string_view column) {
    const auto values = require_double_column(returns, column);
    std::vector<double> valid_values;
    valid_values.reserve(values.size());
    for (const double value : values) {
        if (!std::isnan(value)) {
            valid_values.push_back(value);
        }
    }
    if (valid_values.size() < 2) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    double mean = 0.0;
    for (const double value : valid_values) {
        mean += value;
    }
    mean /= static_cast<double>(valid_values.size());

    double squared_deviations = 0.0;
    for (const double value : valid_values) {
        const double deviation = value - mean;
        squared_deviations += deviation * deviation;
    }
    const double sample_variance =
        squared_deviations / static_cast<double>(valid_values.size() - 1);
    return std::sqrt(sample_variance) * std::sqrt(annualization);
}

}  // namespace fincore
