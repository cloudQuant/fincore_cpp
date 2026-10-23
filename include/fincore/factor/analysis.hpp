#pragma once

#include <fincore/factor/data.hpp>
#include <fincore/factor/performance.hpp>

#include <string>
#include <vector>

namespace fincore {

struct factor_analysis_config {
    bool long_short = true;
    bool group_neutral = false;
    bool equal_weight = false;
};

struct factor_analysis_model {
    prepared_factor_data factor_data;
    std::vector<std::string> forward_periods;
    factor_ic_table information_coefficient;
    factor_ic_summary mean_information_coefficient;
    factor_return_table factor_returns;
    std::vector<factor_weight_row> factor_weights;
    quantile_return_table mean_returns_by_quantile;
    std::vector<factor_alpha_beta_row> alpha_beta;
    std::vector<factor_series_result> quantile_turnover;
    factor_series_result rank_autocorrelation;
    std::string result_fingerprint;
};

factor_analysis_model analyze_factor(
    const prepared_factor_data& factor_data,
    const factor_analysis_config& config = {});

}  // namespace fincore
