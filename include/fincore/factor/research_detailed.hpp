#pragma once

#include <fincore/factor/research.hpp>

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fincore {

/// Detailed research API version recorded in result and export manifests.
inline constexpr const char* research_detailed_api_version = "2.0";
/// Schema version of `research_detailed_result`.
inline constexpr int research_detailed_result_schema_version = 2;

/// One date/factor/period observation from the Python research ``ic`` table.
struct research_detailed_ic_row {
    timestamp time = 0; ///< Observation date in UTC nanoseconds.
    std::string factor; ///< Factor name.
    std::string period; ///< Forward-return period label.
    double ic = 0.0; ///< Pearson IC for the date; NaN when undefined.
    double rank_ic = 0.0; ///< Spearman rank IC; NaN when undefined.
    std::size_t n_assets = 0U; ///< Paired assets contributing to the IC.
};

/// Finite-observation summaries for Pearson and Spearman (rank) IC.
struct research_detailed_ic_summary_row {
    std::string factor; ///< Factor name.
    std::string period; ///< Forward-return period label.
    double mean_ic = 0.0; ///< Mean of finite Pearson IC values.
    double std_ic = 0.0; ///< Sample standard deviation of Pearson ICs.
    double icir = 0.0; ///< Mean IC divided by its sample deviation.
    double mean_rank_ic = 0.0; ///< Mean of finite rank IC values.
    double std_rank_ic = 0.0; ///< Sample standard deviation of rank ICs.
    double rank_icir = 0.0; ///< Mean rank IC divided by its sample deviation.
    std::size_t n_dates = 0U; ///< Number of finite Pearson IC observations.
};

/// One date/factor/period/quantile raw-return mean and its contributing count.
struct research_detailed_quantile_return_row {
    timestamp time = 0; ///< Observation date in UTC nanoseconds.
    std::string factor; ///< Factor name.
    std::string period; ///< Forward-return period label.
    int quantile = 0; ///< One-based quantile label.
    double mean_return = 0.0; ///< Mean raw forward return in the quantile.
    std::size_t n_assets = 0U; ///< Label cells contributing to the mean.
};

/// Quantile turnover with the period-specific lag used by the Python API.
struct research_detailed_turnover_row {
    timestamp time = 0; ///< Observation date in UTC nanoseconds.
    std::string factor; ///< Factor name.
    std::string period; ///< Forward-return period label.
    int quantile = 0; ///< One-based quantile label.
    double turnover = 0.0; ///< Share of members absent at the lagged date.
};

/// Daily top-minus-bottom quantile spread and the quantiles used.
struct research_detailed_spread_row {
    timestamp time = 0; ///< Observation date in UTC nanoseconds.
    std::string factor; ///< Factor name.
    std::string period; ///< Forward-return period label.
    double spread = 0.0; ///< Top-quantile mean minus bottom-quantile mean.
    int top_quantile = 0; ///< Highest populated quantile label.
    int bottom_quantile = 0; ///< Lowest populated quantile label.
};

/// Whole processed-panel stability summary for one requested lag.
struct research_detailed_stability_row {
    std::string factor; ///< Factor name.
    int lag = 0; ///< Lag in observation dates.
    double mean_rank_autocorrelation = 0.0; ///< Mean rank autocorrelation.
    double std_rank_autocorrelation = 0.0; ///< Sample standard deviation.
    std::size_t n_dates = 0U; ///< Number of finite autocorrelation values.
};

/// Coverage counts for the single factor evaluated by this API.
struct research_detailed_coverage_row {
    std::string factor; ///< Factor name.
    std::string kind = "factor"; ///< Coverage row kind; `"factor"` here.
    int direction = 1; ///< Applied direction, +1 or -1.
    std::size_t input_cells = 0U; ///< Cells after window/target selection.
    std::size_t finite_cells = 0U; ///< Cells with a finite processed factor.
    std::size_t label_cells = 0U; ///< Cells with at least one finite label.
    std::size_t evaluable_cells = 0U; ///< Cells with finite factor and label.
    std::string status; ///< `"OK"`, `"PARTIAL"` or `"UNEVALUABLE"`.
};

/// Structured, source-compatible diagnostic row. ``factor`` is empty when
/// the Python table's nullable factor value is absent.
struct research_detailed_diagnostic {
    std::optional<std::string> factor; ///< Factor name; empty when absent.
    std::string stage; ///< Evaluation stage that emitted the diagnostic.
    std::string severity; ///< Severity such as `"warning"` or `"error"`.
    std::string code; ///< Stable machine-readable diagnostic code.
    std::string message; ///< Human-readable diagnostic text.
};

/// Version 2 owns typed rows for the factor-local detailed research tables.
/// Rows follow the pinned Python 1.0 evaluator's append order; all numeric
/// missing values are quiet NaNs. The single-factor entry represents the
/// Python correlation and comparisons tables as empty vectors.
struct research_detailed_result {
    std::string status; ///< Overall status: `"OK"` or `"UNEVALUABLE"`.
    std::vector<research_signal_row> signals; ///< Signal rows per panel cell.
    /// Complete panel × period grid, including NaN values for missing labels.
    std::vector<research_label_row> labels;
    std::vector<research_detailed_ic_row> ic; ///< Per-date Pearson/rank IC.
    std::vector<research_detailed_ic_summary_row> ic_summary; ///< One per period.
    std::vector<research_detailed_quantile_return_row> quantile_returns; ///< Quantile means.
    std::vector<research_detailed_turnover_row> turnover; ///< Quantile turnover.
    std::vector<research_detailed_spread_row> spreads; ///< Daily spreads.
    std::vector<research_detailed_stability_row> stability; ///< Rank stability.
    std::vector<research_correlation_row> correlation; ///< Empty for this API.
    std::vector<research_comparison_row> comparisons; ///< Empty for this API.
    std::vector<research_detailed_coverage_row> coverage; ///< Factor coverage.
    std::vector<research_detailed_diagnostic> diagnostics; ///< Warnings and errors.
    std::map<std::string, std::string> manifest; ///< Metadata and row counts.
    int schema_version = research_detailed_result_schema_version; ///< Schema version.
};

/// Evaluates one named factor against price-derived forward labels.
///
/// ``factor_name`` supplies the Python DataFrame column name used for
/// direction overrides and output rows. ``spec.start``/``end`` and all input
/// timestamps are UTC nanoseconds. Only ``close_to_close`` and
/// ``next_open_to_open`` are supported by this price-based overload;
/// precomputed labels and multi-factor combinations are rejected explicitly.
///
/// @param factor_name Factor name used for output rows and direction
///        overrides.
/// @param factors Factor panel with row-aligned dates, assets and values.
/// @param prices Price panel indexed by date with one column per asset.
/// @param spec Evaluation window, label, preprocessing and table settings.
/// @return Detailed result with signal, label, IC, quantile, stability,
///         coverage and diagnostic tables.
/// @throws factor_data_error When the factor, price or specification input is
///         invalid, including unsupported labels or missing price columns.
research_detailed_result evaluate_factor_research_detailed(
    std::string_view factor_name, const factor_series& factors,
    const dataframe& prices, const research_spec& spec);

}  // namespace fincore
