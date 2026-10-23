#pragma once

#include <fincore/metrics/returns.hpp>

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace fincore {

struct garch11_parameters {
    double mu;
    double omega;
    double alpha;
    double beta;
};

struct garch11_result {
    garch11_parameters params;
    std::vector<double> conditional_variances;
    std::vector<double> residuals;
    double log_likelihood;
    bool converged;
};

double garch11_log_likelihood(std::span<const double> eps,
                              double omega,
                              double alpha,
                              double beta);

garch11_result fit_garch11(const dataframe& returns,
                           bool constant_mean = false,
                           std::string_view column = returns_column);

std::vector<double> forecast_garch11(const garch11_result& result,
                                     std::size_t horizon = 1U);

struct egarch_parameters {
    double omega;
    double alpha;
    double gamma;
    double beta;
};

struct egarch_result {
    egarch_parameters params;
    std::vector<double> conditional_variances;
    std::vector<double> residuals;
    double log_likelihood;
    bool converged;
};

struct gjr_garch_parameters {
    double omega;
    double alpha;
    double gamma;
    double beta;
};

struct gjr_garch_result {
    gjr_garch_parameters params;
    std::vector<double> conditional_variances;
    std::vector<double> residuals;
    double log_likelihood;
    bool converged;
};

enum class garch_model { garch, egarch, gjr_garch };

egarch_result fit_egarch(const dataframe& returns,
                         std::string_view column = returns_column);

std::vector<double> forecast_egarch(const egarch_result& result,
                                    std::size_t horizon = 1U);

gjr_garch_result fit_gjr_garch(const dataframe& returns,
                               std::string_view column = returns_column);

std::vector<double> forecast_gjr_garch(const gjr_garch_result& result,
                                       std::size_t horizon = 1U);

struct garch_forecast_result {
    std::vector<double> forecasts;
    double log_likelihood;
    bool converged;
};

garch_forecast_result forecast_volatility(
    const dataframe& returns, garch_model model, std::size_t horizon = 1U,
    std::string_view column = returns_column);

struct garch_conditional_estimate {
    double value;
    std::vector<double> conditional_variances;
    bool converged;
};

garch_conditional_estimate conditional_var(
    const dataframe& returns, garch_model model, double alpha = 0.05,
    std::size_t horizon = 1U, std::string_view column = returns_column);

garch_conditional_estimate conditional_es(
    const dataframe& returns, garch_model model, double alpha = 0.05,
    std::size_t horizon = 1U, std::string_view column = returns_column);

}  // namespace fincore
