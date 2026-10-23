#pragma once

#include <fincore/portfolio/transactions.hpp>

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace fincore {

struct capacity_config {
    double max_bar_consumption = 0.2;
    double capital_base = 1'000'000.0;
    std::size_t mean_volume_window = 5U;
    std::optional<std::size_t> last_n_days;
};

struct capacity_assessment {
    dataframe liquidation_days;
    std::vector<ticker_liquidity> ticker_maximums;
    std::vector<transaction_liquidity> low_liquidity_transactions;
};

capacity_assessment assess_liquidity(
    const dataframe& positions,
    std::span<const trade_record> transactions,
    const dataframe& prices,
    const dataframe& volumes,
    const capacity_config& config = {});

}  // namespace fincore
