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

/// Version of the native result-fingerprint encoding.
///
/// Mixed into every result fingerprint and echoed by
/// `factor_analysis_model::result_fingerprint_version`.
inline constexpr std::uint32_t factor_analysis_fingerprint_version = 2U;

/// Options that select the labels and result views of one analysis run.
///
/// `periods` narrows the analysed forward-return labels; the remaining flags
/// choose the weight construction and which per-group or portfolio members are
/// populated.
struct factor_analysis_config {
    bool long_short = true; ///< Demeans weights and returns; false keeps raw values.
    bool group_neutral = false; ///< Demeans within date and group; needs groups.
    bool equal_weight = false; ///< Equal-weights the positive and negative sides.
    bool by_group = false; ///< Adds per-group views; needs a group column.
    /// Empty selects every forward-return label.
    std::vector<std::string> periods;
    std::vector<int> turnover_periods{1}; ///< Positive, unique lag values.
    /// Only monthly ``"M"``/``"ME"`` aggregation is implemented natively.
    std::vector<std::string> time_aggregation{"M"};
    bool include_portfolio_inputs = true; ///< Builds inputs for the first selected period.
    std::string portfolio_benchmark_period = "1D"; ///< Non-empty label recorded in the fingerprint.
};

/// Factor-value distribution inside one quantile bucket.
///
/// Only finite factor values are counted, so the moments describe the values
/// that survive preparation.
struct factor_quantile_statistics {
    int quantile = 0; ///< Quantile label.
    double minimum = 0.0; ///< Smallest finite factor value.
    double maximum = 0.0; ///< Largest finite factor value.
    double mean = 0.0; ///< Arithmetic mean of the finite values.
    double stddev = 0.0; ///< Sample standard deviation, NaN below two values.
    std::size_t count = 0U; ///< Finite factor observations in the bucket.
    double count_percent = 0.0; ///< Share of all finite values, in percent.
};

/// Top-minus-bottom quantile return spread for one label.
struct factor_spread_row {
    std::string period; ///< Forward-return label.
    double spread = 0.0; ///< Upper quantile mean minus lower quantile mean.
    double standard_error = 0.0; ///< Combined error; NaN when either leg is.
    /// Empty for aggregate rows; set for a by-group spread row.
    std::string group;
};

/// Compounded factor return curve for one forward-return label.
struct factor_cumulative_curve {
    std::string period; ///< Forward-return label.
    factor_series_result values; ///< Dated growth compounded from one.
};

/// Compounded quantile growth curves for one forward-return label.
struct factor_quantile_curve {
    std::string period; ///< Forward-return label.
    std::vector<timestamp> dates; ///< Curve dates, ascending.
    std::vector<int> quantiles; ///< Descending, matching the source renderer.
    std::vector<double> values; ///< Dates x quantiles, compounded growth.
};

/// Overlapping portfolio positions for one forward-return label.
struct factor_position_series {
    std::string period; ///< Forward-return label.
    std::vector<factor_weight_row> positions; ///< Positions for this label.
};

/// Turnover series for one quantile and lag.
struct factor_turnover_series {
    int quantile = 0; ///< Quantile label.
    int lag = 1; ///< Date lag in observations.
    factor_series_result values; ///< Turnover per date; NaN during warm-up.
};

/// Rank-autocorrelation series for one lag.
struct factor_rank_autocorrelation_series {
    int lag = 1; ///< Date lag in observations.
    factor_series_result values; ///< Rank correlation per date; NaN in warm-up.
};

/// Rerun of the analysis on one subgroup's rows.
struct factor_group_analysis {
    std::string group; ///< Group label that selected the rows.
    /// Factor-value distribution inside the subgroup.
    std::vector<factor_quantile_statistics> quantile_statistics;
    /// Subgroup weights; group adjustment is off for this rerun.
    std::vector<factor_weight_row> factor_weights;
    factor_return_table factor_returns; ///< Portfolio returns of the subgroup.
    /// Subgroup quantile means and standard errors.
    quantile_return_table mean_returns_by_quantile;
    factor_ic_table information_coefficient; ///< Subgroup rank IC per date.
    /// Subgroup mean IC per forward-return label.
    factor_ic_summary mean_information_coefficient;
    /// Subgroup turnover per requested lag and quantile.
    std::vector<factor_turnover_series> quantile_turnover;
    /// Subgroup rank autocorrelation per requested lag.
    std::vector<factor_rank_autocorrelation_series> rank_autocorrelation;
};

/// Event-window summary built from the optional event-return ledger.
struct factor_event_model {
    std::vector<int> offsets; ///< Window offsets, ascending.
    std::vector<timestamp> event_dates; ///< Event dates found in both inputs.
    std::vector<double> mean_returns; ///< Mean per offset; NaN when empty.
    /// Window cells, ordered by offset and then event date.
    std::vector<double> return_distribution;
    /// Per-quantile event averages, ascending by quantile.
    std::vector<quantile_event_statistics> quantile_average_returns;
};

/// Monthly aggregation of the information coefficient.
struct factor_time_information_table {
    std::string frequency; ///< Requested aggregation label.
    std::vector<timestamp> dates; ///< Month rows; repeats once per group.
    std::vector<std::string> labels; ///< Forward-return labels.
    std::vector<double> means; ///< (date[, group]) rows x labels; NaN if empty.
    /// Empty for aggregate output; otherwise parallel to dates.
    std::vector<std::string> groups;
};

/// Python's primary by_group result views, kept separate from grouped_results.
/// These tables were computed from the full analysis universe before their
/// group rows were selected, so long_short demeaning can span groups.
struct factor_group_snapshot {
    std::string group; ///< Group label that selected the rows.
    /// Group rows of the aggregate quantile means.
    quantile_return_table mean_returns_by_quantile;
    /// Group rows of the per-date quantile means.
    quantile_date_return_table mean_returns_by_date;
    /// Spread rows for this group.
    std::vector<factor_spread_row> mean_return_spread;
    /// Group rows of the rank IC table.
    factor_ic_table information_coefficient;
    /// This group's mean IC row.
    factor_ic_summary mean_information_coefficient;
    /// Monthly IC tables restricted to this group.
    std::vector<factor_time_information_table> time_aggregated_information;
};

/// Complete result of one `analyze_factor` run.
///
/// Aggregate members are built for every run; per-group members appear only
/// when `config.by_group` is true, and the event members only for the overload
/// that receives an event-return ledger.
struct factor_analysis_model {
    prepared_factor_data factor_data; ///< Owning copy of the analysed input.
    std::vector<std::string> forward_periods; ///< Selected labels, in request order.
    /// Per-quantile factor distribution.
    std::vector<factor_quantile_statistics> quantile_statistics;
    // Longstanding C++ table fields below remain aggregate views even when
    // config.by_group is true. They map to Python aggregate_* snapshots;
    // group_snapshots owns Python's primary by_group views. grouped_results
    // remains a separate per-subgroup-universe analysis.
    /// Aggregate rank IC per date and label; never filled with group rows.
    factor_ic_table information_coefficient;
    /// Mean of the finite aggregate IC values per label.
    factor_ic_summary mean_information_coefficient;
    factor_return_table factor_returns; ///< Portfolio returns per date and label.
    std::vector<factor_weight_row> factor_weights; ///< Per-observation weights.
    quantile_return_table mean_returns_by_quantile; ///< Aggregate quantile means.
    quantile_date_return_table mean_returns_by_date; ///< Per-date quantile means.
    /// Spreads per label; empty below two observed quantiles.
    std::vector<factor_spread_row> mean_return_spread;
    /// Annualized alpha and beta per label.
    std::vector<factor_alpha_beta_row> alpha_beta;
    /// Turnover per requested lag and observed quantile.
    std::vector<factor_turnover_series> quantile_turnover;
    /// Rank autocorrelation per requested lag.
    std::vector<factor_rank_autocorrelation_series> rank_autocorrelation;
    /// Compounded factor return curve per selected period.
    std::vector<factor_cumulative_curve> factor_cumulative_returns;
    /// Compounded quantile growth curves per selected period.
    std::vector<factor_quantile_curve> quantile_cumulative_returns;
    /// Overlapping positions per selected period.
    std::vector<factor_position_series> factor_positions;
    /// Monthly mean IC, one table per requested frequency.
    std::vector<factor_time_information_table> time_aggregated_information;
    /// Independent subgroup reruns when `by_group` is true.
    std::vector<factor_group_analysis> grouped_results;
    /// Present only for a call that supplies an event-return ledger.
    std::optional<factor_event_model> event_analysis;
    /// Owning copy of the event ledger that was analysed.
    std::optional<dataframe> event_input_snapshot;
    /// Daily inputs from the first selected period when requested.
    std::optional<factor_portfolio_inputs> portfolio_inputs;
    /// SHA-256 hex digest of the options, input and event ledger.
    std::string result_fingerprint;
    /// Present only when group_neutral makes the legacy summary IC differ
    /// from information_coefficient; otherwise that field is the same view.
    std::optional<factor_ic_table> summary_information_coefficient;
    /// Per-group primary views, filled only when `config.by_group` is true.
    std::vector<factor_group_snapshot> group_snapshots;
    /// Owning copy of the requested options, including the original periods.
    factor_analysis_config config;
    /// Fingerprint encoding version recorded with this result.
    std::uint32_t result_fingerprint_version =
        factor_analysis_fingerprint_version;

    /// Returns the deterministic JSON serialization envelope.
    ///
    /// Implemented in factor/serialization.cpp; declared here so the model
    /// keeps the source's ``to_serializable`` member route.
    ///
    /// @return JSON text of the serialization envelope.
    std::string to_serializable() const;
};

/// Computes the complete factor analysis model for one prepared panel.
///
/// Runs the information-coefficient, weight, return, quantile, spread,
/// turnover, rank-autocorrelation, cumulative, position and monthly
/// aggregation kernels over the selected labels. The returned model owns
/// copies of the input and of the requested options.
///
/// @param factor_data Prepared rows, forward-return labels and cleaning losses.
/// @param config Labels and views to build; the defaults analyse every label
///     with long-short aggregate output.
/// @return Aggregate views, optional per-group views and the input fingerprint.
/// @throws factor_data_error If `factor_data` has no rows or labels, a row does
///     not match the label count, `turnover_periods` or `time_aggregation` is
///     empty, duplicated or invalid, `portfolio_benchmark_period` is empty, a
///     selected period is not a prepared label or not a whole-day label, or
///     `by_group` is set without a group column.
factor_analysis_model analyze_factor(
    const prepared_factor_data& factor_data,
    const factor_analysis_config& config = {});

/// Computes the analysis model together with an event-return study.
///
/// Event dates are the factor dates present in `event_returns`. Each window
/// spans `event_before` ledger rows before the event through `event_after` rows
/// after it, clipped to the ledger bounds; ledger values are used as given and
/// are demeaned across the universe when `config.long_short` is true.
///
/// @param factor_data Prepared rows, forward-return labels and cleaning losses.
/// @param config Labels and views to build; `group_neutral` must be false.
/// @param event_returns Rectangular ledger of double columns indexed by date.
/// @param event_before Non-negative ledger rows before each event date.
/// @param event_after Non-negative ledger rows after each event date.
/// @return Model with `event_analysis` and `event_input_snapshot` set.
/// @throws factor_data_error If the common validations fail, the ledger is not
///     a rectangular double table, its index repeats, a factor asset is missing
///     from it, an offset is negative, or group-neutral event analysis is
///     requested.
factor_analysis_model analyze_factor(
    const prepared_factor_data& factor_data,
    const factor_analysis_config& config, const dataframe& event_returns,
    int event_before, int event_after);

/// Runs IC inference and false-discovery-rate correction on the model.
///
/// Forwards `model.information_coefficient` to
/// `information_coefficient_inference`; per-group tables are not consulted.
///
/// @param model Analysed model whose aggregate IC table is read.
/// @param alpha Target false-discovery rate in (0, 1].
/// @return One hypothesis per IC label with the Benjamini-Hochberg adjustment.
/// @throws factor_data_error If `alpha` is not a finite probability in (0, 1].
ic_inference_result factor_model_inference(
    const factor_analysis_model& model, double alpha = 0.05);


namespace detail {

/// Computes the native v2 fingerprint without running the analysis kernels.
///
/// Validates the requested periods like `analyze_factor` and hashes the same
/// encoding, so the digest matches a model built from the same inputs.
///
/// @param data Prepared panel to encode.
/// @param config Options to encode; `periods` are validated against `data`.
/// @return Lower-case SHA-256 hex digest of the full input encoding.
/// @throws factor_data_error If a requested period is empty, duplicated or not
///     a prepared label.
std::string factor_fingerprint_for_benchmark(
    const prepared_factor_data& data, const factor_analysis_config& config);

}  // namespace detail

}  // namespace fincore
