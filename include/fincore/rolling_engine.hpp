#pragma once

#include <fincore/metrics/returns.hpp>

#include <map>
#include <optional>
#include <span>
#include <cstddef>
#include <string>

namespace fincore {

/// Trailing-window metric computed by rolling_engine.
enum class rolling_metric {
    mean_return,  ///< Annualized arithmetic mean of the window.
    volatility,   ///< Annualized sample volatility of the window.
    sharpe,       ///< Annualized Sharpe ratio with zero risk-free rate.
    max_drawdown, ///< Worst peak-to-trough drawdown in the window.
    beta          ///< Unannualized beta against the aligned benchmark window.
};

/// Computes trailing-window series for one or more rolling metrics.
///
/// The engine owns copies of the return and optional benchmark frames. Each
/// metric is evaluated over every complete window of ``window`` observations;
/// rows before the first complete window are omitted from the output.
class rolling_engine {
public:
    /// Constructs an engine over return and optional benchmark frames.
    ///
    /// @param returns Frame with a required double ``returns`` column.
    /// @param benchmark Optional frame with a required double ``benchmark``
    ///     column; required only when rolling beta is requested.
    /// @param window Number of observations per window; must be at least 2 and
    ///     defaults to ``daily_annualization`` cast to ``std::size_t``.
    /// @param annualization Annualization factor used by annualized metrics.
    /// @throws input_error If ``window`` is less than 2, or a required column
    ///     is absent, not a double column, or does not match its frame index
    ///     length.
    rolling_engine(dataframe returns,
                   std::optional<dataframe> benchmark = std::nullopt,
                   std::size_t window = static_cast<std::size_t>(daily_annualization),
                   double annualization = daily_annualization);

    /// Returns one trailing-window series per requested metric.
    ///
    /// The result maps each metric's enum name (``mean_return``,
    /// ``volatility``, ``sharpe``, ``max_drawdown``, ``beta``) to a frame
    /// whose single ``returns`` column holds the metric value at each complete
    /// window's final timestamp. Beta inputs are first aligned on their common
    /// index, so its series can be shorter. An empty metric list returns an
    /// empty map, and fewer than ``window`` available observations yield empty
    /// series.
    ///
    /// @param metrics Metrics to compute.
    /// @return Map from metric key to a single-column series indexed by window
    ///     end timestamps.
    /// @throws input_error If a metric value is unknown, beta is requested
    ///     without a benchmark, or a required column is invalid.
    [[nodiscard]] std::map<std::string, dataframe> compute(
        std::span<const rolling_metric> metrics) const;

private:
    dataframe returns_;
    std::optional<dataframe> benchmark_;
    std::size_t window_;
    double annualization_;
};

}  // namespace fincore
