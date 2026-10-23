#include <fincore/factor/report.hpp>

#include <fincore/detail/json.hpp>
#include <fincore/factor/performance.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace fincore {
namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();

report_metric number_metric(std::string key, std::string label,
                            std::string unit, const double value) {
    report_metric metric;
    metric.key = std::move(key);
    metric.label = std::move(label);
    metric.unit = std::move(unit);
    if (std::isfinite(value)) {
        metric.number = value;
    }
    return metric;
}

report_cell numeric_cell(const double value) {
    if (!std::isfinite(value)) {
        return {};
    }
    return report_cell::number_cell(value);
}

report_cell count_cell(const std::size_t value) {
    return report_cell::number_cell(static_cast<double>(value));
}

double mean_of(const std::vector<double>& values) {
    double total = 0.0;
    std::size_t count = 0U;
    for (const double value : values) {
        if (std::isfinite(value)) {
            total += value;
            ++count;
        }
    }
    return count > 0U ? total / static_cast<double>(count) : nan;
}

double sample_std_of(const std::vector<double>& values) {
    const double mean = mean_of(values);
    if (!std::isfinite(mean)) {
        return nan;
    }
    double variance = 0.0;
    std::size_t count = 0U;
    for (const double value : values) {
        if (std::isfinite(value)) {
            variance += (value - mean) * (value - mean);
            ++count;
        }
    }
    return count > 1U
               ? std::sqrt(variance / static_cast<double>(count - 1U))
               : nan;
}

double population_skew(const std::vector<double>& values) {
    std::vector<double> finite;
    for (const double value : values) {
        if (std::isfinite(value)) {
            finite.push_back(value);
        }
    }
    if (finite.size() < 2U) {
        return nan;
    }
    const double mean = mean_of(finite);
    double second = 0.0;
    double third = 0.0;
    for (const double value : finite) {
        const double delta = value - mean;
        second += delta * delta;
        third += delta * delta * delta;
    }
    const auto count = static_cast<double>(finite.size());
    second /= count;
    third /= count;
    if (second <= 0.0) {
        return nan;
    }
    return third / std::pow(second, 1.5);
}

// scipy.stats.kurtosis(..., fisher=True, bias=True).
double population_excess_kurtosis(const std::vector<double>& values) {
    std::vector<double> finite;
    for (const double value : values) {
        if (std::isfinite(value)) {
            finite.push_back(value);
        }
    }
    if (finite.size() < 2U) {
        return nan;
    }
    const double mean = mean_of(finite);
    double second = 0.0;
    double fourth = 0.0;
    for (const double value : finite) {
        const double delta = value - mean;
        second += delta * delta;
        fourth += delta * delta * delta * delta;
    }
    const auto count = static_cast<double>(finite.size());
    second /= count;
    fourth /= count;
    if (second <= 0.0) {
        return nan;
    }
    return fourth / (second * second) - 3.0;
}

report_table make_table(std::string key, std::string title,
                        std::vector<std::string> columns) {
    report_table table;
    table.key = std::move(key);
    table.title = std::move(title);
    table.columns = std::move(columns);
    return table;
}

void append_summary_metrics(report_section& summary,
                            const factor_analysis_model& model) {
    summary.metrics.push_back(number_metric(
        "forward_period_count", "Forward period count", "count",
        static_cast<double>(model.forward_periods.size())));
    for (std::size_t index = 0; index < model.forward_periods.size();
         ++index) {
        if (index >= model.mean_information_coefficient.means.size()) {
            break;
        }
        summary.metrics.push_back(number_metric(
            "mean_information_coefficient." + model.forward_periods[index],
            "Mean IC " + model.forward_periods[index], "correlation",
            model.mean_information_coefficient.means[index]));
    }
    std::size_t finite_factors = 0U;
    std::set<std::string> assets;
    std::set<timestamp> dates;
    std::vector<std::size_t> forward_counts(model.forward_periods.size(), 0U);
    for (const auto& row : model.factor_data.rows) {
        assets.insert(row.asset);
        dates.insert(row.date);
        if (std::isfinite(row.factor)) {
            ++finite_factors;
        }
        for (std::size_t label = 0;
             label < model.forward_periods.size() &&
             label < row.forward_returns.size();
             ++label) {
            if (std::isfinite(row.forward_returns[label])) {
                ++forward_counts[label];
            }
        }
    }
    const auto observations = model.factor_data.rows.size();
    summary.metrics.push_back(number_metric(
        "observation_count", "Observation count", "rows",
        static_cast<double>(observations)));
    summary.metrics.push_back(number_metric(
        "finite_factor_count", "Finite factor count", "rows",
        static_cast<double>(finite_factors)));
    summary.metrics.push_back(number_metric(
        "coverage_ratio", "Coverage ratio", "fraction",
        observations > 0U
            ? static_cast<double>(finite_factors) /
                  static_cast<double>(observations)
            : nan));
    summary.metrics.push_back(number_metric(
        "quantile_count", "Quantile count", "count",
        static_cast<double>(model.mean_returns_by_quantile.quantiles.size())));
    for (std::size_t label = 0; label < model.forward_periods.size();
         ++label) {
        summary.metrics.push_back(number_metric(
            "forward_return_count." + model.forward_periods[label],
            "Forward return count " + model.forward_periods[label], "rows",
            static_cast<double>(forward_counts[label])));
    }
}

void append_quantile_statistics(report_section& summary,
                                const factor_analysis_model& model) {
    auto table = make_table("quantile_statistics", "Quantile statistics",
                            {"quantile", "min", "max", "mean", "std", "count",
                             "count %"});
    for (const auto& statistics : model.quantile_statistics) {
        table.rows.push_back(
            {report_cell::number_cell(
                 static_cast<double>(statistics.quantile)),
             numeric_cell(statistics.minimum),
             numeric_cell(statistics.maximum),
             numeric_cell(statistics.mean),
             numeric_cell(statistics.stddev),
             count_cell(statistics.count),
             numeric_cell(statistics.count_percent)});
    }
    summary.tables.push_back(std::move(table));
}

void append_alpha_beta(report_section& section,
                       const factor_analysis_model& model) {
    auto table = make_table("alpha_beta", "Alpha and beta", {"measure"});
    for (const auto& entry : model.alpha_beta) {
        table.columns.push_back(entry.label);
    }
    std::vector<report_cell> alpha_row{report_cell::text_cell("Ann. alpha")};
    std::vector<report_cell> beta_row{report_cell::text_cell("beta")};
    for (const auto& entry : model.alpha_beta) {
        alpha_row.push_back(numeric_cell(entry.annual_alpha));
        beta_row.push_back(numeric_cell(entry.beta));
    }
    table.rows.push_back(std::move(alpha_row));
    table.rows.push_back(std::move(beta_row));
    section.tables.push_back(std::move(table));
}

void append_mean_returns(report_section& section,
                         const factor_analysis_model& model) {
    auto table = make_table("mean_returns_by_quantile",
                            "Mean returns by quantile", {"quantile"});
    for (const auto& label : model.mean_returns_by_quantile.labels) {
        table.columns.push_back(label);
    }
    for (std::size_t quantile = 0;
         quantile < model.mean_returns_by_quantile.quantiles.size();
         ++quantile) {
        std::vector<report_cell> row;
        row.push_back(report_cell::number_cell(static_cast<double>(
            model.mean_returns_by_quantile.quantiles[quantile])));
        for (std::size_t label = 0;
             label < model.mean_returns_by_quantile.labels.size(); ++label) {
            row.push_back(numeric_cell(
                model.mean_returns_by_quantile.means
                    [quantile * model.mean_returns_by_quantile.labels.size() +
                     label]));
        }
        table.rows.push_back(std::move(row));
    }
    section.tables.push_back(std::move(table));
}

void append_information_coefficient_table(report_section& summary,
                                          const factor_analysis_model& model) {
    auto table = make_table("information_coefficient",
                            "Information coefficient", {"date"});
    for (const auto& label : model.information_coefficient.labels) {
        table.columns.push_back(label);
    }
    for (std::size_t date = 0;
         date < model.information_coefficient.dates.size(); ++date) {
        std::vector<report_cell> row;
        row.push_back(report_cell::text_cell(detail::format_timestamp(
            model.information_coefficient.dates[date])));
        for (std::size_t label = 0;
             label < model.information_coefficient.labels.size(); ++label) {
            row.push_back(numeric_cell(
                model.information_coefficient.values
                    [date * model.information_coefficient.labels.size() +
                     label]));
        }
        table.rows.push_back(std::move(row));
    }
    summary.tables.push_back(std::move(table));
}

report_series line_series(std::string key, std::string title,
                          std::string unit, std::string y_label,
                          const std::vector<timestamp>& dates,
                          const std::vector<double>& values) {
    report_series series;
    series.key = std::move(key);
    series.title = std::move(title);
    series.x_label = "timestamp";
    series.y_label = std::move(y_label);
    series.unit = std::move(unit);
    for (std::size_t index = 0;
         index < dates.size() && index < values.size(); ++index) {
        if (!std::isfinite(values[index])) {
            continue;
        }
        series.x.push_back(static_cast<double>(dates[index]));
        series.y.push_back(values[index]);
    }
    return series;
}

std::vector<double> information_column(const factor_analysis_model& model,
                                       const std::size_t label) {
    std::vector<double> values;
    const std::size_t labels = model.information_coefficient.labels.size();
    for (std::size_t date = 0;
         date < model.information_coefficient.dates.size(); ++date) {
        values.push_back(
            model.information_coefficient.values[date * labels + label]);
    }
    return values;
}

void append_information_section(report_document& document,
                                const factor_analysis_model& model) {
    report_section section;
    section.key = "factor_information";
    section.title = "Information coefficient";
    auto table = make_table("information_summary", "Information summary",
                            {"period", "IC Mean", "IC Std.",
                             "Risk-Adjusted IC", "t-stat(IC)", "p-value(IC)",
                             "IC Skew", "IC Kurtosis"});
    for (std::size_t label = 0;
         label < model.information_coefficient.labels.size(); ++label) {
        const auto values = information_column(model, label);
        const double mean = mean_of(values);
        const double deviation = sample_std_of(values);
        std::size_t observations = 0U;
        for (const double value : values) {
            if (std::isfinite(value)) {
                ++observations;
            }
        }
        const double t_statistic =
            observations > 1U && std::isfinite(deviation) && deviation > 0.0
                ? mean / (deviation /
                          std::sqrt(static_cast<double>(observations)))
                : nan;
        const double p_value =
            std::isfinite(t_statistic)
                ? student_t_two_sided_p(
                      t_statistic, static_cast<double>(observations - 1U))
                : nan;
        table.rows.push_back(
            {report_cell::text_cell(
                 model.information_coefficient.labels[label]),
             numeric_cell(mean), numeric_cell(deviation),
             numeric_cell(mean / deviation), numeric_cell(t_statistic),
             numeric_cell(p_value), numeric_cell(population_skew(values)),
             numeric_cell(population_excess_kurtosis(values))});
        section.series.push_back(line_series(
            "ic." + model.information_coefficient.labels[label],
            model.information_coefficient.labels[label] + " IC",
            "correlation", "IC", model.information_coefficient.dates,
            values));
    }
    section.tables.push_back(std::move(table));
    document.sections.push_back(std::move(section));
}

void append_returns_section(report_document& document,
                            const factor_analysis_model& model) {
    report_section section;
    section.key = "factor_returns";
    section.title = "Factor returns";

    auto summary =
        make_table("returns_summary", "Returns summary", {"measure"});
    for (const auto& label : model.forward_periods) {
        summary.columns.push_back(label);
    }
    std::vector<double> top(model.mean_returns_by_quantile.labels.size(), nan);
    std::vector<double> bottom(top.size(), nan);
    if (!model.mean_returns_by_quantile.quantiles.empty()) {
        const std::size_t quantiles =
            model.mean_returns_by_quantile.quantiles.size();
        const std::size_t labels =
            model.mean_returns_by_quantile.labels.size();
        for (std::size_t label = 0; label < labels; ++label) {
            top[label] = model.mean_returns_by_quantile.means
                             [(quantiles - 1U) * labels + label] *
                         10'000.0;
            bottom[label] =
                model.mean_returns_by_quantile.means[label] * 10'000.0;
        }
    }
    std::vector<report_cell> alpha_row{report_cell::text_cell("Ann. alpha")};
    std::vector<report_cell> beta_row{report_cell::text_cell("beta")};
    std::vector<report_cell> top_row{report_cell::text_cell(
        "Mean Period Wise Return Top Quantile (bps)")};
    std::vector<report_cell> bottom_row{report_cell::text_cell(
        "Mean Period Wise Return Bottom Quantile (bps)")};
    std::vector<report_cell> spread_row{report_cell::text_cell(
        "Mean Period Wise Spread (bps)")};
    for (std::size_t index = 0; index < model.forward_periods.size();
         ++index) {
        const double annual_alpha = index < model.alpha_beta.size()
                                        ? model.alpha_beta[index].annual_alpha
                                        : nan;
        const double beta =
            index < model.alpha_beta.size() ? model.alpha_beta[index].beta
                                            : nan;
        double spread = nan;
        if (index < model.mean_return_spread.size()) {
            spread = model.mean_return_spread[index].spread * 10'000.0;
        }
        alpha_row.push_back(numeric_cell(annual_alpha));
        beta_row.push_back(numeric_cell(beta));
        top_row.push_back(numeric_cell(index < top.size() ? top[index] : nan));
        bottom_row.push_back(
            numeric_cell(index < bottom.size() ? bottom[index] : nan));
        spread_row.push_back(numeric_cell(spread));
    }
    summary.rows = {std::move(alpha_row), std::move(beta_row),
                    std::move(top_row), std::move(bottom_row),
                    std::move(spread_row)};
    section.tables.push_back(std::move(summary));
    append_alpha_beta(section, model);
    append_mean_returns(section, model);
    document.sections.push_back(std::move(section));
}

void append_turnover_section(report_document& document,
                             const factor_analysis_model& model) {
    report_section section;
    section.key = "factor_turnover";
    section.title = "Turnover and autocorrelation";

    std::vector<int> lags;
    for (const auto& turnover : model.quantile_turnover) {
        if (std::find(lags.begin(), lags.end(), turnover.lag) == lags.end()) {
            lags.push_back(turnover.lag);
        }
    }
    for (const auto& autocorrelation : model.rank_autocorrelation) {
        if (std::find(lags.begin(), lags.end(), autocorrelation.lag) ==
            lags.end()) {
            lags.push_back(autocorrelation.lag);
        }
    }
    std::sort(lags.begin(), lags.end());
    std::set<int> quantiles;
    for (const auto& turnover : model.quantile_turnover) {
        quantiles.insert(turnover.quantile);
    }

    auto mean_turnover = make_table(
        "mean_turnover_by_quantile", "Mean turnover by quantile",
        {"factor_quantile"});
    auto turnover_summary =
        make_table("turnover_summary", "Turnover summary", {"measure"});
    auto autocorrelation_summary = make_table(
        "rank_autocorrelation_summary", "Rank autocorrelation summary",
        {"measure"});
    for (const int lag : lags) {
        const std::string column = std::to_string(lag) + "D";
        mean_turnover.columns.push_back(column);
        turnover_summary.columns.push_back(column);
        autocorrelation_summary.columns.push_back(column);
    }
    for (const int quantile : quantiles) {
        std::vector<report_cell> row{
            report_cell::number_cell(static_cast<double>(quantile))};
        std::vector<report_cell> summary_row{report_cell::text_cell(
            "Quantile " + std::to_string(quantile) + " Mean Turnover ")};
        for (const int lag : lags) {
            double mean = nan;
            for (const auto& turnover : model.quantile_turnover) {
                if (turnover.lag == lag && turnover.quantile == quantile) {
                    mean = mean_of(turnover.values.values);
                    break;
                }
            }
            row.push_back(numeric_cell(mean));
            summary_row.push_back(numeric_cell(mean));
        }
        mean_turnover.rows.push_back(std::move(row));
        turnover_summary.rows.push_back(std::move(summary_row));
    }
    std::vector<report_cell> autocorrelation_row{report_cell::text_cell(
        "Mean Factor Rank Autocorrelation")};
    for (const int lag : lags) {
        double mean = nan;
        for (const auto& autocorrelation : model.rank_autocorrelation) {
            if (autocorrelation.lag == lag) {
                mean = mean_of(autocorrelation.values.values);
                break;
            }
        }
        autocorrelation_row.push_back(numeric_cell(mean));
    }
    autocorrelation_summary.rows.push_back(std::move(autocorrelation_row));

    section.tables.push_back(std::move(mean_turnover));
    section.tables.push_back(std::move(autocorrelation_summary));
    section.tables.push_back(std::move(turnover_summary));
    document.sections.push_back(std::move(section));
}

}  // namespace

report_document build_factor_report(const factor_analysis_model& model,
                                    std::string title) {
    report_document document;
    document.domain = "factor";
    document.title = std::move(title);
    std::set<std::string> assets;
    std::set<timestamp> dates;
    for (const auto& row : model.factor_data.rows) {
        assets.insert(row.asset);
        dates.insert(row.date);
    }
    std::string periods;
    for (std::size_t index = 0; index < model.forward_periods.size();
         ++index) {
        if (index > 0U) {
            periods += ",";
        }
        periods += model.forward_periods[index];
    }
    document.metadata = {
        {"factor_result_fingerprint", model.result_fingerprint},
        {"forward_periods", periods},
        {"asset_count", std::to_string(assets.size())},
        {"date_count", std::to_string(dates.size())},
        {"start", dates.empty() ? std::string{}
                                : detail::format_timestamp(*dates.begin())},
        {"end", dates.empty() ? std::string{}
                              : detail::format_timestamp(*dates.rbegin())}};

    report_section summary;
    summary.key = "factor_summary";
    summary.title = "Factor summary";
    append_summary_metrics(summary, model);
    append_quantile_statistics(summary, model);
    append_alpha_beta(summary, model);
    append_mean_returns(summary, model);
    append_information_coefficient_table(summary, model);
    for (const auto& curve : model.factor_cumulative_returns) {
        summary.series.push_back(line_series(
            "factor_cumulative_returns." + curve.period,
            "Factor " + curve.period, "growth_multiple", "growth",
            curve.values.dates, curve.values.values));
    }
    document.sections.push_back(std::move(summary));

    append_information_section(document, model);
    append_returns_section(document, model);
    append_turnover_section(document, model);

    for (auto& section : document.sections) {
        if (section.key == "factor_summary" ||
            section.key == "factor_returns") {
            section.notes.push_back(
                "Values are computed from the prepared factor panel; the "
                "report does not recalculate metrics.");
            section.notes.push_back(
                "Reference research output, not investment advice.");
        }
    }
    return document;
}

}  // namespace fincore
