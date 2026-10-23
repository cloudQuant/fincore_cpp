#pragma once

#include <fincore/factor/analysis.hpp>
#include <fincore/viz/chart_data.hpp>

namespace fincore {

struct factor_tear_charts {
    series_chart information_coefficient;
    bar_chart quantile_returns;
    series_chart turnover;
    series_chart spread;
    series_chart rank_autocorrelation;
};

factor_tear_charts build_factor_tear_charts(
    const factor_analysis_model& model);

}  // namespace fincore
