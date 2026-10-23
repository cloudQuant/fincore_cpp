#pragma once

#include <fincore/dataframe.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace fincore {

struct risk_parity_result {
    std::vector<double> weights;
    std::vector<double> risk_contributions;
    double volatility = 0.0;
    std::vector<std::string> asset_names;
};

risk_parity_result risk_parity(
    const dataframe& returns,
    std::optional<std::vector<double>> risk_budget = std::nullopt,
    std::size_t max_iter = 1000U);

}  // namespace fincore
