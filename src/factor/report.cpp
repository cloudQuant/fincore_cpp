#include <fincore/factor/report.hpp>

#include <fincore/detail/json.hpp>

#include <cmath>
#include <utility>
#include <string>
#include <vector>

namespace fincore {
namespace {

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

}  // namespace

report_document build_factor_report(const factor_analysis_model& model,
                                    std::string title) {
    report_document document;
    document.domain = "factor";
    document.title = std::move(title);
    document.metadata = {
        {"fingerprint", model.result_fingerprint},
        {"observations", std::to_string(model.factor_data.rows.size())},
        {"periods", std::to_string(model.forward_periods.size())}};

    report_section summary;
    summary.key = "factor_summary";
    summary.title = "Factor summary";
    summary.metrics.push_back(number_metric(
        "observations", "Prepared observations", "rows",
        static_cast<double>(model.factor_data.rows.size())));
    summary.metrics.push_back(number_metric(
        "periods", "Forward periods", "count",
        static_cast<double>(model.forward_periods.size())));
    summary.metrics.push_back(number_metric(
        "quantiles", "Quantiles", "count",
        static_cast<double>(model.mean_returns_by_quantile.quantiles.size())));
    for (std::size_t index = 0; index < model.forward_periods.size();
         ++index) {
        const std::string& period = model.forward_periods[index];
        if (index < model.mean_information_coefficient.means.size()) {
            summary.metrics.push_back(number_metric(
                "mean_ic." + period, "Mean IC " + period, "correlation",
                model.mean_information_coefficient.means[index]));
        }
        if (index < model.alpha_beta.size()) {
            summary.metrics.push_back(number_metric(
                "annual_alpha." + period, "Annual alpha " + period, "ratio",
                model.alpha_beta[index].annual_alpha));
            summary.metrics.push_back(number_metric(
                "beta." + period, "Beta " + period, "ratio",
                model.alpha_beta[index].beta));
        }
    }
    for (std::size_t index = 0; index < model.quantile_turnover.size();
         ++index) {
        double total = 0.0;
        std::size_t count = 0U;
        for (const double value :
             model.quantile_turnover[index].values) {
            if (std::isfinite(value)) {
                total += value;
                ++count;
            }
        }
        if (count > 0U) {
            summary.metrics.push_back(number_metric(
                "mean_turnover.q" + std::to_string(index + 1U),
                "Mean turnover q" + std::to_string(index + 1U), "fraction",
                total / static_cast<double>(count)));
        }
    }
    document.sections.push_back(summary);

    report_section performance;
    performance.key = "factor_performance";
    performance.title = "Factor performance";

    report_table ic_table;
    ic_table.key = "information_coefficient";
    ic_table.title = "Information coefficient";
    ic_table.columns.push_back("date");
    for (const auto& label : model.information_coefficient.labels) {
        ic_table.columns.push_back(label);
    }
    for (std::size_t date = 0; date < model.information_coefficient.dates.size();
         ++date) {
        std::vector<report_cell> row;
        row.push_back(report_cell::text_cell(fincore::detail::format_timestamp(
            model.information_coefficient.dates[date])));
        for (std::size_t label = 0;
             label < model.information_coefficient.labels.size(); ++label) {
            row.push_back(numeric_cell(
                model.information_coefficient.values[date * model
                                                          .information_coefficient
                                                          .labels.size() +
                                                     label]));
        }
        ic_table.rows.push_back(std::move(row));
    }
    performance.tables.push_back(std::move(ic_table));

    report_table quantile_table;
    quantile_table.key = "quantile_returns";
    quantile_table.title = "Mean returns by quantile";
    quantile_table.columns.push_back("quantile");
    for (const auto& label : model.mean_returns_by_quantile.labels) {
        quantile_table.columns.push_back(label);
    }
    for (std::size_t quantile = 0;
         quantile < model.mean_returns_by_quantile.quantiles.size();
         ++quantile) {
        std::vector<report_cell> row;
        row.push_back(report_cell::number_cell(
            static_cast<double>(model.mean_returns_by_quantile
                                    .quantiles[quantile])));
        for (std::size_t label = 0;
             label < model.mean_returns_by_quantile.labels.size(); ++label) {
            row.push_back(numeric_cell(
                model.mean_returns_by_quantile.means
                    [quantile * model.mean_returns_by_quantile.labels.size() +
                     label]));
        }
        quantile_table.rows.push_back(std::move(row));
    }
    performance.tables.push_back(std::move(quantile_table));

    report_table alpha_table;
    alpha_table.key = "alpha_beta";
    alpha_table.title = "Alpha and beta";
    alpha_table.columns = {"period", "annual_alpha", "beta"};
    for (const auto& entry : model.alpha_beta) {
        alpha_table.rows.push_back(
            {report_cell::text_cell(entry.label),
             numeric_cell(entry.annual_alpha),
             numeric_cell(entry.beta)});
    }
    performance.tables.push_back(std::move(alpha_table));

    report_table turnover_table;
    turnover_table.key = "turnover";
    turnover_table.title = "Quantile turnover";
    turnover_table.columns.push_back("date");
    for (std::size_t index = 0; index < model.quantile_turnover.size();
         ++index) {
        turnover_table.columns.push_back("q" + std::to_string(index + 1U));
    }
    if (!model.quantile_turnover.empty()) {
        const auto& first = model.quantile_turnover.front();
        for (std::size_t date = 0; date < first.dates.size(); ++date) {
            std::vector<report_cell> row;
            row.push_back(report_cell::text_cell(
                fincore::detail::format_timestamp(first.dates[date])));
            for (const auto& turnover : model.quantile_turnover) {
                row.push_back(numeric_cell(turnover.values[date]));
            }
            turnover_table.rows.push_back(std::move(row));
        }
    }
    performance.tables.push_back(std::move(turnover_table));

    for (std::size_t label = 0; label < model.factor_returns.labels.size();
         ++label) {
        report_series series;
        series.key = "factor_return." + model.factor_returns.labels[label];
        series.title = "Factor return " + model.factor_returns.labels[label];
        series.x_label = "timestamp";
        series.y_label = "return";
        series.unit = "simple_return";
        for (std::size_t date = 0; date < model.factor_returns.dates.size();
             ++date) {
            const double value =
                model.factor_returns.values[date * model.factor_returns.labels
                                                        .size() +
                                            label];
            if (!std::isfinite(value)) {
                continue;
            }
            series.x.push_back(static_cast<double>(
                model.factor_returns.dates[date]));
            series.y.push_back(value);
        }
        performance.series.push_back(std::move(series));
    }
    performance.notes.push_back(
        "Values are computed from the prepared factor panel; the report does "
        "not recalculate metrics.");
    performance.notes.push_back(
        "Reference research output, not investment advice.");
    document.sections.push_back(std::move(performance));
    return document;
}

}  // namespace fincore
