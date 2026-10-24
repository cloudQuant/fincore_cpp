#pragma once

#include <fincore/factor/calendar.hpp>
#include <fincore/factor/data.hpp>
#include <fincore/factor/performance.hpp>
#include <fincore/factor/portfolio.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace fincore {

inline constexpr std::uint32_t factor_analysis_fingerprint_version = 2U;

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
    /// Empty for aggregate rows; set for a by-group spread row.
    std::string group;
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
    std::vector<timestamp> dates;  // month rows; repeats once per group
    std::vector<std::string> labels;
    std::vector<double> means;  // (date[, group]) rows x labels
    /// Empty for aggregate output; otherwise parallel to dates.
    std::vector<std::string> groups;
};

/// Python's primary by_group result views, kept separate from grouped_results.
/// These tables were computed from the full analysis universe before their
/// group rows were selected, so long_short demeaning can span groups.
struct factor_group_snapshot {
    std::string group;
    quantile_return_table mean_returns_by_quantile;
    quantile_date_return_table mean_returns_by_date;
    std::vector<factor_spread_row> mean_return_spread;
    factor_ic_table information_coefficient;
    factor_ic_summary mean_information_coefficient;
    std::vector<factor_time_information_table> time_aggregated_information;
};

struct factor_analysis_model {
    prepared_factor_data factor_data;
    std::vector<std::string> forward_periods;
    std::vector<factor_quantile_statistics> quantile_statistics;
    // Longstanding C++ table fields below remain aggregate views even when
    // config.by_group is true. They map to Python aggregate_* snapshots;
    // group_snapshots owns Python's primary by_group views. grouped_results
    // remains a separate per-subgroup-universe analysis.
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
    /// Present only when group_neutral makes the legacy summary IC differ
    /// from information_coefficient; otherwise that field is the same view.
    std::optional<factor_ic_table> summary_information_coefficient;
    std::vector<factor_group_snapshot> group_snapshots;
    /// Owning copy of the requested options, including the original periods.
    factor_analysis_config config;
    std::uint32_t result_fingerprint_version =
        factor_analysis_fingerprint_version;

    /// Deterministic JSON representation (native serialization envelope).
    ///
    /// Implemented in factor/serialization.cpp; declared here so the model
    /// keeps the source's ``to_serializable`` member route.
    std::string to_serializable() const;
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


namespace detail {

/// Benchmark-only entry to the native v2 fingerprint encoder.
std::string factor_fingerprint_for_benchmark(
    const prepared_factor_data& data, const factor_analysis_config& config);

}  // namespace detail

}  // namespace fincore
