#pragma once

#include <fincore/errors.hpp>
#include <fincore/factor/data.hpp>
#include <fincore/factor/performance.hpp>
#include <fincore/factor/serialization.hpp>

#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fincore {

/// Research API version reported by capability and result manifests.
inline constexpr const char* research_api_version = "1.0";
/// Schema version of research result envelopes and their manifests.
inline constexpr int research_result_schema_version = 1;
/// Schema version of the normalized research evaluation configuration.
inline constexpr int EVALUATION_CONFIG_SCHEMA_VERSION = 1;
/// Lower-case alias of EVALUATION_CONFIG_SCHEMA_VERSION.
inline constexpr int evaluation_config_schema_version =
    EVALUATION_CONFIG_SCHEMA_VERSION;

/// Native structured research errors. Python exception inheritance is not
/// reproduced; ``category`` preserves the source class name and ``code``
/// carries the stable machine-readable identifier.
class research_error : public fincore_error {
public:
    /// Constructs a structured research error.
    ///
    /// @param category Source exception class name preserved for callers.
    /// @param code Stable machine-readable error identifier.
    /// @param message Human-readable failure description.
    /// @param report Optional structured diagnostic fields.
    research_error(std::string category, std::string code,
                   const std::string& message,
                   std::map<std::string, std::string> report = {})
        : fincore_error(make_error(std::move(category), std::move(code),
                                   message, {}, std::move(report))) {}
};

/// Research error with the ``ConfigError`` category.
class research_config_error : public research_error {
public:
    /// Category name reported for configuration failures.
    static constexpr const char* default_category = "ConfigError";
    /// Default machine-readable code for invalid configuration.
    static constexpr const char* default_code = "invalid_config";
    /// Constructs a configuration error.
    ///
    /// @param message Human-readable failure description.
    /// @param code Machine-readable identifier; defaults to
    ///        ``invalid_config``.
    /// @param report Optional structured diagnostic fields.
    explicit research_config_error(const std::string& message,
                                   std::string code = default_code,
                                   std::map<std::string, std::string> report = {})
        : research_error(default_category, std::move(code), message,
                         std::move(report)) {}
};

/// Research error with the ``DatasetError`` category.
class research_dataset_error : public research_error {
public:
    /// Category name reported for dataset failures.
    static constexpr const char* default_category = "DatasetError";
    /// Default machine-readable code for invalid datasets.
    static constexpr const char* default_code = "invalid_dataset";
    /// Constructs a dataset error.
    ///
    /// @param message Human-readable failure description.
    /// @param code Machine-readable identifier; defaults to
    ///        ``invalid_dataset``.
    /// @param report Optional structured diagnostic fields.
    explicit research_dataset_error(const std::string& message,
                                    std::string code = default_code,
                                    std::map<std::string, std::string> report = {})
        : research_error(default_category, std::move(code), message,
                         std::move(report)) {}
};

/// Research error with the ``DependencyError`` category.
class research_dependency_error : public research_error {
public:
    /// Category name reported for missing dependencies.
    static constexpr const char* default_category = "DependencyError";
    /// Default machine-readable code for a missing dependency.
    static constexpr const char* default_code = "missing_dependency";
    /// Constructs a dependency error.
    ///
    /// @param message Human-readable failure description.
    /// @param code Machine-readable identifier; defaults to
    ///        ``missing_dependency``.
    /// @param report Optional structured diagnostic fields.
    explicit research_dependency_error(const std::string& message,
                                       std::string code = default_code,
                                       std::map<std::string, std::string> report = {})
        : research_error(default_category, std::move(code), message,
                         std::move(report)) {}
};

/// Research error with the ``DataQualityError`` category.
class research_data_quality_error : public research_error {
public:
    /// Category name reported for data-quality failures.
    static constexpr const char* default_category = "DataQualityError";
    /// Default machine-readable code for data-quality failures.
    static constexpr const char* default_code = "data_quality";
    /// Constructs a data-quality error.
    ///
    /// @param message Human-readable failure description.
    /// @param code Machine-readable identifier; defaults to ``data_quality``.
    /// @param report Optional structured diagnostic fields.
    explicit research_data_quality_error(const std::string& message,
                                         std::string code = default_code,
                                         std::map<std::string, std::string> report = {})
        : research_error(default_category, std::move(code), message,
                         std::move(report)) {}
};

/// Research error with the ``FactorComputationError`` category.
class research_factor_computation_error : public research_error {
public:
    /// Category name reported for factor-computation failures.
    static constexpr const char* default_category = "FactorComputationError";
    /// Default machine-readable code for a computation failure.
    static constexpr const char* default_code = "factor_computation_failed";
    /// Constructs a factor-computation error.
    ///
    /// @param message Human-readable failure description.
    /// @param code Machine-readable identifier; defaults to
    ///        ``factor_computation_failed``.
    /// @param report Optional structured diagnostic fields.
    explicit research_factor_computation_error(
        const std::string& message, std::string code = default_code,
        std::map<std::string, std::string> report = {})
        : research_error(default_category, std::move(code), message,
                         std::move(report)) {}
};

/// Pinned capability declaration for the native research surface.
struct research_capabilities {
    std::string api_version = research_api_version; ///< Research API version string.
    int result_schema_version = research_result_schema_version; ///< Result schema version.
    std::vector<std::string> labels{"close_to_close", "next_open_to_open",
                                    "precomputed"}; ///< Supported label bases.
    std::vector<std::string> preprocess{"cross_section_rank",
                                        "cross_section_zscore", "none"}; ///< Preprocessing methods.
    std::vector<std::string> metrics{
        "pearson_ic", "spearman_ic", "icir", "quantile_returns", "spread",
        "rank_autocorrelation", "quantile_turnover", "factor_correlation",
        "paired_comparison"}; ///< Advertised metric identifiers.
    std::vector<std::string> result_tables{
        "signals", "labels", "ic", "ic_summary", "quantile_returns",
        "turnover", "spreads", "stability", "correlation", "comparisons",
        "coverage", "diagnostics"}; ///< Names accepted by research_result::table().
    std::vector<std::string> export_formats{"json", "csv"}; ///< Advertised export formats.
    std::vector<std::string> statuses{"OK", "PARTIAL", "UNEVALUABLE"}; ///< Evaluation statuses.
};

/// Returns the pinned native research capability declaration.
///
/// @return Capability record listing the supported labels, preprocessing
///         methods, metrics, result tables, export formats and statuses.
research_capabilities research_capabilities_report();

/// Execution specification consumed by the research evaluators.
struct research_spec {
    std::vector<std::string> targets; ///< Requested targets; empty selects all observed assets.
    timestamp start = 0; ///< Inclusive UTC-nanosecond window start.
    timestamp end = 0; ///< Inclusive UTC-nanosecond window end.
    std::vector<int> periods{1, 5, 20}; ///< Forward price-row horizons; non-empty, positive.
    int quantiles = 5; ///< Quantile bucket count; must be at least two.
    int min_assets = 1; ///< Minimum asset count for correlation and detailed thresholds.
    int min_quantile_assets = 1; ///< Minimum observations per quantile for detailed thresholds.
    std::string label = "close_to_close"; ///< Forward-return basis.
    std::string preprocess = "cross_section_rank"; ///< Cross-sectional preprocessing method.
    double max_loss = 1.0; ///< Tolerated binning data-loss fraction in [0, 1].
    std::vector<int> stability_lags{1, 5, 20}; ///< Positive rank-autocorrelation lags.
    std::map<std::string, int> direction_overrides; ///< Optional per-asset signs (+1 or -1).
    std::map<std::string, std::pair<std::string, std::string>> segments; ///< Named provenance ranges.
    std::map<std::string, std::pair<std::string, std::string>> comparisons; ///< Panel-evaluator factor pairs.
    std::map<std::string, std::map<std::string, double>> combinations; ///< Weighted factor blends.
    std::map<std::string, std::string> groups; ///< Asset-to-group map covering evaluated assets.
    std::string snapshot_id = "unspecified"; ///< Provenance snapshot identifier.
    std::string price_basis = "unspecified"; ///< Provenance price basis.
    std::string calendar_id = "unspecified"; ///< Provenance calendar identifier.
    std::string frequency = "unspecified"; ///< Provenance data frequency.
};

/// One signal row: a factor observation for a date and asset.
struct research_signal_row {
    timestamp date = 0; ///< Observation date in UTC nanoseconds.
    std::string asset; ///< Asset identifier.
    std::string factor; ///< Factor name; ``factor`` for single-factor evaluations.
    int direction = 1; ///< Direction sign applied to the factor value.
    double value = 0.0; ///< Direction-adjusted raw factor value.
    double processed_value = 0.0; ///< Value after direction adjustment and preprocessing.
    std::string preprocess; ///< Preprocessing method used for the panel.
    std::string group; ///< Configured group name, or ``default`` when groups are unset.
};

/// One forward-return label row.
struct research_label_row {
    timestamp date = 0; ///< Observation date in UTC nanoseconds.
    std::string asset; ///< Asset identifier.
    std::string period; ///< Forward-return period label, such as ``1D``.
    double value = 0.0; ///< Forward return for the period; NaN when missing.
};

/// One named research result table in native column form.
struct research_table {
    std::string name; ///< Table name.
    std::vector<std::string> columns; ///< Column names in row-cell order.
    std::vector<std::vector<factor_value>> rows; ///< Row-major cells aligned with ``columns``.
};

/// Validated, default-filled research evaluation specification.
///
/// Mirrors the pinned Python ``ResearchEvaluationSpec`` field set, including
/// the native UTC-nanosecond window endpoints.
struct research_evaluation_spec {
    std::vector<std::string> targets; ///< Target assets; non-empty and duplicate-free.
    timestamp start = 0; ///< Inclusive window start; must not exceed ``end``.
    timestamp end = 0; ///< Inclusive window end.
    std::string snapshot_id = "unspecified"; ///< Provenance snapshot identifier.
    std::string price_basis = "unspecified"; ///< Provenance price basis.
    std::string calendar_id = "unspecified"; ///< Provenance calendar identifier.
    std::string frequency = "unspecified"; ///< Provenance data frequency.
    std::string source_timezone = "UTC"; ///< Descriptive source timezone; timestamps remain UTC.
    std::string feed_time_kind = "close_time"; ///< Descriptive feed timestamp convention.
    std::vector<int> periods{1, 5, 20}; ///< Non-empty, positive, unique price-row horizons.
    std::string label = "next_open_to_open"; ///< Forward-return basis.
    std::string precomputed_entry = "next_open"; ///< Precomputed-label entry convention.
    int quantiles = 5; ///< Quantile bucket count; normalization requires at least one.
    int min_assets = 10; ///< Minimum asset count threshold.
    int min_quantile_assets = 1; ///< Minimum observations per quantile.
    std::string preprocess = "cross_section_rank"; ///< Cross-sectional preprocessing method.
    double max_loss = 1.0; ///< Tolerated data-loss fraction in [0, 1].
    std::vector<int> stability_lags{1, 5, 20}; ///< Positive rank-autocorrelation lags.
    std::map<std::string, int> direction_overrides; ///< Per-asset signs; values must be +1 or -1.
    std::map<std::string, std::map<std::string, double>> combinations; ///< Weighted factor blends.
    std::map<std::string, std::pair<std::string, std::string>> comparisons; ///< Named factor pairs; sides distinct.
    std::map<std::string, std::pair<std::string, std::string>> segments; ///< Named provenance ranges.
};

/// Caller-supplied values before defaulting. Unset optionals take the pinned
/// defaults; ``unknown_fields`` mirrors the source's unknown-key rejection.
struct research_evaluation_input {
    std::optional<std::vector<std::string>> targets; ///< Optional target list; normalization requires it.
    std::optional<timestamp> start; ///< Optional window start; normalization requires it.
    std::optional<timestamp> end; ///< Optional window end; normalization requires it.
    std::optional<std::string> snapshot_id; ///< Optional snapshot id; unset uses ``unspecified``.
    std::optional<std::string> price_basis; ///< Optional price basis; unset uses ``unspecified``.
    std::optional<std::string> calendar_id; ///< Optional calendar id; unset uses ``unspecified``.
    std::optional<std::string> frequency; ///< Optional frequency; unset uses ``unspecified``.
    std::optional<std::string> source_timezone; ///< Optional timezone metadata; unset uses ``UTC``.
    std::optional<std::string> feed_time_kind; ///< Optional feed kind; unset uses ``close_time``.
    std::optional<std::vector<int>> periods; ///< Optional horizons; unset uses {1, 5, 20}.
    std::optional<std::string> label; ///< Optional label basis; unset uses ``next_open_to_open``.
    std::optional<std::string> precomputed_entry; ///< Optional entry convention; unset uses ``next_open``.
    std::optional<int> quantiles; ///< Optional quantile count; unset uses 5.
    std::optional<int> min_assets; ///< Optional minimum assets; unset uses 10.
    std::optional<int> min_quantile_assets; ///< Optional minimum quantile observations; unset uses 1.
    std::optional<std::string> preprocess; ///< Optional method; unset uses ``cross_section_rank``.
    std::optional<double> max_loss; ///< Optional loss fraction; unset uses 1.0.
    std::optional<std::vector<int>> stability_lags; ///< Optional lags; unset uses {1, 5, 20}.
    std::optional<std::map<std::string, int>> direction_overrides; ///< Optional per-asset signs (+1 or -1).
    std::optional<std::map<std::string, std::map<std::string, double>>>
        combinations; ///< Optional weighted blends; unset leaves the map empty.
    std::optional<std::map<std::string, std::pair<std::string, std::string>>>
        comparisons; ///< Optional factor pairs; unset leaves the map empty.
    std::optional<std::map<std::string, std::pair<std::string, std::string>>>
        segments; ///< Optional provenance ranges; unset leaves the map empty.
    std::vector<std::string> unknown_fields; ///< Unknown keys rejected by normalization.
};

/// Validates caller values and fills every documented default.
///
/// @param values Caller-supplied optional fields and unknown keys.
/// @return Normalized evaluation spec with defaults applied.
/// @throws research_config_error When unknown fields are present or a supplied
///         value violates its field contract; ``code()`` identifies the kind
///         of failure.
research_evaluation_spec normalize_research_evaluation_spec(
    const research_evaluation_input& values);

/// Projects a normalized evaluation spec onto the execution spec.
///
/// @param values Normalized evaluation spec.
/// @return Execution spec carrying the shared fields; analysis-only metadata
///         has no execution-spec counterpart.
research_spec to_research_spec(const research_evaluation_spec& values);

/// Aggregated single-factor research result.
///
/// ``status`` is ``OK`` when every requested horizon has prepared rows,
/// ``PARTIAL`` when at least one horizon has none, and ``UNEVALUABLE`` when no
/// horizon was prepared at all.
struct research_result {
    std::string status; ///< ``OK``, ``PARTIAL`` or ``UNEVALUABLE``.
    std::vector<research_label_row> labels; ///< Forward-return rows for prepared cells.
    std::vector<research_signal_row> signals; ///< Signal rows for evaluated cells.
    factor_ic_table information_coefficient; ///< Per-date Spearman rank IC by period; NaN missing.
    factor_ic_summary information_coefficient_summary; ///< Mean rank IC per period.
    quantile_return_table quantile_returns; ///< Raw quantile return means and standard errors.
    std::vector<factor_series_result> turnover; ///< Turnover series per quantile, first horizon.
    std::vector<factor_series_result> stability; ///< Rank autocorrelation per lag, first horizon.
    std::vector<int> stability_lags; ///< Lags used for the stability series.
    std::vector<double> spread;  ///< Top-minus-bottom quantile return per period; NaN if absent.
    std::vector<std::size_t> coverage;  ///< Prepared observation count per period.
    std::vector<std::string> diagnostics; ///< Human-readable status notes.
    std::map<std::string, std::string> manifest; ///< Provenance key/value metadata.
    int schema_version = research_result_schema_version; ///< Result schema version.

    /// Returns one named result table.
    ///
    /// Unknown names raise a structured ``DatasetError``
    /// (``unknown_result_table``). The ``correlation`` and ``comparisons``
    /// tables expose canonical columns with no rows in this single-factor
    /// result.
    ///
    /// @param name Table name; one of ``research_capabilities::result_tables``.
    /// @return Table with canonical columns and row-major cells.
    /// @throws research_dataset_error When ``name`` is not a known table, with
    ///         code ``unknown_result_table``.
    research_table table(std::string_view name) const;
    /// Returns the JSON-compatible status/manifest/tables/columns payload.
    ///
    /// @return JSON text whose ``tables`` and ``columns`` objects contain all
    ///         canonical result tables.
    std::string to_serializable() const;
};

/// Named factor panel used as correlation input.
struct research_factor_panel {
    std::string name; ///< Factor name; must be non-empty and unique.
    factor_series values; ///< Factor values by (date, asset).
};

/// Correlation summary for one ordered factor pair.
struct research_correlation_row {
    std::string left; ///< Left factor name.
    std::string right; ///< Right factor name.
    double mean_correlation = 0.0; ///< Mean Pearson correlation; NaN if none.
    double std_correlation = 0.0; ///< Sample standard deviation; NaN with fewer than two dates.
    std::size_t n_dates = 0U; ///< Number of dates with a finite correlation.
};

/// Computes per-date Pearson correlations across common assets for every
/// factor pair.
///
/// Dates with fewer than ``min_assets`` common finite assets and dates with
/// zero dispersion on either side are skipped. Pairs follow input order, and
/// each row summarizes the contributing dates.
///
/// @param factors Named factor panels; names must be non-empty and unique.
/// @param min_assets Minimum common assets per date; must be positive.
/// @return One row per factor pair in input order, empty when ``factors`` is
///         empty.
/// @throws factor_data_error When ``min_assets`` is not positive or a panel
///         name is empty or duplicated.
std::vector<research_correlation_row> research_factor_correlations(
    std::span<const research_factor_panel> factors, int min_assets = 1);

/// Named single-factor result used by comparison diagnostics.
struct named_research_result {
    std::string name; ///< Factor or combination name; must be non-empty and unique.
    research_result result; ///< Stored evaluation result.
};

/// Mean-IC and ICIR comparison for one factor pair and period.
struct research_comparison_row {
    std::string comparison; ///< Comparison name from the requested map.
    std::string left; ///< Left factor name.
    std::string right; ///< Right factor name.
    std::string period; ///< Forward-return period label.
    double mean_ic = 0.0; ///< Left-minus-right mean IC; NaN when either side is unavailable.
    double icir = 0.0; ///< Left-minus-right ICIR; NaN when either side is unavailable.
    double mean_ic_left = 0.0; ///< Mean IC of the left factor; NaN when unavailable.
    double mean_ic_right = 0.0; ///< Mean IC of the right factor; NaN when unavailable.
    double icir_left = 0.0; ///< Left ICIR; NaN with too few dates or zero deviation.
    double icir_right = 0.0; ///< Right ICIR; NaN with too few dates or zero deviation.
    std::size_t n_dates_left = 0U; ///< Finite IC dates used for the left factor.
    std::size_t n_dates_right = 0U; ///< Finite IC dates used for the right factor.
};

/// Computes mean-IC and ICIR differences for named factor pairs, per period.
///
/// Period labels are the union of both stored IC label sets. Statistics use
/// finite IC values only; a difference is NaN when either side is unavailable.
///
/// @param results Named single-factor results; names must be non-empty and
///        unique.
/// @param comparisons Comparison name to (left, right) factor names.
/// @return One row per comparison and union period label.
/// @throws factor_data_error When a result name is empty or duplicated, or a
///         comparison references an unknown factor.
std::vector<research_comparison_row> research_factor_comparisons(
    std::span<const named_research_result> results,
    const std::map<std::string, std::pair<std::string, std::string>>&
        comparisons);

/// Named factor panel accepted by the multi-factor evaluator.
struct named_factor_panel {
    std::string name; ///< Factor name; must be non-empty and unique.
    factor_series values; ///< Factor values by (date, asset).
};

/// Aggregated multi-factor evaluation result.
///
/// ``status`` is ``OK`` when every named result is ``OK``, ``UNEVALUABLE``
/// when all are, and ``PARTIAL`` otherwise.
struct research_panel_result {
    std::string status; ///< Aggregated evaluation status.
    std::vector<named_research_result> factors; ///< Input factors first, then combinations.
    std::vector<research_correlation_row> correlations; ///< Pairwise processed-panel correlations.
    std::vector<research_comparison_row> comparisons; ///< Comparison rows; empty if not requested.
    std::vector<research_signal_row> signals; ///< Concatenated signals tagged with factor names.
    std::map<std::string, std::string> manifest; ///< Counts and API/schema version metadata.
};

/// Evaluates multiple named factor panels and derives correlations/comparisons.
///
/// Each panel is evaluated independently; ``spec.combinations`` are then
/// blended from the processed panels with preprocessing disabled and direction
/// overrides cleared, and the named comparisons are appended.
///
/// @param factors Named factor panels; names must be non-empty and unique.
/// @param prices Price grid used for forward returns.
/// @param spec Execution spec including combinations and comparisons.
/// @return Named results, correlations, comparisons, concatenated signals and
///         manifest counts.
/// @throws factor_data_error When names are empty or duplicated, a combination
///         or comparison references an unknown factor, weights are invalid, or
///         a panel evaluation fails.
research_panel_result evaluate_factor_research_panel(
    std::span<const named_factor_panel> factors, const dataframe& prices,
    const research_spec& spec);

/// Evaluates one factor against forward returns derived from prices.
///
/// ``spec.label`` must be ``close_to_close`` or ``next_open_to_open``; the
/// precomputed basis is served by the forward-return-table overload. Period
/// labels are derived from the price grid. ``close_to_close`` labels span
/// ``period`` price rows from each factor date; ``next_open_to_open`` labels
/// enter one row later and span ``period`` rows from that entry.
///
/// @param factors Factor values by (date, asset).
/// @param prices Price dataframe with asset columns and a UTC timestamp index.
/// @param spec Execution spec; its inclusive window and target guards are
///        applied before evaluation.
/// @return Aggregated result with ``OK``, ``PARTIAL`` or ``UNEVALUABLE``
///         status.
/// @throws factor_data_error When the spec, factor panel or price grid
///         violates the evaluated contract, including unsupported labels,
///         missing targets, an empty window, duplicates or malformed prices.
research_result evaluate_factor_research(
    const factor_series& factors, const dataframe& prices,
    const research_spec& spec);

/// Evaluates one factor against price-derived returns under a wide mask.
///
/// The boolean mask keeps only cells whose (date, asset) entry is true, and
/// the returned result records ``universe=applied`` in its manifest.
///
/// @param factors Factor values by (date, asset).
/// @param prices Price dataframe with asset columns and a UTC timestamp index.
/// @param spec Execution spec; the precomputed label basis is rejected.
/// @param universe Wide boolean eligibility mask (dates x assets).
/// @return Aggregated result for the masked panel.
/// @throws factor_data_error When the mask contains non-boolean columns,
///         duplicate dates or removes every cell, or the underlying
///         evaluation fails.
research_result evaluate_factor_research(
    const factor_series& factors, const dataframe& prices,
    const research_spec& spec, const dataframe& universe);

/// Evaluates one factor against caller-supplied forward returns.
///
/// ``spec.label`` must not be ``next_open_to_open``; the result manifest
/// records the ``precomputed`` label.
///
/// @param factors Factor values by (date, asset).
/// @param precomputed_returns Label-major forward-return table covering the
///        requested periods.
/// @param spec Execution spec selecting the periods and quantiles.
/// @return Aggregated result for the supplied labels.
/// @throws factor_data_error When the table is empty, malformed, has duplicate
///         keys, misses a requested period or yields no prepared rows, or the
///         factor panel violates its contract.
research_result evaluate_factor_research(
    const factor_series& factors,
    const forward_return_table& precomputed_returns,
    const research_spec& spec);

/// Files written by export_factor_research().
struct research_export_result {
    std::string directory; ///< Requested output directory as supplied.
    std::vector<std::string> files; ///< Relative names of written files, in write order.
};

/// Writes deterministic CSV tables and a JSON manifest without overwriting.
///
/// A preexisting destination is rejected and left untouched. A directory
/// created by this call is removed if a later write fails. ``charts=true``
/// requests raster chart export, which the native library does not provide;
/// it fails closed instead of silently skipping charts.
///
/// @param result Result tables to export.
/// @param directory Fresh destination directory.
/// @param charts Whether to request charts; the native library rejects true.
/// @return The requested directory and the relative names of the files
///         written.
/// @throws factor_data_error When charts are requested, the result tables are
///         malformed, the manifest uses reserved keys ``status`` or
///         ``exports``, the directory is empty, contains a null byte or
///         already exists, or a file cannot be written.
research_export_result export_factor_research(
    const research_result& result, const std::string& directory,
    bool charts = false);

/// Returns the native value form used by fingerprints and JSON envelopes.
///
/// @param result Research result to serialize.
/// @return Object with ``status``, ``schema_version``, ``manifest``,
///         ``tables`` and ``columns``; non-finite numbers become null.
factor_value serializable_value(const research_result& result);


namespace detail {

/// Provides a benchmark-only entry to the window/target validation and
/// filtering stage.
///
/// @param factors Factor values by (date, asset).
/// @param spec Execution spec supplying the window, targets and preprocessing.
/// @return Panel restricted to the window and targets, with direction signs
///         and preprocessing applied.
/// @throws factor_data_error When panel validation fails, as in the
///         evaluators.
factor_series research_panel_for_benchmark(
    const factor_series& factors, const research_spec& spec);

}  // namespace detail

}  // namespace fincore
