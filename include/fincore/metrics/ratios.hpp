#pragma once

#include <fincore/metrics/alpha_beta.hpp>
#include <fincore/metrics/drawdown.hpp>

#include <string_view>

namespace fincore {

double sharpe_ratio(const dataframe& returns,
                    double risk_free = 0.0,
                    double annualization = daily_annualization,
                    std::string_view column = returns_column);

double adjusted_sharpe_ratio(const dataframe& returns,
                             double risk_free = 0.0,
                             std::string_view column = returns_column);

double sortino_ratio(const dataframe& returns,
                     double required_return = 0.0,
                     double annualization = daily_annualization,
                     std::string_view column = returns_column);

double calmar_ratio(const dataframe& returns,
                    double risk_free = 0.0,
                    double annualization = daily_annualization,
                    std::string_view column = returns_column);

double mar_ratio(const dataframe& returns,
                 double annualization = daily_annualization,
                 std::string_view column = returns_column);

double conditional_sharpe_ratio(const dataframe& returns,
                                double cutoff = 0.05,
                                double risk_free = 0.0,
                                double annualization = daily_annualization,
                                std::string_view column = returns_column);

double deflated_sharpe_ratio(const dataframe& returns,
                             double risk_free = 0.0,
                             int num_trials = 1,
                             std::string_view column = returns_column);

double burke_ratio(const dataframe& returns,
                   double risk_free = 0.0,
                   double annualization = daily_annualization,
                   std::string_view column = returns_column);

double common_sense_ratio(const dataframe& returns,
                          std::string_view column = returns_column);

double kappa_three_ratio(const dataframe& returns,
                         double mar = 0.0,
                         std::string_view column = returns_column);

double excess_sharpe(const dataframe& returns,
                     const dataframe& benchmark,
                     std::string_view returns_column_name = returns_column,
                     std::string_view benchmark_column_name = benchmark_column);

double information_ratio(const dataframe& returns,
                         const dataframe& benchmark,
                         double annualization = daily_annualization,
                         std::string_view returns_column_name = returns_column,
                         std::string_view benchmark_column_name = benchmark_column);

double omega_ratio(const dataframe& returns,
                   double risk_free = 0.0,
                   double required_return = 0.0,
                   double annualization = daily_annualization,
                   std::string_view column = returns_column);

double stability_of_timeseries(const dataframe& returns,
                               std::string_view column = returns_column);

double capture(const dataframe& returns,
               const dataframe& benchmark,
               double annualization = daily_annualization,
               std::string_view returns_column_name = returns_column,
               std::string_view benchmark_column_name = benchmark_column);

double up_capture(const dataframe& returns,
                  const dataframe& benchmark,
                  double annualization = daily_annualization,
                  std::string_view returns_column_name = returns_column,
                  std::string_view benchmark_column_name = benchmark_column);

double down_capture(const dataframe& returns,
                    const dataframe& benchmark,
                    double annualization = daily_annualization,
                    std::string_view returns_column_name = returns_column,
                    std::string_view benchmark_column_name = benchmark_column);

double up_down_capture(const dataframe& returns,
                       const dataframe& benchmark,
                       double annualization = daily_annualization,
                       std::string_view returns_column_name = returns_column,
                       std::string_view benchmark_column_name = benchmark_column);

}  // namespace fincore
