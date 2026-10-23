#include <fincore/risk/garch.hpp>

#include <fincore/detail/optimization.hpp>

#include <cmath>
#include <limits>
#include <vector>

namespace fincore {
namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();
constexpr double omega_floor = 1e-6;

struct transform_context {
    bool constant_mean;
};

double sigmoid(const double value) {
    return 1.0 / (1.0 + std::exp(-value));
}

double logit(const double value) {
    return std::log(value / (1.0 - value));
}

garch11_parameters from_unconstrained(const std::vector<double>& theta,
                                      const bool constant_mean) {
    garch11_parameters params{};
    std::size_t index = 0;
    if (constant_mean) {
        params.mu = theta[index++];
    }
    params.omega = omega_floor + std::exp(theta[index++]);
    params.alpha = omega_floor + (1.0 - 2.0 * omega_floor) * sigmoid(theta[index++]);
    params.beta = omega_floor + (1.0 - 2.0 * omega_floor) * sigmoid(theta[index++]);
    return params;
}

std::vector<double> to_unconstrained(const garch11_parameters& params,
                                     const bool constant_mean) {
    std::vector<double> theta;
    if (constant_mean) {
        theta.push_back(params.mu);
    }
    theta.push_back(std::log(std::max(params.omega - omega_floor, 1e-12)));
    const double alpha_unit =
        (params.alpha - omega_floor) / (1.0 - 2.0 * omega_floor);
    const double beta_unit =
        (params.beta - omega_floor) / (1.0 - 2.0 * omega_floor);
    theta.push_back(logit(std::min(std::max(alpha_unit, 1e-9), 1.0 - 1e-9)));
    theta.push_back(logit(std::min(std::max(beta_unit, 1e-9), 1.0 - 1e-9)));
    return theta;
}

std::vector<double> conditional_variances(const std::vector<double>& eps,
                                          const double omega,
                                          const double alpha,
                                          const double beta) {
    std::vector<double> variances(eps.size(), 0.0);
    double mean = 0.0;
    for (const double value : eps) {
        mean += value;
    }
    mean /= static_cast<double>(eps.size());
    double variance = 0.0;
    for (const double value : eps) {
        const double deviation = value - mean;
        variance += deviation * deviation;
    }
    variance /= static_cast<double>(eps.size());
    variances[0] = variance;
    for (std::size_t position = 1; position < eps.size(); ++position) {
        variances[position] =
            omega + alpha * eps[position - 1] * eps[position - 1] +
            beta * variances[position - 1];
    }
    return variances;
}

double negative_log_likelihood(const garch11_parameters& params,
                               const std::vector<double>& eps) {
    const auto variances =
        conditional_variances(eps, params.omega, params.alpha, params.beta);
    double total = 0.0;
    for (std::size_t position = 1; position < eps.size(); ++position) {
        if (!(variances[position] > 0.0) ||
            !std::isfinite(variances[position])) {
            return std::numeric_limits<double>::infinity();
        }
        total += std::log(2.0 * 3.14159265358979323846 * variances[position]) +
                 eps[position] * eps[position] / variances[position];
    }
    return 0.5 * total;
}

}  // namespace

double garch11_log_likelihood(const std::span<const double> eps,
                              const double omega, const double alpha,
                              const double beta) {
    if (eps.size() < 2U) {
        return nan;
    }
    std::vector<double> values(eps.begin(), eps.end());
    return -negative_log_likelihood({0.0, omega, alpha, beta}, values);
}

garch11_result fit_garch11(const dataframe& returns, const bool constant_mean,
                           const std::string_view column) {
    if (column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    const auto values = require_double_column(returns, column);
    std::vector<double> clean;
    clean.reserve(values.size());
    for (const double value : values) {
        if (!std::isnan(value)) {
            clean.push_back(value);
        }
    }
    if (clean.size() < 11U) {
        throw input_error{"Insufficient data for GARCH estimation"};
    }

    double mean = 0.0;
    for (const double value : clean) {
        mean += value;
    }
    mean /= static_cast<double>(clean.size());
    double variance = 0.0;
    for (const double value : clean) {
        const double deviation = value - mean;
        variance += deviation * deviation;
    }
    variance /= static_cast<double>(clean.size());

    garch11_parameters initial{};
    initial.mu = constant_mean ? mean : 0.0;
    initial.omega = variance * 0.1;
    initial.alpha = 0.1;
    initial.beta = 0.85;
    if (initial.omega <= omega_floor) {
        initial.omega = omega_floor * 10.0;
    }
    const auto initial_theta = to_unconstrained(initial, constant_mean);

    const auto objective = [&clean, constant_mean](const std::vector<double>& theta) {
        const auto params = from_unconstrained(theta, constant_mean);
        std::vector<double> eps(clean.size());
        for (std::size_t position = 0; position < clean.size(); ++position) {
            eps[position] = clean[position] - params.mu;
        }
        return negative_log_likelihood(params, eps);
    };
    detail::nelder_mead_options options;
    options.function_tolerance = 1e-13;
    options.simplex_tolerance = 1e-11;
    options.max_iterations = 8000U;
    const auto optimized =
        detail::minimize_nelder_mead(objective, initial_theta, options);

    garch11_result result{};
    result.params = from_unconstrained(optimized.parameters, constant_mean);
    std::vector<double> eps(clean.size());
    for (std::size_t position = 0; position < clean.size(); ++position) {
        eps[position] = clean[position] - result.params.mu;
    }
    result.conditional_variances = conditional_variances(
        eps, result.params.omega, result.params.alpha, result.params.beta);
    result.residuals.resize(clean.size());
    for (std::size_t position = 0; position < clean.size(); ++position) {
        result.residuals[position] =
            eps[position] / std::sqrt(result.conditional_variances[position]);
    }
    result.log_likelihood = -negative_log_likelihood(result.params, eps);
    result.converged =
        optimized.converged && std::isfinite(result.params.omega) &&
        std::isfinite(result.params.alpha) &&
        std::isfinite(result.params.beta) && result.params.omega > 0.0 &&
        result.params.alpha >= 0.0 && result.params.beta >= 0.0 &&
        result.params.alpha + result.params.beta < 1.0;
    return result;
}

std::vector<double> forecast_garch11(const garch11_result& result,
                                     const std::size_t horizon) {
    if (horizon < 1U) {
        throw input_error{"horizon must be >= 1"};
    }
    std::vector<double> forecasts(horizon, nan);
    if (result.conditional_variances.empty()) {
        return forecasts;
    }
    const double last_variance = result.conditional_variances.back();
    const double last_residual = result.residuals.back();
    const double last_eps = last_residual * std::sqrt(last_variance);
    const double persistence = result.params.alpha + result.params.beta;
    forecasts[0] = result.params.omega +
                   result.params.alpha * last_eps * last_eps +
                   result.params.beta * last_variance;
    for (std::size_t step = 1; step < horizon; ++step) {
        forecasts[step] =
            result.params.omega + persistence * forecasts[step - 1];
    }
    return forecasts;
}

}  // namespace fincore
