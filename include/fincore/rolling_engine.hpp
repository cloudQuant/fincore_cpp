#pragma once

#include <fincore/metrics/returns.hpp>

#include <map>
#include <optional>
#include <span>
#include <cstddef>
#include <string>

namespace fincore {

enum class rolling_metric { mean_return, volatility, sharpe, max_drawdown, beta };

class rolling_engine {
public:
    rolling_engine(dataframe returns,
                   std::optional<dataframe> benchmark = std::nullopt,
                   std::size_t window = static_cast<std::size_t>(daily_annualization),
                   double annualization = daily_annualization);

    [[nodiscard]] std::map<std::string, dataframe> compute(
        std::span<const rolling_metric> metrics) const;

private:
    dataframe returns_;
    std::optional<dataframe> benchmark_;
    std::size_t window_;
    double annualization_;
};

}  // namespace fincore
