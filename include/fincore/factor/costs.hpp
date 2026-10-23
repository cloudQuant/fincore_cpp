#pragma once

#include <fincore/dataframe.hpp>
#include <fincore/factor/performance.hpp>

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace fincore {

struct factor_cost_model {
    double half_spread_bps = 0.0;
    double impact_coefficient = 0.0;
    double max_participation = 1.0;
    double impact_exponent = 0.5;
};

struct factor_capacity_result {
    double maximum_portfolio_value = 0.0;
    std::vector<timestamp> dates;
    std::vector<double> maximum_portfolio_value_by_date;
    std::vector<std::string> assets;
    std::vector<double> trade_weights;  // dates x assets
    double max_participation = 1.0;
    timestamp binding_date = 0;
    std::string binding_asset;
};

struct factor_cost_result {
    factor_cost_model model;
    double portfolio_value = 0.0;
    std::vector<timestamp> dates;
    std::vector<std::string> assets;
    std::vector<double> gross_returns;
    std::vector<double> net_returns;
    std::vector<double> turnover;
    std::vector<double> spread_cost;
    std::vector<double> impact_cost;
    std::vector<double> borrow_cost;
    std::vector<double> total_cost;
    std::vector<double> trade_weights;
    std::vector<double> participation;
    factor_capacity_result capacity;
};

factor_capacity_result estimate_factor_capacity(
    const std::vector<factor_weight_row>& weights,
    const dataframe& dollar_volume, double max_participation);

factor_cost_result apply_factor_costs(
    std::span<const double> gross_returns,
    const std::vector<factor_weight_row>& weights,
    const dataframe& dollar_volume, double portfolio_value,
    const factor_cost_model& model,
    const dataframe* borrow_rates = nullptr,
    const dataframe* borrow_available = nullptr);

}  // namespace fincore
