#pragma once

#include <fincore/factor/analysis.hpp>
#include <fincore/viz/chart_data.hpp>

#include <string>
#include <vector>

namespace fincore {

struct factor_group_tear_charts {
    std::string group;
    series_chart information_coefficient;
    bar_chart quantile_returns;
    series_chart turnover;
    series_chart spread;
    series_chart rank_autocorrelation;
};

struct factor_tear_charts {
    series_chart information_coefficient;
    bar_chart quantile_returns;
    series_chart turnover;
    series_chart spread;
    series_chart rank_autocorrelation;
    std::vector<factor_group_tear_charts> grouped;
};

factor_tear_charts build_factor_tear_charts(
    const factor_analysis_model& model);

}  // namespace fincore
