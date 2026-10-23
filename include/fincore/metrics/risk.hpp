#pragma once

#include <fincore/metrics/alpha_beta.hpp>

#include <string_view>

namespace fincore {

double historical_value_at_risk(const dataframe &returns, double cutoff = 0.05,
                                std::string_view column = returns_column);

double trading_value_at_risk(
    const dataframe& returns, double sigma = 2.0,
    std::string_view column = returns_column);

double var_excess_return(
    const dataframe& returns, double cutoff = 0.05, double risk_free = 0.0,
    frequency period = frequency::daily,
    std::optional<double> annualization = std::nullopt,
    std::string_view column = returns_column);

double conditional_value_at_risk(const dataframe &returns, double cutoff = 0.05,
                                 std::string_view column = returns_column);

double downside_risk(const dataframe &returns, double required_return = 0.0,
                     double annualization = daily_annualization,
                     std::string_view column = returns_column);

double tail_ratio(const dataframe &returns,
                  std::string_view column = returns_column);

double
tracking_error(const dataframe &returns, const dataframe &benchmark,
               double annualization = daily_annualization,
               std::string_view returns_column_name = returns_column,
               std::string_view benchmark_column_name = benchmark_column);

double residual_risk(const dataframe &returns, const dataframe &benchmark,
                     double risk_free = 0.0,
                     double annualization = daily_annualization,
                     std::string_view returns_column_name = returns_column,
                     std::string_view benchmark_column_name = benchmark_column);

double var_cov_var_normal(double portfolio_value, double confidence,
                          double mean = 0.0, double standard_deviation = 1.0);

struct gpd_risk_estimates_result {
    double threshold;
    double scale;
    double shape;
    double var;
    double es;
};

gpd_risk_estimates_result gpd_risk_estimates(
    const dataframe& returns,
    double var_p = 0.01,
    std::string_view column = returns_column);

} // namespace fincore
