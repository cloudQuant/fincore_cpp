#pragma once

#include <fincore/factor/analysis.hpp>
#include <fincore/viz/chart_data.hpp>

#include <optional>
#include <string>
#include <vector>

namespace fincore {

struct factor_group_tear_charts {
  std::string group;
  series_chart information_coefficient;
  bar_chart quantile_returns;
  series_chart turnover;
  bar_chart spread;
  series_chart rank_autocorrelation;
};

struct factor_event_tear_charts {
  std::vector<int> offsets;
  series_chart mean_returns;
  histogram_bins return_distribution;
  series_chart quantile_average_returns;
};

struct factor_tear_charts {
  series_chart information_coefficient;
  bar_chart quantile_returns;
  series_chart turnover;
  bar_chart spread;
  series_chart rank_autocorrelation;
  std::vector<factor_group_tear_charts> grouped;
  std::vector<factor_group_tear_charts> group_snapshots;
  std::optional<factor_event_tear_charts> event_analysis;
};

factor_tear_charts build_factor_tear_charts(const factor_analysis_model &model);

} // namespace fincore
