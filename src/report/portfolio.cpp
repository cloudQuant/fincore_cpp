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
#include <fincore/portfolio/transactions.hpp>

#include <cmath>
#include <cstdint>
#include <map>
#include <set>
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

// The two curves handle a missing return differently on purpose: this one treats
// it as no growth and still emits the timestamp, so the reported path stays
// continuous, while drawdown_curve below multiplies the raw value and lets a NaN
// appear in the series for whoever renders it.
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

// Uniqueness is again judged on neighbouring entries, so an out-of-order index
// that repeats a date further along passes and the later map build keeps only the
// last row for that date; this is the second instance of the pattern registered as
// F10-D11.
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

constexpr std::int64_t day_ns = 86'400'000'000'000LL;

report_section transactions_section(
    std::span<const portfolio_transaction> transactions) {
    report_section section;
    section.key = "transactions";
    section.title = "Transactions";
    std::map<timestamp, std::pair<std::size_t, double>> by_day;
    std::set<std::string> symbols;
    for (const auto& transaction : transactions) {
        if (!std::isfinite(transaction.amount) ||
            !std::isfinite(transaction.price)) {
            throw input_error{
                "transactions must contain finite amount and price values"};
        }
        timestamp day = transaction.date / day_ns;
        if (transaction.date % day_ns < 0) {
            --day;
        }
        day *= day_ns;
        auto& entry = by_day[day];
        ++entry.first;
        entry.second += std::abs(transaction.amount) * transaction.price;
        symbols.insert(transaction.symbol);
    }
    double total_count = 0.0;
    double total_notional = 0.0;
    for (const auto& [day, entry] : by_day) {
        total_count += static_cast<double>(entry.first);
        total_notional += entry.second;
    }
    const auto day_count = static_cast<double>(by_day.size());
    section.metrics = {
        number_metric("transaction_count", "Transaction count", "count",
                      static_cast<double>(transactions.size())),
        number_metric("trading_days", "Trading days", "count", day_count),
        number_metric("average_daily_transactions",
                      "Average daily transactions", "count",
                      day_count > 0.0 ? total_count / day_count
                                      : std::nan("")),
        number_metric("average_daily_notional", "Average daily notional",
                      "notional",
                      day_count > 0.0 ? total_notional / day_count
                                      : std::nan("")),
        number_metric("symbol_count", "Symbol count", "count",
                      static_cast<double>(symbols.size()))};

    auto daily_count = make_series("daily_transaction_count", "Trades",
                                   "count", "count");
    auto daily_notional = make_series("daily_transaction_notional",
                                      "Notional", "notional", "notional");
    for (const auto& [day, entry] : by_day) {
        daily_count.x.push_back(static_cast<double>(day));
        daily_count.y.push_back(static_cast<double>(entry.first));
        daily_notional.x.push_back(static_cast<double>(day));
        daily_notional.y.push_back(entry.second);
    }
    section.series.push_back(std::move(daily_count));
    section.series.push_back(std::move(daily_notional));
    return section;
}

report_section portfolio_section(
    const dataframe& positions,
    const std::optional<dataframe>& turnover_frame) {
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
    if (turnover_frame.has_value()) {
        const auto& turnover_values =
            require_double_column(*turnover_frame, "turnover");
        double total_turnover = 0.0;
        std::size_t turnover_count = 0U;
        for (const double value : turnover_values) {
            if (std::isfinite(value)) {
                total_turnover += value;
                ++turnover_count;
            }
        }
        section.metrics.push_back(number_metric(
            "average_turnover", "Average turnover", "ratio",
            turnover_count > 0U
                ? total_turnover / static_cast<double>(turnover_count)
                : std::nan("")));
        section.series.push_back(frame_series(*turnover_frame, "turnover",
                                              "Turnover", "turnover",
                                              "ratio"));
    }
    return section;
}

}  // namespace

report_document build_portfolio_report(const dataframe& returns,
                                       const dataframe* benchmark,
                                       std::string title,
                                       const dataframe* positions,
                                       const std::size_t rolling_window,
                                       const std::span<const portfolio_transaction>
                                           transactions) {
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
        const auto aligned = align_positions(returns, *positions);
        std::optional<dataframe> turnover_frame;
        if (!transactions.empty()) {
            std::map<timestamp, double> daily_volume;
            for (const auto& transaction : transactions) {
                timestamp day = transaction.date / day_ns;
                if (transaction.date % day_ns < 0) {
                    --day;
                }
                daily_volume[day * day_ns] +=
                    std::abs(transaction.amount) * transaction.price;
            }
            dataframe ledger;
            std::vector<timestamp> days;
            std::vector<double> amounts;
            for (const auto& [day, volume] : daily_volume) {
                days.push_back(day);
                amounts.push_back(volume);
            }
            ledger.load_index(std::move(days));
            ledger.load_column<double>("amount", std::move(amounts),
                                       hmdf::nan_policy::dont_pad_with_nans);
            ledger.load_column<double>(
                "price", std::vector<double>(daily_volume.size(), 1.0),
                hmdf::nan_policy::dont_pad_with_nans);
            turnover_frame = get_turnover(aligned, ledger);
        }
        document.sections.push_back(
            portfolio_section(aligned, turnover_frame));
    }
    if (!transactions.empty()) {
        document.sections.push_back(transactions_section(transactions));
    }
    return document;
}

}  // namespace fincore
