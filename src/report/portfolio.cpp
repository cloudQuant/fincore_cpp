#include <fincore/report/portfolio.hpp>

#include <fincore/detail/json.hpp>
#include <fincore/detail/numeric.hpp>
#include <fincore/metrics/alpha_beta.hpp>
#include <fincore/metrics/drawdown.hpp>
#include <fincore/metrics/ratios.hpp>
#include <fincore/metrics/returns.hpp>
#include <fincore/metrics/risk.hpp>

#include <cmath>
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

report_series cumulative_series(std::string key, std::string title,
                                const dataframe& frame,
                                const std::string_view column) {
    report_series series;
    series.key = std::move(key);
    series.title = std::move(title);
    series.x_label = "timestamp";
    series.y_label = "growth_multiple";
    series.unit = "growth_multiple";
    const auto values = require_double_column(frame, column);
    double growth = 1.0;
    for (std::size_t index = 0; index < values.size(); ++index) {
        growth *= 1.0 + values[index];
        series.x.push_back(static_cast<double>(frame.get_index()[index]));
        series.y.push_back(growth);
    }
    return series;
}

}  // namespace

report_document build_portfolio_report(const dataframe& returns,
                                       const dataframe* benchmark,
                                       std::string title) {
    report_document document;
    document.domain = "portfolio";
    document.title = std::move(title);
    document.metadata = {
        {"observations",
         std::to_string(returns.get_index().size())},
        {"period", "daily"}};

    report_section performance;
    performance.key = "performance";
    performance.title = "Performance";
    performance.metrics = {
        number_metric("annual_return", "Annual return", "ratio",
                      annual_return(returns)),
        number_metric("cumulative_return", "Cumulative return", "ratio",
                      cumulative_return(returns)),
        number_metric("annual_volatility", "Annual volatility", "ratio",
                      annual_volatility(returns)),
        number_metric("sharpe_ratio", "Sharpe ratio", "ratio",
                      sharpe_ratio(returns)),
        number_metric("sortino_ratio", "Sortino ratio", "ratio",
                      sortino_ratio(returns)),
        number_metric("calmar_ratio", "Calmar ratio", "ratio",
                      calmar_ratio(returns)),
        number_metric("stability", "Stability", "r_squared",
                      stability_of_timeseries(returns)),
        number_metric("max_drawdown", "Max drawdown", "ratio",
                      max_drawdown(returns)),
        number_metric("omega_ratio", "Omega ratio", "ratio",
                      omega_ratio(returns)),
        number_metric("tail_ratio", "Tail ratio", "ratio",
                      tail_ratio(returns)),
        number_metric("value_at_risk", "Historical VaR", "return",
                      historical_value_at_risk(returns))};
    performance.series.push_back(
        cumulative_series("cumulative_returns", "Cumulative returns", returns,
                          returns_column));
    performance.notes.push_back(
        "Metrics are computed once from the supplied daily returns; the "
        "report does not recalculate them.");
    document.sections.push_back(std::move(performance));

    if (benchmark != nullptr) {
        report_section section;
        section.key = "benchmark";
        section.title = "Benchmark comparison";
        const double up = up_capture(returns, *benchmark);
        const double down = down_capture(returns, *benchmark);
        const auto aligned = align_on_common_index(
            returns, returns_column, *benchmark, benchmark_column);
        const auto strategy_values =
            require_double_column(aligned.first, returns_column);
        const auto benchmark_values =
            require_double_column(aligned.second, benchmark_column);
        std::vector<double> active;
        active.reserve(strategy_values.size());
        for (std::size_t index = 0; index < strategy_values.size(); ++index) {
            active.push_back(strategy_values[index] -
                             benchmark_values[index]);
        }
        const double tracking_error =
            std::sqrt(detail::sample_variance(active)) *
            std::sqrt(daily_annualization);
        section.metrics = {
            number_metric("information_ratio", "Information ratio", "ratio",
                          information_ratio(returns, *benchmark)),
            number_metric("tracking_error", "Tracking error", "ratio",
                          tracking_error),
            number_metric("up_capture", "Up capture", "ratio", up),
            number_metric("down_capture", "Down capture", "ratio", down),
            number_metric("capture_ratio", "Capture ratio", "ratio",
                          std::isfinite(down) && down != 0.0
                              ? up / down
                              : std::nan("")),
            number_metric("beta", "Beta", "ratio",
                          beta(returns, *benchmark))};
        section.series.push_back(cumulative_series(
            "benchmark_cumulative_returns", "Benchmark cumulative returns",
            *benchmark, benchmark_column));
        document.sections.push_back(std::move(section));
    }
    return document;
}

}  // namespace fincore
