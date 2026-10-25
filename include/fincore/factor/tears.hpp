#pragma once

#include <fincore/factor/analysis.hpp>
#include <fincore/viz/chart_data.hpp>

#include <optional>
#include <string>
#include <vector>

namespace fincore {

/// Chart data for one factor group.
struct factor_group_tear_charts {
  std::string group; ///< Group label.
  series_chart information_coefficient; ///< Information coefficient by forward label.
  bar_chart quantile_returns; ///< Mean return by quantile per forward label.
  series_chart turnover; ///< Quantile turnover at the primary lag.
  bar_chart spread; ///< Top-minus-bottom quantile spread by period.
  series_chart rank_autocorrelation; ///< Rank autocorrelation by lag.
};

/// Chart data for an optional event study.
struct factor_event_tear_charts {
  std::vector<int> offsets; ///< Event offsets in observations.
  series_chart mean_returns; ///< Mean return over the offsets.
  histogram_bins return_distribution; ///< Histogram of event-window returns.
  series_chart quantile_average_returns; ///< Mean return by quantile over the offsets.
};

/// Chart bundle for one factor analysis.
///
/// `grouped` entries carry every panel, while `group_snapshots` entries carry
/// only their information coefficient, quantile returns and spread panels.
struct factor_tear_charts {
  series_chart information_coefficient; ///< Information coefficient by forward label.
  bar_chart quantile_returns; ///< Mean return by quantile per forward label.
  series_chart turnover; ///< Quantile turnover at the primary lag.
  bar_chart spread; ///< Top-minus-bottom spread per forward period.
  series_chart rank_autocorrelation; ///< Rank autocorrelation by lag.
  std::vector<factor_group_tear_charts> grouped; ///< Per-group charts with every panel.
  std::vector<factor_group_tear_charts> group_snapshots; ///< Per-group snapshot charts.
  std::optional<factor_event_tear_charts> event_analysis; ///< Event-study charts when present.
};

/// Builds owning chart data for a factor tear sheet.
///
/// Time-series charts omit non-finite points, bar charts keep the panel
/// ordering of `model`, and the event histogram is rebuilt from the model's
/// finite return distribution.
///
/// @param model Analyzed factor model supplying every panel.
/// @return Chart bundle that owns copies and outlives `model`.
/// @throws input_error When event analysis is present without a finite
///         return-distribution value.
factor_tear_charts build_factor_tear_charts(const factor_analysis_model &model);

} // namespace fincore
