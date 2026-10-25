#pragma once

#include <fincore/factor/research.hpp>

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fincore {

inline constexpr const char* research_detailed_api_version = "2.0";
inline constexpr int research_detailed_result_schema_version = 2;

/// One date/factor/period observation from the Python research ``ic`` table.
struct research_detailed_ic_row {
    timestamp time = 0;
    std::string factor;
    std::string period;
    double ic = 0.0;
    double rank_ic = 0.0;
    std::size_t n_assets = 0U;
};

/// Finite-observation summaries for Pearson and Spearman (rank) IC.
struct research_detailed_ic_summary_row {
    std::string factor;
    std::string period;
    double mean_ic = 0.0;
    double std_ic = 0.0;
    double icir = 0.0;
    double mean_rank_ic = 0.0;
    double std_rank_ic = 0.0;
    double rank_icir = 0.0;
    std::size_t n_dates = 0U;
};

/// One date/factor/period/quantile raw-return mean and its contributing count.
struct research_detailed_quantile_return_row {
    timestamp time = 0;
    std::string factor;
    std::string period;
    int quantile = 0;
    double mean_return = 0.0;
    std::size_t n_assets = 0U;
};

/// Quantile turnover with the period-specific lag used by the Python API.
struct research_detailed_turnover_row {
    timestamp time = 0;
    std::string factor;
    std::string period;
    int quantile = 0;
    double turnover = 0.0;
};

/// Daily top-minus-bottom quantile spread and the quantiles used.
struct research_detailed_spread_row {
    timestamp time = 0;
    std::string factor;
    std::string period;
    double spread = 0.0;
    int top_quantile = 0;
    int bottom_quantile = 0;
};

/// Whole processed-panel stability summary for one requested lag.
struct research_detailed_stability_row {
    std::string factor;
    int lag = 0;
    double mean_rank_autocorrelation = 0.0;
    double std_rank_autocorrelation = 0.0;
    std::size_t n_dates = 0U;
};

/// Coverage counts for the single factor evaluated by this API.
struct research_detailed_coverage_row {
    std::string factor;
    std::string kind = "factor";
    int direction = 1;
    std::size_t input_cells = 0U;
    std::size_t finite_cells = 0U;
    std::size_t label_cells = 0U;
    std::size_t evaluable_cells = 0U;
    std::string status;
};

/// Structured, source-compatible diagnostic row. ``factor`` is empty when
/// the Python table's nullable factor value is absent.
struct research_detailed_diagnostic {
    std::optional<std::string> factor;
    std::string stage;
    std::string severity;
    std::string code;
    std::string message;
};

/// Version 2 owns typed rows for the factor-local detailed research tables.
/// Rows follow the pinned Python 1.0 evaluator's append order; all numeric
/// missing values are quiet NaNs. The single-factor entry represents the
/// Python correlation and comparisons tables as empty vectors.
struct research_detailed_result {
    std::string status;
    std::vector<research_signal_row> signals;
    /// Complete panel × period grid, including NaN values for missing labels.
    std::vector<research_label_row> labels;
    std::vector<research_detailed_ic_row> ic;
    std::vector<research_detailed_ic_summary_row> ic_summary;
    std::vector<research_detailed_quantile_return_row> quantile_returns;
    std::vector<research_detailed_turnover_row> turnover;
    std::vector<research_detailed_spread_row> spreads;
    std::vector<research_detailed_stability_row> stability;
    std::vector<research_correlation_row> correlation;
    std::vector<research_comparison_row> comparisons;
    std::vector<research_detailed_coverage_row> coverage;
    std::vector<research_detailed_diagnostic> diagnostics;
    std::map<std::string, std::string> manifest;
    int schema_version = research_detailed_result_schema_version;
};

/// Evaluate one named factor against price-derived forward labels.
///
/// ``factor_name`` supplies the Python DataFrame column name used for
/// direction overrides and output rows. ``spec.start``/``end`` and all input
/// timestamps are UTC nanoseconds. Only ``close_to_close`` and
/// ``next_open_to_open`` are supported by this price-based overload;
/// precomputed labels and multi-factor combinations are rejected explicitly.
research_detailed_result evaluate_factor_research_detailed(
    std::string_view factor_name, const factor_series& factors,
    const dataframe& prices, const research_spec& spec);

}  // namespace fincore
