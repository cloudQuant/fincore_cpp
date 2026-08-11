#include <fincore/metrics/perf_stats.hpp>

#include <fincore/metrics/alpha_beta.hpp>
#include <fincore/metrics/drawdown.hpp>
#include <fincore/metrics/ratios.hpp>
#include <fincore/metrics/returns.hpp>
#include <fincore/metrics/risk.hpp>
#include <fincore/metrics/stats.hpp>

#include <map>
#include <string>
#include <string_view>

namespace fincore {
namespace {

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

}  // namespace fincore
