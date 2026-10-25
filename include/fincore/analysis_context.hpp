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
    /// Constructs a context from return and optional benchmark frames.
    ///
    /// The frames are stored as supplied; only the required double columns
    /// are validated, and no sorting or index alignment is performed here.
    ///
    /// @param returns Frame with a required double ``returns`` column.
    /// @param benchmark Optional frame with a required double ``benchmark``
    ///     column; enables the Alpha and Beta summary entries.
    /// @param annualization Annualization factor used by annualized metrics.
    /// @throws input_error If a required column is absent, not a double
    ///     column, or does not match its frame index length.
    explicit analysis_context(dataframe returns,
                              std::optional<dataframe> benchmark = std::nullopt,
                              double annualization = daily_annualization);

    /// Returns the cached performance summary.
    ///
    /// Core keys are ``Annual return``, ``Cumulative returns``, ``Annual
    /// volatility``, ``Sharpe ratio``, ``Sortino ratio``, ``Calmar ratio``,
    /// ``Max drawdown``, ``Historical value at risk`` and ``Conditional value
    /// at risk``. With a benchmark the result also contains ``Alpha`` and
    /// ``Beta``. Each metric is computed on first use and reused by later
    /// calls.
    ///
    /// @return Map from stable metric name to computed value.
    [[nodiscard]] std::map<std::string, double> summary() const;

    /// Returns how many times ``key`` has been computed and cached.
    ///
    /// @param key Exact summary metric key.
    /// @return Number of computations for ``key``; 0 if it has not been
    ///     computed.
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
