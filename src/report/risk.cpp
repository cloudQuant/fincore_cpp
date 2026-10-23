#include <fincore/report/risk.hpp>

#include <fincore/detail/json.hpp>

#include <cmath>
#include <string>
#include <utility>
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

report_metric text_metric(std::string key, std::string label,
                          std::string value) {
    report_metric metric;
    metric.key = std::move(key);
    metric.label = std::move(label);
    metric.text = std::move(value);
    return metric;
}

report_cell numeric_cell(const double value) {
    if (!std::isfinite(value)) {
        return {};
    }
    return report_cell::number_cell(value);
}

report_cell timestamp_cell(const timestamp value) {
    return report_cell::text_cell(detail::format_timestamp(value));
}

}  // namespace

report_document build_risk_report(const risk_validation_report& report,
                                  std::string title) {
    report_document document;
    document.domain = "risk";
    document.title = std::move(title);
    document.metadata = {
        {"inputs_digest", report.inputs_digest},
        {"schema_version", std::to_string(report.schema_version)}};

    report_section section;
    section.key = "risk_validation";
    section.title = "Risk validation";
    section.metrics.push_back(text_metric(
        "status", "Status",
        std::string(walk_forward_status_name(report.status))));
    section.metrics.push_back(number_metric(
        "forecast_event_count", "Forecast events", "count",
        static_cast<double>(report.forecast_events.size())));
    section.metrics.push_back(number_metric(
        "refit_count", "Refits", "count",
        static_cast<double>(report.refits.size())));
    if (!report.diagnostics.method.empty()) {
        section.metrics.push_back(
            text_metric("diagnostics.method", "Method",
                        report.diagnostics.method));
        section.metrics.push_back(text_metric(
            "diagnostics.quantile_method", "Quantile method",
            report.diagnostics.quantile_method));
        section.metrics.push_back(number_metric(
            "diagnostics.window", "Window", "observations",
            static_cast<double>(report.diagnostics.window)));
        section.metrics.push_back(number_metric(
            "diagnostics.refit_cadence", "Refit cadence", "observations",
            static_cast<double>(report.diagnostics.refit_cadence)));
        section.metrics.push_back(text_metric(
            "diagnostics.backtest_status", "Backtest status",
            report.diagnostics.backtest_status));
    }
    if (!report.diagnostics.reason.empty()) {
        section.metrics.push_back(
            text_metric("diagnostics.reason", "Reason",
                        report.diagnostics.reason));
    }

    report_table events;
    events.key = "forecast_events";
    events.title = "Forecast events";
    events.columns = {"timestamp", "forecast", "realized", "exception",
                      "refit"};
    for (const auto& event : report.forecast_events) {
        events.rows.push_back(
            {timestamp_cell(event.date), numeric_cell(event.forecast),
             numeric_cell(event.realized),
             report_cell::boolean_cell(event.exception),
             report_cell::boolean_cell(event.refit)});
    }
    section.tables.push_back(std::move(events));

    report_table refits;
    refits.key = "refits";
    refits.title = "Refits";
    if (report.specification.distribution == "normal") {
        refits.columns = {"timestamp", "forecast", "mean",
                          "standard_deviation", "n_observations"};
    } else {
        refits.columns = {"timestamp", "forecast", "n_observations",
                          "quantile_method"};
    }
    for (const auto& refit : report.refits) {
        if (report.specification.distribution == "normal") {
            refits.rows.push_back(
                {timestamp_cell(refit.date), numeric_cell(refit.forecast),
                 numeric_cell(refit.mean),
                 numeric_cell(refit.standard_deviation),
                 report_cell::number_cell(
                     static_cast<double>(refit.n_observations))});
        } else {
            refits.rows.push_back(
                {timestamp_cell(refit.date), numeric_cell(refit.forecast),
                 report_cell::number_cell(
                     static_cast<double>(refit.n_observations)),
                 report_cell::text_cell("weibull")});
        }
    }
    section.tables.push_back(std::move(refits));

    if (report.has_backtest) {
        const auto& backtest = report.backtest;
        section.metrics.push_back(number_metric(
            "backtest.observations", "Backtest observations", "count",
            static_cast<double>(backtest.observations)));
        section.metrics.push_back(number_metric(
            "backtest.exceptions", "Backtest exceptions", "count",
            static_cast<double>(backtest.exceptions)));
        section.metrics.push_back(number_metric(
            "backtest.expected_exceptions", "Expected exceptions", "count",
            backtest.expected_exceptions));
        section.metrics.push_back(number_metric(
            "backtest.exception_rate", "Exception rate", "fraction",
            backtest.exception_rate));
        section.metrics.push_back(number_metric(
            "backtest.kupiec_lr", "Kupiec LR", "statistic", backtest.kupiec_lr));
        section.metrics.push_back(number_metric(
            "backtest.kupiec_pvalue", "Kupiec p-value", "probability",
            backtest.kupiec_pvalue));
        section.metrics.push_back(number_metric(
            "backtest.christoffersen_lr", "Christoffersen LR", "statistic",
            backtest.christoffersen_lr));
        section.metrics.push_back(number_metric(
            "backtest.christoffersen_pvalue", "Christoffersen p-value",
            "probability", backtest.christoffersen_pvalue));
        section.metrics.push_back(number_metric(
            "backtest.confidence_level", "Backtest confidence", "probability",
            backtest.confidence_level));
        section.metrics.push_back(text_metric(
            "backtest.status", "Backtest status",
            std::string(risk_backtest_status_name(backtest.status))));
        section.metrics.push_back(text_metric(
            "backtest.traffic_light", "Basel zone",
            report.traffic_light_zone));
    }
    section.notes.push_back(report.disclosure);
    document.sections.push_back(std::move(section));
    return document;
}

}  // namespace fincore
