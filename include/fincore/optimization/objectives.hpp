#pragma once

#include <fincore/dataframe.hpp>

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace fincore {

enum class portfolio_objective {
    max_sharpe,
    min_variance,
    target_return,
    target_risk
};

struct sector_allocation_constraint {
    std::string sector;
    double lower;
    double upper;
};

struct portfolio_optimization_config {
    portfolio_objective objective = portfolio_objective::max_sharpe;
    double risk_free_rate = 0.0;
    std::optional<double> target_return;
    std::optional<double> target_volatility;
    bool short_allowed = false;
    double max_weight = 1.0;
    std::optional<double> min_weight;
    std::vector<sector_allocation_constraint> sector_constraints;
    std::map<std::string, std::string> sector_map;
};

struct portfolio_optimization_result {
    std::vector<double> weights;
    double expected_return = 0.0;
    double volatility = 0.0;
    double sharpe = 0.0;
    std::vector<std::string> asset_names;
    portfolio_objective objective = portfolio_objective::max_sharpe;
};

portfolio_optimization_result optimize_portfolio(
    const dataframe& returns,
    const portfolio_optimization_config& config = {});

}  // namespace fincore
