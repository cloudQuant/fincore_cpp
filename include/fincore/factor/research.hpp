#pragma once

#include <fincore/factor/data.hpp>
#include <fincore/factor/performance.hpp>

#include <map>
#include <string>
#include <vector>

namespace fincore {

inline constexpr const char* research_api_version = "1.0";
inline constexpr int research_result_schema_version = 1;

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
    std::string preprocess = "cross_section_rank";
    double max_loss = 1.0;
    std::vector<int> stability_lags{1, 5, 20};
    std::map<std::string, int> direction_overrides;
    std::string snapshot_id = "unspecified";
    std::string price_basis = "unspecified";
    std::string calendar_id = "unspecified";
    std::string frequency = "unspecified";
};

struct research_result {
    std::string status;
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
};

research_result evaluate_factor_research(
    const factor_series& factors, const dataframe& prices,
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

}  // namespace fincore
