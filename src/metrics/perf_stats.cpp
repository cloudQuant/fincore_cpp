#include <fincore/metrics/perf_stats.hpp>

#include <fincore/metrics/alpha_beta.hpp>
#include <fincore/metrics/drawdown.hpp>
#include <fincore/metrics/ratios.hpp>
#include <fincore/metrics/returns.hpp>
#include <fincore/metrics/risk.hpp>
#include <fincore/metrics/stats.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace fincore {
namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();

double linear_percentile(const std::vector<double>& sorted,
                         const double percentile) {
    if (sorted.empty()) {
        return nan;
    }
    const double virtual_index =
        static_cast<double>(sorted.size() - 1) * (percentile / 100.0);
    const auto lower = static_cast<std::size_t>(std::floor(virtual_index));
    const auto upper = static_cast<std::size_t>(std::ceil(virtual_index));
    if (lower == upper) {
        return sorted[lower];
    }
    const double difference = sorted[upper] - sorted[lower];
    const double fraction = virtual_index - static_cast<double>(lower);
    return fraction < 0.5
               ? sorted[lower] + difference * fraction
               : sorted[upper] - difference * (1.0 - fraction);
}

double bootstrap_statistic(
    const std::vector<double>& values,
    const std::string_view name,
    const double annualization,
    const std::string_view column) {
    const auto returns = make_series(
        [&values] {
            std::vector<timestamp> index(values.size());
            for (std::size_t position = 0; position < values.size();
                 ++position) {
                index[position] = static_cast<timestamp>(position);
            }
            return index;
        }(),
        column, values);
    if (name == "Annual return") {
        return annual_return(returns, annualization, column);
    }
    if (name == "Cumulative returns") {
        return cumulative_return(returns, column);
    }
    if (name == "Annual volatility") {
        return annual_volatility(returns, annualization, column);
    }
    if (name == "Sharpe ratio") {
        return sharpe_ratio(returns, 0.0, annualization, column);
    }
    if (name == "Calmar ratio") {
        return calmar_ratio(returns, 0.0, annualization, column);
    }
    if (name == "Stability") {
        return stability_of_timeseries(returns, column);
    }
    if (name == "Max drawdown") {
        return max_drawdown(returns, column);
    }
    if (name == "Omega ratio") {
        return omega_ratio(returns, 0.0, 0.0, daily_annualization, column);
    }
    if (name == "Sortino ratio") {
        return sortino_ratio(returns, 0.0, annualization, column);
    }
    if (name == "Skew") {
        return skewness(returns, column);
    }
    if (name == "Kurtosis") {
        return kurtosis(returns, column);
    }
    if (name == "Tail ratio") {
        return tail_ratio(returns, column);
    }
    if (name == "Daily value at risk") {
        return historical_value_at_risk(returns, 0.05, column);
    }
    return nan;
}

const std::vector<const char*>& bootstrap_metric_names() {
    static const std::vector<const char*> names{
        "Annual return",       "Cumulative returns", "Annual volatility",
        "Sharpe ratio",        "Calmar ratio",       "Stability",
        "Max drawdown",        "Omega ratio",        "Sortino ratio",
        "Skew",                "Kurtosis",           "Tail ratio",
        "Daily value at risk"};
    return names;
}

std::vector<bootstrap_metric_stats> make_bootstrap_metric_results(
    const std::size_t sample_count) {
    std::vector<bootstrap_metric_stats> result;
    result.reserve(bootstrap_metric_names().size());
    for (const char* name : bootstrap_metric_names()) {
        result.push_back({name, {}, {}});
        result.back().samples.reserve(sample_count);
    }
    return result;
}

void append_bootstrap_metric_sample(
    std::vector<bootstrap_metric_stats>& result,
    const std::vector<double>& sample,
    const double annualization,
    const std::string_view column) {
    for (auto& metric : result) {
        metric.samples.push_back(
            bootstrap_statistic(sample, metric.name, annualization, column));
    }
}

void finish_bootstrap_metric_results(
    std::vector<bootstrap_metric_stats>& result) {
    for (auto& metric : result) {
        metric.summary = calc_distribution_stats(metric.samples);
    }
}

void validate_bootstrap_draws(
    const std::span<const std::vector<std::size_t>> draw_indices,
    const std::size_t population_size) {
    for (const auto& draw : draw_indices) {
        if (draw.size() != population_size) {
            throw input_error{
                "performance bootstrap draw length must match the input population"};
        }
        for (const std::size_t index : draw) {
            if (index >= population_size) {
                throw input_error{
                    "performance bootstrap draw index is outside the input population"};
            }
        }
    }
}

std::map<std::string, double> unary_statistics(
    const dataframe& returns,
    const double annualization,
    const std::string_view returns_column_name) {
    return {
        {"Annual return",
         annual_return(returns, annualization, returns_column_name)},
        {"Cumulative returns", cumulative_return(returns, returns_column_name)},
        {"Annual volatility",
         annual_volatility(returns, annualization, returns_column_name)},
        {"Sharpe ratio",
         sharpe_ratio(returns, 0.0, annualization, returns_column_name)},
        {"Calmar ratio",
         calmar_ratio(returns, 0.0, annualization, returns_column_name)},
        {"Stability", stability_of_timeseries(returns, returns_column_name)},
        {"Max drawdown", max_drawdown(returns, returns_column_name)},
        {"Omega ratio",
         omega_ratio(returns, 0.0, 0.0, daily_annualization,
                     returns_column_name)},
        {"Sortino ratio",
         sortino_ratio(returns, 0.0, annualization, returns_column_name)},
        {"Skew", skewness(returns, returns_column_name)},
        {"Kurtosis", kurtosis(returns, returns_column_name)},
        {"Tail ratio", tail_ratio(returns, returns_column_name)},
        {"Daily value at risk",
         historical_value_at_risk(returns, 0.05, returns_column_name)},
    };
}

}  // namespace

std::map<std::string, double> perf_stats(
    const dataframe& returns,
    const frequency period,
    const std::string_view returns_column_name) {
    static_cast<void>(require_double_column(returns, returns_column_name));
    const double annualization = annualization_factor(period);
    return unary_statistics(returns, annualization, returns_column_name);
}

std::map<std::string, double> perf_stats(
    const dataframe& returns,
    const dataframe& benchmark,
    const frequency period,
    const std::string_view returns_column_name,
    const std::string_view benchmark_column_name) {
    static_cast<void>(require_double_column(returns, returns_column_name));
    static_cast<void>(require_double_column(benchmark, benchmark_column_name));
    const double annualization = annualization_factor(period);

    auto result = unary_statistics(returns, annualization, returns_column_name);
    const auto [alpha_value, beta_value] = alpha_beta(
        returns, benchmark, 0.0, daily_annualization, returns_column_name,
        benchmark_column_name);
    result.emplace("Alpha", alpha_value);
    result.emplace("Beta", beta_value);
    return result;
}

distribution_stats calc_distribution_stats(
    const std::span<const double> values) {
    std::vector<double> sorted(values.begin(), values.end());
    std::sort(sorted.begin(), sorted.end());
    double total = 0.0;
    for (const double value : sorted) {
        total += value;
    }
    const double mean =
        sorted.empty() ? nan : total / static_cast<double>(sorted.size());
    double squared_total = 0.0;
    for (const double value : sorted) {
        const double deviation = value - mean;
        squared_total += deviation * deviation;
    }
    const double standard_deviation =
        sorted.empty()
            ? nan
            : std::sqrt(squared_total / static_cast<double>(sorted.size()));
    const double p25 = linear_percentile(sorted, 25.0);
    const double p75 = linear_percentile(sorted, 75.0);
    return {mean,
            linear_percentile(sorted, 50.0),
            standard_deviation,
            linear_percentile(sorted, 5.0),
            p25,
            p75,
            linear_percentile(sorted, 95.0),
            p75 - p25};
}

std::vector<bootstrap_metric_stats> perf_stats_bootstrap(
    const dataframe& returns,
    const std::size_t n_samples,
    const std::uint64_t seed,
    const std::string_view column) {
    if (column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    const auto values = require_double_column(returns, column);
    const double annualization = daily_annualization;
    auto result = make_bootstrap_metric_results(n_samples);
    if (values.empty()) {
        finish_bootstrap_metric_results(result);
        return result;
    }

    std::mt19937_64 engine{seed};
    std::uniform_int_distribution<std::size_t> pick(
        0, values.size() - 1);
    std::vector<double> resampled(values.size());
    for (std::size_t sample = 0; sample < n_samples; ++sample) {
        for (std::size_t position = 0; position < values.size(); ++position) {
            resampled[position] = values[pick(engine)];
        }
        append_bootstrap_metric_sample(result, resampled, annualization, column);
    }
    finish_bootstrap_metric_results(result);
    return result;
}

std::vector<bootstrap_metric_stats> perf_stats_bootstrap(
    const dataframe& returns,
    const std::span<const std::vector<std::size_t>> draw_indices,
    const std::string_view column) {
    if (column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    const auto values = require_double_column(returns, column);
    if (values.empty() && !draw_indices.empty()) {
        throw input_error{
            "performance bootstrap cannot draw from an empty input population"};
    }
    validate_bootstrap_draws(draw_indices, values.size());

    auto result = make_bootstrap_metric_results(draw_indices.size());
    std::vector<double> resampled(values.size());
    for (const auto& draw : draw_indices) {
        for (std::size_t position = 0; position < draw.size(); ++position) {
            resampled[position] = values[draw[position]];
        }
        append_bootstrap_metric_sample(result, resampled, daily_annualization,
                                       column);
    }
    finish_bootstrap_metric_results(result);
    return result;
}

}  // namespace fincore
