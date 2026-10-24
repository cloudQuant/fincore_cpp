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

inline constexpr const char* research_api_version = "1.0";
inline constexpr int research_result_schema_version = 1;
inline constexpr int EVALUATION_CONFIG_SCHEMA_VERSION = 1;
inline constexpr int evaluation_config_schema_version =
    EVALUATION_CONFIG_SCHEMA_VERSION;

/// Native structured research errors. Python exception inheritance is not
/// reproduced; ``category`` preserves the source class name and ``code``
/// carries the stable machine-readable identifier.
class research_error : public fincore_error {
public:
    research_error(std::string category, std::string code,
                   const std::string& message,
                   std::map<std::string, std::string> report = {})
        : fincore_error(make_error(std::move(category), std::move(code),
                                   message, {}, std::move(report))) {}
};

class research_config_error : public research_error {
public:
    static constexpr const char* default_category = "ConfigError";
    static constexpr const char* default_code = "invalid_config";
    explicit research_config_error(const std::string& message,
                                   std::string code = default_code,
                                   std::map<std::string, std::string> report = {})
        : research_error(default_category, std::move(code), message,
                         std::move(report)) {}
};

class research_dataset_error : public research_error {
public:
    static constexpr const char* default_category = "DatasetError";
    static constexpr const char* default_code = "invalid_dataset";
    explicit research_dataset_error(const std::string& message,
                                    std::string code = default_code,
                                    std::map<std::string, std::string> report = {})
        : research_error(default_category, std::move(code), message,
                         std::move(report)) {}
};

class research_dependency_error : public research_error {
public:
    static constexpr const char* default_category = "DependencyError";
    static constexpr const char* default_code = "missing_dependency";
    explicit research_dependency_error(const std::string& message,
                                       std::string code = default_code,
                                       std::map<std::string, std::string> report = {})
        : research_error(default_category, std::move(code), message,
                         std::move(report)) {}
};

class research_data_quality_error : public research_error {
public:
    static constexpr const char* default_category = "DataQualityError";
    static constexpr const char* default_code = "data_quality";
    explicit research_data_quality_error(const std::string& message,
                                         std::string code = default_code,
                                         std::map<std::string, std::string> report = {})
        : research_error(default_category, std::move(code), message,
                         std::move(report)) {}
};

class research_factor_computation_error : public research_error {
public:
    static constexpr const char* default_category = "FactorComputationError";
    static constexpr const char* default_code = "factor_computation_failed";
    explicit research_factor_computation_error(
        const std::string& message, std::string code = default_code,
        std::map<std::string, std::string> report = {})
        : research_error(default_category, std::move(code), message,
                         std::move(report)) {}
};

struct research_capabilities {
    std::string api_version = research_api_version;
    int result_schema_version = research_result_schema_version;
    std::vector<std::string> labels{"close_to_close", "next_open_to_open",
                                    "precomputed"};
    std::vector<std::string> preprocess{"cross_section_rank",
                                        "cross_section_zscore", "none"};
    std::vector<std::string> metrics{
        "pearson_ic", "spearman_ic", "icir", "quantile_returns", "spread",
        "rank_autocorrelation", "quantile_turnover", "factor_correlation",
        "paired_comparison"};
    std::vector<std::string> result_tables{
        "signals", "labels", "ic", "ic_summary", "quantile_returns",
        "turnover", "spreads", "stability", "correlation", "comparisons",
        "coverage", "diagnostics"};
    std::vector<std::string> export_formats{"json", "csv"};
    std::vector<std::string> statuses{"OK", "PARTIAL", "UNEVALUABLE"};
};

research_capabilities research_capabilities_report();

struct research_spec {
    std::vector<std::string> targets;
    timestamp start = 0;
    timestamp end = 0;
    std::vector<int> periods{1, 5, 20};
    int quantiles = 5;
    int min_assets = 1;
    int min_quantile_assets = 1;
    std::string label = "close_to_close";
    std::string preprocess = "cross_section_rank";
    double max_loss = 1.0;
    std::vector<int> stability_lags{1, 5, 20};
    std::map<std::string, int> direction_overrides;
    std::map<std::string, std::pair<std::string, std::string>> segments;
    std::map<std::string, std::pair<std::string, std::string>> comparisons;
    std::map<std::string, std::map<std::string, double>> combinations;
    std::map<std::string, std::string> groups;
    std::string snapshot_id = "unspecified";
    std::string price_basis = "unspecified";
    std::string calendar_id = "unspecified";
    std::string frequency = "unspecified";
};

struct research_signal_row {
    timestamp date = 0;
    std::string asset;
    std::string factor;
    int direction = 1;
    double value = 0.0;
    double processed_value = 0.0;
    std::string preprocess;
    std::string group;
};

struct research_label_row {
    timestamp date = 0;
    std::string asset;
    std::string period;
    double value = 0.0;
};

/// One named research result table in native column form.
struct research_table {
    std::string name;
    std::vector<std::string> columns;
    std::vector<std::vector<factor_value>> rows;
};

/// Validated, default-filled research evaluation specification.
///
/// Mirrors the pinned Python ``ResearchEvaluationSpec`` field set, including
/// the native UTC-nanosecond window endpoints.
struct research_evaluation_spec {
    std::vector<std::string> targets;
    timestamp start = 0;
    timestamp end = 0;
    std::string snapshot_id = "unspecified";
    std::string price_basis = "unspecified";
    std::string calendar_id = "unspecified";
    std::string frequency = "unspecified";
    std::string source_timezone = "UTC";
    std::string feed_time_kind = "close_time";
    std::vector<int> periods{1, 5, 20};
    std::string label = "next_open_to_open";
    std::string precomputed_entry = "next_open";
    int quantiles = 5;
    int min_assets = 10;
    int min_quantile_assets = 1;
    std::string preprocess = "cross_section_rank";
    double max_loss = 1.0;
    std::vector<int> stability_lags{1, 5, 20};
    std::map<std::string, int> direction_overrides;
    std::map<std::string, std::map<std::string, double>> combinations;
    std::map<std::string, std::pair<std::string, std::string>> comparisons;
    std::map<std::string, std::pair<std::string, std::string>> segments;
};

/// Caller-supplied values before defaulting. Unset optionals take the pinned
/// defaults; ``unknown_fields`` mirrors the source's unknown-key rejection.
struct research_evaluation_input {
    std::optional<std::vector<std::string>> targets;
    std::optional<timestamp> start;
    std::optional<timestamp> end;
    std::optional<std::string> snapshot_id;
    std::optional<std::string> price_basis;
    std::optional<std::string> calendar_id;
    std::optional<std::string> frequency;
    std::optional<std::string> source_timezone;
    std::optional<std::string> feed_time_kind;
    std::optional<std::vector<int>> periods;
    std::optional<std::string> label;
    std::optional<std::string> precomputed_entry;
    std::optional<int> quantiles;
    std::optional<int> min_assets;
    std::optional<int> min_quantile_assets;
    std::optional<std::string> preprocess;
    std::optional<double> max_loss;
    std::optional<std::vector<int>> stability_lags;
    std::optional<std::map<std::string, int>> direction_overrides;
    std::optional<std::map<std::string, std::map<std::string, double>>>
        combinations;
    std::optional<std::map<std::string, std::pair<std::string, std::string>>>
        comparisons;
    std::optional<std::map<std::string, std::pair<std::string, std::string>>>
        segments;
    std::vector<std::string> unknown_fields;
};

/// Validate caller values and fill every documented default.
research_evaluation_spec normalize_research_evaluation_spec(
    const research_evaluation_input& values);

/// Project a normalized evaluation spec onto the execution spec.
research_spec to_research_spec(const research_evaluation_spec& values);

struct research_result {
    std::string status;
    std::vector<research_label_row> labels;
    std::vector<research_signal_row> signals;
    factor_ic_table information_coefficient;
    factor_ic_summary information_coefficient_summary;
    quantile_return_table quantile_returns;
    std::vector<factor_series_result> turnover;
    std::vector<factor_series_result> stability;
    std::vector<int> stability_lags;
    std::vector<double> spread;  // periods
    std::vector<std::size_t> coverage;  // periods
    std::vector<std::string> diagnostics;
    std::map<std::string, std::string> manifest;
    int schema_version = research_result_schema_version;

    /// Return one named result table. Unknown names raise a structured
    /// ``DatasetError`` (``unknown_result_table``).
    research_table table(std::string_view name) const;
    /// JSON-compatible status/manifest/tables/columns payload.
    std::string to_serializable() const;
};

struct research_factor_panel {
    std::string name;
    factor_series values;
};

struct research_correlation_row {
    std::string left;
    std::string right;
    double mean_correlation = 0.0;
    double std_correlation = 0.0;
    std::size_t n_dates = 0U;
};

/// Per-date Pearson correlation across common assets for every factor pair.
std::vector<research_correlation_row> research_factor_correlations(
    std::span<const research_factor_panel> factors, int min_assets = 1);

struct named_research_result {
    std::string name;
    research_result result;
};

struct research_comparison_row {
    std::string comparison;
    std::string left;
    std::string right;
    std::string period;
    double mean_ic = 0.0;
    double icir = 0.0;
    double mean_ic_left = 0.0;
    double mean_ic_right = 0.0;
    double icir_left = 0.0;
    double icir_right = 0.0;
    std::size_t n_dates_left = 0U;
    std::size_t n_dates_right = 0U;
};

/// Mean-IC/ICIR differences for explicitly named factor pairs, per period.
std::vector<research_comparison_row> research_factor_comparisons(
    std::span<const named_research_result> results,
    const std::map<std::string, std::pair<std::string, std::string>>&
        comparisons);

struct named_factor_panel {
    std::string name;
    factor_series values;
};

struct research_panel_result {
    std::string status;
    std::vector<named_research_result> factors;
    std::vector<research_correlation_row> correlations;
    std::vector<research_comparison_row> comparisons;
    std::vector<research_signal_row> signals;
    std::map<std::string, std::string> manifest;
};

/// Evaluate multiple named factor panels and derive correlations/comparisons.
research_panel_result evaluate_factor_research_panel(
    std::span<const named_factor_panel> factors, const dataframe& prices,
    const research_spec& spec);

research_result evaluate_factor_research(
    const factor_series& factors, const dataframe& prices,
    const research_spec& spec);

/// Evaluate with a wide boolean eligibility mask (dates x assets).
research_result evaluate_factor_research(
    const factor_series& factors, const dataframe& prices,
    const research_spec& spec, const dataframe& universe);

/// Evaluate against caller-supplied forward returns instead of prices.
research_result evaluate_factor_research(
    const factor_series& factors,
    const forward_return_table& precomputed_returns,
    const research_spec& spec);

struct research_export_result {
    std::string directory;
    std::vector<std::string> files;
};

/// Write deterministic CSV tables and a JSON manifest without overwriting.
///
/// ``charts=true`` requests raster chart export, which the native library
/// does not provide; it fails closed instead of silently skipping charts.
research_export_result export_factor_research(
    const research_result& result, const std::string& directory,
    bool charts = false);

/// Native value form used by fingerprints and JSON envelopes.
factor_value serializable_value(const research_result& result);


namespace detail {

/// Benchmark-only entry to the window/target validation and filtering stage.
factor_series research_panel_for_benchmark(
    const factor_series& factors, const research_spec& spec);

}  // namespace detail

}  // namespace fincore
