#pragma once

#include <fincore/risk/garch.hpp>

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

// Source-tree-only hooks for exercising the production GARCH recursions and
// bounded fit behavior. This header is not part of the installed public API.
namespace fincore::risk::internal {

std::vector<double> garch11_conditional_variances(
    std::span<const double> eps, double omega, double alpha, double beta);
double garch11_log_likelihood(std::span<const double> eps, double omega,
                              double alpha, double beta);

std::vector<double> egarch_conditional_variances(
    std::span<const double> values, const egarch_parameters& params);
double egarch_log_likelihood(std::span<const double> values,
                             const egarch_parameters& params);

std::vector<double> gjr_garch_conditional_variances(
    std::span<const double> values, const gjr_garch_parameters& params);
double gjr_garch_log_likelihood(
    std::span<const double> values, const gjr_garch_parameters& params);

garch11_result fit_garch11_with_max_iterations(
    const dataframe& returns, bool constant_mean, std::string_view column,
    std::size_t max_iterations);
egarch_result fit_egarch_with_max_iterations(const dataframe& returns,
                                             std::string_view column,
                                             std::size_t max_iterations);
gjr_garch_result fit_gjr_garch_with_max_iterations(
    const dataframe& returns, std::string_view column,
    std::size_t max_iterations);

}  // namespace fincore::risk::internal
