#pragma once

#include <fincore/metrics/returns.hpp>

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace fincore {

/// Lazy, value-owning container for first-phase performance analytics.
///
/// summary() returns these stable keys: Annual return, Cumulative returns,
/// Annual volatility, Sharpe ratio, Sortino ratio, Calmar ratio, Max drawdown,
/// Historical value at risk, Conditional value at risk, and, when supplied a
/// benchmark, Alpha and Beta.
class analysis_context {
public:
    explicit analysis_context(dataframe returns,
                              std::optional<dataframe> benchmark = std::nullopt,
                              double annualization = daily_annualization);

    [[nodiscard]] std::map<std::string, double> summary() const;
    [[nodiscard]] std::size_t computation_count(std::string_view key) const;

private:
    [[nodiscard]] double cached(std::string_view key) const;

    dataframe returns_;
    std::optional<dataframe> benchmark_;
    double annualization_;
    mutable std::unordered_map<std::string, double> scalar_cache_;
    mutable std::unordered_map<std::string, std::size_t> computation_counts_;
};

}  // namespace fincore
