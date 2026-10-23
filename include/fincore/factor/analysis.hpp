#pragma once

#include <fincore/factor/data.hpp>
#include <fincore/factor/performance.hpp>
#include <fincore/factor/portfolio.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace fincore {

struct factor_analysis_config {
    bool long_short = true;
    bool group_neutral = false;
    bool equal_weight = false;
    bool by_group = false;
    /// Empty selects every forward-return label.
    std::vector<std::string> periods;
    std::vector<int> turnover_periods{1};
    /// Only monthly ``"M"``/``"ME"`` aggregation is implemented natively.
    std::vector<std::string> time_aggregation{"M"};
    bool include_portfolio_inputs = true;
    std::string portfolio_benchmark_period = "1D";
};

struct factor_quantile_statistics {
    int quantile = 0;
    double minimum = 0.0;
    double maximum = 0.0;
    double mean = 0.0;
    double stddev = 0.0;  // sample standard deviation, NaN below two values
    std::size_t count = 0U;
    double count_percent = 0.0;
};

struct factor_spread_row {
    std::string period;
    double spread = 0.0;
    double standard_error = 0.0;
};

struct factor_cumulative_curve {
    std::string period;
    factor_series_result values;
};

struct factor_quantile_curve {
    std::string period;
    std::vector<timestamp> dates;
    std::vector<int> quantiles;  // descending, matching the source renderer
    std::vector<double> values;  // dates x quantiles
};

struct factor_position_series {
    std::string period;
    std::vector<factor_weight_row> positions;
};

struct factor_turnover_series {
    int quantile = 0;
    int lag = 1;
    factor_series_result values;
};

struct factor_rank_autocorrelation_series {
    int lag = 1;
    factor_series_result values;
};

struct factor_group_analysis {
    std::string group;
    std::vector<factor_quantile_statistics> quantile_statistics;
    std::vector<factor_weight_row> factor_weights;
    factor_return_table factor_returns;
    quantile_return_table mean_returns_by_quantile;
    factor_ic_table information_coefficient;
    factor_ic_summary mean_information_coefficient;
    std::vector<factor_turnover_series> quantile_turnover;
    std::vector<factor_rank_autocorrelation_series> rank_autocorrelation;
};

struct factor_event_model {
    std::vector<int> offsets;
    std::vector<timestamp> event_dates;
    std::vector<double> mean_returns;         // per offset
    std::vector<double> return_distribution;  // flattened finite window cells
    std::vector<quantile_event_statistics> quantile_average_returns;
};

struct factor_time_information_table {
    std::string frequency;
    std::vector<timestamp> dates;
    std::vector<std::string> labels;
    std::vector<double> means;  // dates x labels
};

struct factor_analysis_model {
    prepared_factor_data factor_data;
    std::vector<std::string> forward_periods;
    std::vector<factor_quantile_statistics> quantile_statistics;
    factor_ic_table information_coefficient;
    factor_ic_summary mean_information_coefficient;
    factor_return_table factor_returns;
    std::vector<factor_weight_row> factor_weights;
    quantile_return_table mean_returns_by_quantile;
    quantile_date_return_table mean_returns_by_date;
    std::vector<factor_spread_row> mean_return_spread;
    std::vector<factor_alpha_beta_row> alpha_beta;
    std::vector<factor_turnover_series> quantile_turnover;
    std::vector<factor_rank_autocorrelation_series> rank_autocorrelation;
    std::vector<factor_cumulative_curve> factor_cumulative_returns;
    std::vector<factor_quantile_curve> quantile_cumulative_returns;
    std::vector<factor_position_series> factor_positions;
    std::vector<factor_time_information_table> time_aggregated_information;
    std::vector<factor_group_analysis> grouped_results;
    std::optional<factor_event_model> event_analysis;
    std::optional<dataframe> event_input_snapshot;
    std::optional<factor_portfolio_inputs> portfolio_inputs;
    std::string result_fingerprint;
};

factor_analysis_model analyze_factor(
    const prepared_factor_data& factor_data,
    const factor_analysis_config& config = {});

/// Analyze with an optional event-return ledger (`event_before`/`event_after`
/// index offsets). Event analysis requires `group_neutral=false`.
factor_analysis_model analyze_factor(
    const prepared_factor_data& factor_data,
    const factor_analysis_config& config, const dataframe& event_returns,
    int event_before, int event_after);

/// IC/FDR inference over the model's stored aggregate IC snapshot.
ic_inference_result factor_model_inference(
    const factor_analysis_model& model, double alpha = 0.05);

}  // namespace fincore
