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

}  // namespace fincore
