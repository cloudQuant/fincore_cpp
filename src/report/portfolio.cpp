#include <fincore/report/portfolio.hpp>

#include <fincore/detail/json.hpp>
#include <fincore/detail/numeric.hpp>
#include <fincore/metrics/alpha_beta.hpp>
#include <fincore/metrics/drawdown.hpp>
#include <fincore/metrics/ratios.hpp>
#include <fincore/metrics/returns.hpp>
#include <fincore/metrics/risk.hpp>
#include <fincore/metrics/rolling.hpp>
#include <fincore/portfolio/positions.hpp>

#include <cmath>
#include <map>
#include <string>
#include <typeindex>
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

report_series make_series(std::string key, std::string title,
                          std::string y_label, std::string unit) {
    report_series series;
    series.key = std::move(key);
    series.title = std::move(title);
    series.x_label = "timestamp";
    series.y_label = std::move(y_label);
    series.unit = std::move(unit);
    return series;
}

report_cell numeric_cell(const double value) {
    if (!std::isfinite(value)) {
        return report_cell::missing();
    }
    return report_cell::number_cell(value);
}

report_series cumulative_series(std::string key, std::string title,
                                const dataframe& frame,
                                const std::string_view column) {
    auto series =
        make_series(std::move(key), std::move(title), "growth_multiple",
                    "growth_multiple");
    const auto values = require_double_column(frame, column);
    double growth = 1.0;
    for (std::size_t index = 0; index < values.size(); ++index) {
        growth *= 1.0 + (std::isfinite(values[index]) ? values[index] : 0.0);
        series.x.push_back(static_cast<double>(frame.get_index()[index]));
        series.y.push_back(growth);
    }
    return series;
}

// Project the first genuine double column of a value-owned frame.
report_series frame_series(const dataframe& frame, std::string key,
                           std::string title, std::string y_label,
                           std::string unit) {
    auto series =
        make_series(std::move(key), std::move(title), std::move(y_label),
                    std::move(unit));
    for (const auto& info : frame.get_columns_info<double>()) {
        if (std::get<2>(info) != typeid(double)) {
            continue;
        }
        const std::string name{std::get<0>(info).c_str()};
        const auto& values = frame.get_column<double>(name.c_str());
        for (std::size_t index = 0; index < values.size(); ++index) {
            if (!std::isfinite(values[index])) {
                continue;
            }
            series.x.push_back(static_cast<double>(frame.get_index()[index]));
            series.y.push_back(values[index]);
        }
        break;
    }
    return series;
}

report_series drawdown_curve(const dataframe& returns) {
    auto series = make_series("drawdown", "Drawdown", "drawdown",
                              "decimal_return");
    const auto values = require_double_column(returns, returns_column);
    double growth = 1.0;
    double peak = 1.0;
    for (std::size_t index = 0; index < values.size(); ++index) {
        growth *= 1.0 + values[index];
        peak = std::max(peak, growth);
        series.x.push_back(static_cast<double>(returns.get_index()[index]));
        series.y.push_back(peak > 0.0 ? growth / peak - 1.0 : 0.0);
    }
    return series;
}

dataframe align_positions(const dataframe& returns,
                          const dataframe& positions) {
    if (positions.get_index().empty()) {
        throw input_error{"positions must contain at least one row"};
    }
    if (std::adjacent_find(positions.get_index().begin(),
                           positions.get_index().end()) !=
        positions.get_index().end()) {
        throw input_error{"positions index must be unique"};
    }
    static_cast<void>(require_double_column(positions, cash_column));
    std::map<timestamp, std::size_t> by_date;
    for (std::size_t index = 0; index < positions.get_index().size(); ++index) {
        by_date[positions.get_index()[index]] = index;
    }
    std::vector<std::string> names;
    std::vector<const double*> columns;
    for (const auto& info : positions.get_columns_info<double>()) {
        if (std::get<2>(info) != typeid(double)) {
            continue;
        }
        const std::string name{std::get<0>(info).c_str()};
        const auto& values = positions.get_column<double>(name.c_str());
        for (const double value : values) {
            if (!std::isfinite(value)) {
                throw input_error{
                    "positions must contain only finite values"};
            }
        }
        names.push_back(name);
        columns.push_back(values.data());
    }
    dataframe aligned;
    std::vector<timestamp> index = returns.get_index();
    aligned.load_index(std::move(index));
    for (std::size_t column = 0; column < names.size(); ++column) {
        std::vector<double> values;
        values.reserve(returns.get_index().size());
        for (const timestamp date : returns.get_index()) {
            const auto found = by_date.find(date);
            if (found == by_date.end()) {
                throw input_error{
                    "positions must cover every return timestamp"};
            }
            values.push_back(columns[column][found->second]);
        }
        aligned.load_column<double>(names[column].c_str(), std::move(values),
                                    hmdf::nan_policy::dont_pad_with_nans);
    }
    return aligned;
}

report_section portfolio_section(const dataframe& positions) {
    report_section section;
    section.key = "portfolio";
    section.title = "Portfolio exposure";
    std::size_t asset_count = 0U;
    for (const auto& info : positions.get_columns_info<double>()) {
        if (std::get<2>(info) == typeid(double) &&
            std::string{std::get<0>(info).c_str()} != cash_column) {
            ++asset_count;
        }
    }
    const auto leverage_frame = gross_lev(positions);
    const auto leverage = require_double_column(leverage_frame, "gross_lev");
    double total = 0.0;
    double maximum = 0.0;
    std::size_t finite = 0U;
    for (const double value : leverage) {
        if (std::isfinite(value)) {
            total += value;
            maximum = std::max(maximum, value);
            ++finite;
        }
    }
    section.metrics = {
        number_metric("asset_count", "Asset count", "count",
                      static_cast<double>(asset_count)),
        number_metric("average_gross_leverage", "Average gross leverage",
                      "ratio",
                      finite > 0U
                          ? total / static_cast<double>(finite)
                          : std::nan("")),
        number_metric("maximum_gross_leverage", "Maximum gross leverage",
                      "ratio", maximum)};
    section.series.push_back(frame_series(leverage_frame, "gross_leverage",
                                          "Gross leverage", "leverage",
                                          "ratio"));
    return section;
}

}  // namespace

report_document build_portfolio_report(const dataframe& returns,
                                       const dataframe* benchmark,
                                       std::string title,
                                       const dataframe* positions,
                                       const std::size_t rolling_window) {
    if (rolling_window == 0U) {
        throw input_error{"rolling_window must be positive"};
    }
    report_document document;
    document.domain = "portfolio";
    document.title = std::move(title);
    document.metadata = {
        {"observations", std::to_string(returns.get_index().size())},
        {"period", "daily"},
        {"rolling_window", std::to_string(rolling_window)}};

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
        number_metric("calmar_ratio", "Calmar ratio", "ratio",
                      calmar_ratio(returns)),
        number_metric("stability", "Stability", "r_squared",
                      stability_of_timeseries(returns)),
        number_metric("max_drawdown", "Max drawdown", "ratio",
                      max_drawdown(returns)),
        number_metric("omega_ratio", "Omega ratio", "ratio",
                      omega_ratio(returns)),
        number_metric("sortino_ratio", "Sortino ratio", "ratio",
                      sortino_ratio(returns)),
        number_metric("tail_ratio", "Tail ratio", "ratio",
                      tail_ratio(returns)),
        number_metric("value_at_risk", "Historical VaR", "return",
                      historical_value_at_risk(returns))};

    auto returns_series =
        make_series("returns", "Strategy return", "return", "decimal_return");
    {
        const auto values = require_double_column(returns, returns_column);
        for (std::size_t index = 0; index < values.size(); ++index) {
            returns_series.x.push_back(
                static_cast<double>(returns.get_index()[index]));
            returns_series.y.push_back(values[index]);
        }
    }
    performance.series.push_back(std::move(returns_series));
    performance.series.push_back(
        cumulative_series("cumulative_returns", "Cumulative returns", returns,
                          returns_column));
    performance.series.push_back(drawdown_curve(returns));
    performance.series.push_back(frame_series(
        roll_sharpe_ratio(returns, rolling_window), "rolling_sharpe",
        "Rolling Sharpe", "sharpe", "ratio"));
    performance.series.push_back(frame_series(
        roll_annual_volatility(returns, rolling_window),
        "rolling_volatility", "Rolling volatility", "volatility",
        "annualized_decimal_return"));
    performance.series.push_back(frame_series(
        aggregate_returns(returns, return_aggregation_period::monthly),
        "monthly_returns", "Monthly return", "return", "decimal_return"));

    report_table drawdowns;
    drawdowns.key = "drawdowns";
    drawdowns.title = "Top drawdowns";
    drawdowns.columns = {"rank", "net_drawdown", "peak", "valley",
                         "recovery", "duration_business_days"};
    const auto windows = gen_drawdown_table(returns, 5U);
    for (std::size_t index = 0; index < windows.size(); ++index) {
        const auto& window = windows[index];
        std::vector<report_cell> row;
        row.push_back(report_cell::number_cell(
            static_cast<double>(index + 1U)));
        row.push_back(numeric_cell(window.net_drawdown_percent));
        row.push_back(window.peak.has_value()
                          ? report_cell::text_cell(detail::format_timestamp(
                                *window.peak))
                          : report_cell::missing());
        row.push_back(window.valley.has_value()
                          ? report_cell::text_cell(detail::format_timestamp(
                                *window.valley))
                          : report_cell::missing());
        row.push_back(window.recovery.has_value()
                          ? report_cell::text_cell(detail::format_timestamp(
                                *window.recovery))
                          : report_cell::missing());
        row.push_back(window.duration_business_days.has_value()
                          ? numeric_cell(static_cast<double>(
                                *window.duration_business_days))
                          : report_cell::missing());
        drawdowns.rows.push_back(std::move(row));
    }
    performance.tables.push_back(std::move(drawdowns));
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
            number_metric("alpha", "Alpha", "ratio",
                          alpha(returns, *benchmark)),
            number_metric("beta", "Beta", "ratio",
                          beta(returns, *benchmark))};
        section.series.push_back(cumulative_series(
            "benchmark_cumulative_returns", "Benchmark cumulative returns",
            *benchmark, benchmark_column));
        section.series.push_back(frame_series(
            rolling_beta(returns, *benchmark, rolling_window), "rolling_beta",
            "Strategy beta", "beta", "beta"));
        document.sections.push_back(std::move(section));
    }
    if (positions != nullptr) {
        document.sections.push_back(
            portfolio_section(align_positions(returns, *positions)));
    }
    return document;
}

}  // namespace fincore
