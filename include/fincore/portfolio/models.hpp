#pragma once

#include <fincore/dataframe.hpp>

#include <map>
#include <optional>
#include <string>
#include <utility>

namespace fincore {

// Detached optional input tables for portfolio-domain workflows. Each value
// is owned by this object; materialize() returns another independent copy.
// Only supplied fields are returned, under returns, positions, transactions,
// and benchmark_returns respectively. A supplied empty DataFrame is retained.
class portfolio_inputs {
  public:
    portfolio_inputs(std::optional<dataframe> returns = std::nullopt,
                     std::optional<dataframe> positions = std::nullopt,
                     std::optional<dataframe> transactions = std::nullopt,
                     std::optional<dataframe> benchmark_returns = std::nullopt)
        : returns_(std::move(returns)),
          positions_(std::move(positions)),
          transactions_(std::move(transactions)),
          benchmark_returns_(std::move(benchmark_returns)) {}

    [[nodiscard]] std::map<std::string, dataframe> materialize() const {
        std::map<std::string, dataframe> result;
        if (returns_) {
            result.emplace("returns", *returns_);
        }
        if (positions_) {
            result.emplace("positions", *positions_);
        }
        if (transactions_) {
            result.emplace("transactions", *transactions_);
        }
        if (benchmark_returns_) {
            result.emplace("benchmark_returns", *benchmark_returns_);
        }
        return result;
    }

  private:
    std::optional<dataframe> returns_;
    std::optional<dataframe> positions_;
    std::optional<dataframe> transactions_;
    std::optional<dataframe> benchmark_returns_;
};

}  // namespace fincore
