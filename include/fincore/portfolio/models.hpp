#pragma once

#include <fincore/dataframe.hpp>

#include <map>
#include <optional>
#include <string>
#include <utility>

namespace fincore {

/// Detached optional input tables for portfolio-domain workflows.
///
/// Each supplied frame is owned by this object; materialize() returns another
/// independent copy. Only supplied fields are returned, under ``returns``,
/// ``positions``, ``transactions`` and ``benchmark_returns`` respectively. A
/// supplied empty frame is retained.
class portfolio_inputs {
  public:
    /// Constructs a container from optional input frames.
    ///
    /// @param returns Optional return frame stored under ``returns``.
    /// @param positions Optional position frame stored under ``positions``.
    /// @param transactions Optional transaction frame stored under
    ///     ``transactions``.
    /// @param benchmark_returns Optional benchmark frame stored under
    ///     ``benchmark_returns``.
    portfolio_inputs(std::optional<dataframe> returns = std::nullopt,
                     std::optional<dataframe> positions = std::nullopt,
                     std::optional<dataframe> transactions = std::nullopt,
                     std::optional<dataframe> benchmark_returns = std::nullopt)
        : returns_(std::move(returns)),
          positions_(std::move(positions)),
          transactions_(std::move(transactions)),
          benchmark_returns_(std::move(benchmark_returns)) {}

    /// Returns an independent copy of each supplied input table.
    ///
    /// Keys are added only for fields that were supplied; a supplied empty
    /// frame is retained. Repeated calls produce independent frames.
    ///
    /// @return Map from input name to an owned copy of the frame.
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
