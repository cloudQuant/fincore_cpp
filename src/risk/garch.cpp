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

namespace {

double normal_ppf(const double probability) {
    constexpr double low = 0.02425;
    const double high = 1.0 - low;
    double result = 0.0;
    if (probability < low) {
        const double q = std::sqrt(-2.0 * std::log(probability));
        result = (((((-7.784894002430293e-03 * q - 3.223964580411365e-01) * q -
                      2.400758277161838e+00) * q - 2.549732539343734e+00) * q +
                    4.374664141464968e+00) * q + 2.938163982698783e+00) /
                 ((((7.784695709041462e-03 * q + 3.224671290700398e-01) * q +
                    2.445134137142996e+00) * q + 3.754408661907416e+00) * q + 1.0);
    } else if (probability <= high) {
        const double q = probability - 0.5;
        const double r = q * q;
        result = (((((-3.969683028665376e+01 * r + 2.209460984245205e+02) * r -
                      2.759285104469687e+02) * r + 1.383577518672690e+02) * r -
                    3.066479806614716e+01) * r + 2.506628277459239e+00) * q /
                 (((((-5.447609879822406e+01 * r + 1.615858368580409e+02) * r -
                      1.556989798598866e+02) * r + 6.680131188771972e+01) * r -
                    1.328068155288572e+01) * r + 1.0);
    } else {
        const double q = std::sqrt(-2.0 * std::log(1.0 - probability));
        result = -(((((-7.784894002430293e-03 * q - 3.223964580411365e-01) * q -
                       2.400758277161838e+00) * q - 2.549732539343734e+00) * q +
                     4.374664141464968e+00) * q + 2.938163982698783e+00) /
                  ((((7.784695709041462e-03 * q + 3.224671290700398e-01) * q +
                     2.445134137142996e+00) * q + 3.754408661907416e+00) * q + 1.0);
    }
    const double error =
        0.5 * std::erfc(-result / std::sqrt(2.0)) - probability;
    const double correction =
        error * std::sqrt(2.0 * 3.14159265358979323846) *
        std::exp(result * result / 2.0);
    return result - correction / (1.0 + result * correction / 2.0);
}

double normal_pdf(const double value) {
    return std::exp(-0.5 * value * value) / std::sqrt(2.0 * 3.14159265358979323846);
}

double population_variance(const std::vector<double>& values) {
    double mean = 0.0;
    for (const double value : values) {
        mean += value;
    }
    mean /= static_cast<double>(values.size());
    double total = 0.0;
    for (const double value : values) {
        const double deviation = value - mean;
        total += deviation * deviation;
    }
    return total / static_cast<double>(values.size());
}

egarch_parameters egarch_from_theta(const std::vector<double>& theta) {
    egarch_parameters params{};
    params.omega = theta[0];
    params.alpha = omega_floor + std::exp(theta[1]);
    params.gamma = std::tanh(theta[2]);
    params.beta = omega_floor + (1.0 - 2.0 * omega_floor) * sigmoid(theta[3]);
    return params;
}

std::vector<double> egarch_to_theta(const egarch_parameters& params) {
    return {params.omega,
            std::log(std::max(params.alpha - omega_floor, 1e-12)),
            std::atanh(std::min(std::max(params.gamma, -0.999), 0.999)),
            logit(std::min(std::max(
                      (params.beta - omega_floor) / (1.0 - 2.0 * omega_floor),
                      1e-9),
                  1.0 - 1e-9))};
}

std::vector<double> egarch_variances(const std::vector<double>& values,
                                     const egarch_parameters& params) {
    std::vector<double> variances(values.size(), nan);
    const double initial = population_variance(values);
    if (!(initial > 0.0) || !std::isfinite(initial)) {
        return variances;
    }
    variances[0] = initial;
    double log_variance = std::log(initial);
    for (std::size_t index = 1; index < values.size(); ++index) {
        const double z = values[index - 1] / std::sqrt(variances[index - 1]);
        log_variance = params.omega + params.alpha * std::fabs(z) +
                       params.gamma * z + params.beta * log_variance;
        variances[index] = std::exp(log_variance);
        if (!std::isfinite(variances[index])) {
            return std::vector<double>(values.size(), nan);
        }
    }
    return variances;
}

double egarch_negative_log_likelihood(const std::vector<double>& values,
                                      const egarch_parameters& params) {
    const auto variances = egarch_variances(values, params);
    for (const double value : variances) {
        if (!std::isfinite(value) || value <= 0.0) {
            return 1e100;
        }
    }
    double total = 0.0;
    for (std::size_t index = 1; index < values.size(); ++index) {
        const double eps = values[index] / std::sqrt(variances[index]);
        total += std::log(2.0 * 3.14159265358979323846 * variances[index]) +
                 eps * eps;
    }
    const double loglik = -0.5 * total;
    return std::isfinite(loglik) ? -loglik : 1e100;
}

gjr_garch_parameters gjr_from_theta(const std::vector<double>& theta) {
    gjr_garch_parameters params{};
    params.omega = omega_floor + std::exp(theta[0]);
    params.alpha = omega_floor + (1.0 - 2.0 * omega_floor) * sigmoid(theta[1]);
    params.gamma = omega_floor + (1.0 - 2.0 * omega_floor) * sigmoid(theta[2]);
    params.beta = omega_floor + (1.0 - 2.0 * omega_floor) * sigmoid(theta[3]);
    return params;
}

std::vector<double> gjr_to_theta(const gjr_garch_parameters& params) {
    const auto unit = [](const double value) {
        return std::min(std::max(
                            (value - omega_floor) / (1.0 - 2.0 * omega_floor),
                            1e-9),
                        1.0 - 1e-9);
    };
    return {std::log(std::max(params.omega - omega_floor, 1e-12)),
            logit(unit(params.alpha)), logit(unit(params.gamma)),
            logit(unit(params.beta))};
}

std::vector<double> gjr_variances(const std::vector<double>& values,
                                  const gjr_garch_parameters& params) {
    std::vector<double> variances(values.size(), 0.0);
    variances[0] = population_variance(values);
    for (std::size_t index = 1; index < values.size(); ++index) {
        const double indicator = values[index - 1] < 0.0 ? 1.0 : 0.0;
        variances[index] =
            params.omega + params.alpha * values[index - 1] * values[index - 1] +
            params.gamma * indicator * values[index - 1] * values[index - 1] +
            params.beta * variances[index - 1];
    }
    return variances;
}

double gjr_negative_log_likelihood(const std::vector<double>& values,
                                   const gjr_garch_parameters& params) {
    const auto variances = gjr_variances(values, params);
    double total = 0.0;
    for (std::size_t index = 1; index < values.size(); ++index) {
        if (!(variances[index] > 0.0) || !std::isfinite(variances[index])) {
            return 1e100;
        }
        total += std::log(2.0 * 3.14159265358979323846 * variances[index]) +
                 values[index] * values[index] / variances[index];
    }
    return 0.5 * total;
}

std::vector<double> clean_series(const dataframe& returns,
                                 const std::string_view column) {
    if (column.find(' ') != std::string_view::npos) {
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
    return clean;
}

}  // namespace

egarch_result fit_egarch(const dataframe& returns,
                         const std::string_view column) {
    const auto clean = clean_series(returns, column);
    if (clean.size() < 10U) {
        throw input_error{"Insufficient data for EGARCH estimation"};
    }
    const double initial_variance = population_variance(clean);
    for (const double value : clean) {
        if (!std::isfinite(value)) {
            throw input_error{"EGARCH estimation requires finite returns with positive variance"};
        }
    }
    if (!(initial_variance > 0.0) || !std::isfinite(initial_variance)) {
        throw input_error{"EGARCH estimation requires finite returns with positive variance"};
    }

    const auto objective = [&clean](const std::vector<double>& theta) {
        return egarch_negative_log_likelihood(clean, egarch_from_theta(theta));
    };
    detail::nelder_mead_options options;
    options.function_tolerance = 1e-13;
    options.simplex_tolerance = 1e-11;
    options.max_iterations = 10000U;
    const std::vector<egarch_parameters> starts{
        {0.01, 0.1, -0.1, 0.95}, {0.01, 0.3, 0.2, 0.95},
        {0.01, 0.3, -0.2, 0.9},  {-0.2, 0.2, 0.1, 0.8}};
    detail::nelder_mead_result optimized =
        detail::minimize_nelder_mead(objective, egarch_to_theta(starts[0]),
                                     options);
    for (std::size_t start = 1; start < starts.size(); ++start) {
        const auto candidate = detail::minimize_nelder_mead(
            objective, egarch_to_theta(starts[start]), options);
        if (candidate.value < optimized.value) {
            optimized = candidate;
        }
    }

    egarch_result result{};
    result.params = egarch_from_theta(optimized.parameters);
    result.conditional_variances = egarch_variances(clean, result.params);
    result.residuals.resize(clean.size());
    for (std::size_t index = 0; index < clean.size(); ++index) {
        result.residuals[index] =
            clean[index] / std::sqrt(result.conditional_variances[index]);
    }
    result.log_likelihood =
        -egarch_negative_log_likelihood(clean, result.params);
    result.converged =
        optimized.converged && std::isfinite(result.params.omega) &&
        std::isfinite(result.params.alpha) &&
        std::isfinite(result.params.gamma) &&
        std::isfinite(result.params.beta) && result.params.alpha >= 0.0 &&
        std::fabs(result.params.beta) < 1.0;
    return result;
}

std::vector<double> forecast_egarch(const egarch_result& result,
                                    const std::size_t horizon) {
    if (horizon < 1U) {
        throw input_error{"horizon must be >= 1"};
    }
    std::vector<double> forecasts(horizon, nan);
    if (result.conditional_variances.empty()) {
        return forecasts;
    }
    const double last_variance = result.conditional_variances.back();
    const double last_z = result.residuals.back();
    const double last_log = last_variance > 0.0 ? std::log(last_variance) : 0.0;
    const double drift =
        result.params.alpha * std::sqrt(2.0 / 3.14159265358979323846);
    double log_forecast = result.params.omega +
                          result.params.alpha * std::fabs(last_z) +
                          result.params.gamma * last_z +
                          result.params.beta * last_log;
    forecasts[0] = std::exp(log_forecast);
    for (std::size_t step = 1; step < horizon; ++step) {
        log_forecast = result.params.omega + drift +
                       result.params.beta * log_forecast;
        forecasts[step] = std::exp(log_forecast);
    }
    return forecasts;
}

gjr_garch_result fit_gjr_garch(const dataframe& returns,
                               const std::string_view column) {
    const auto clean = clean_series(returns, column);
    if (clean.size() < 10U) {
        throw input_error{"Insufficient data for GJR-GARCH estimation"};
    }
    const auto objective = [&clean](const std::vector<double>& theta) {
        return gjr_negative_log_likelihood(clean, gjr_from_theta(theta));
    };
    detail::nelder_mead_options options;
    options.function_tolerance = 1e-13;
    options.simplex_tolerance = 1e-11;
    options.max_iterations = 10000U;
    const std::vector<gjr_garch_parameters> starts{
        {0.01, 0.05, 0.05, 0.9}, {0.01, 0.2, 0.1, 0.8},
        {0.0001, 0.1, 0.2, 0.7}};
    detail::nelder_mead_result optimized =
        detail::minimize_nelder_mead(objective, gjr_to_theta(starts[0]),
                                     options);
    for (std::size_t start = 1; start < starts.size(); ++start) {
        const auto candidate = detail::minimize_nelder_mead(
            objective, gjr_to_theta(starts[start]), options);
        if (candidate.value < optimized.value) {
            optimized = candidate;
        }
    }

    gjr_garch_result result{};
    result.params = gjr_from_theta(optimized.parameters);
    result.conditional_variances = gjr_variances(clean, result.params);
    result.residuals.resize(clean.size());
    for (std::size_t index = 0; index < clean.size(); ++index) {
        result.residuals[index] =
            clean[index] / std::sqrt(result.conditional_variances[index]);
    }
    result.log_likelihood =
        -gjr_negative_log_likelihood(clean, result.params);
    result.converged =
        optimized.converged && std::isfinite(result.params.omega) &&
        std::isfinite(result.params.alpha) &&
        std::isfinite(result.params.gamma) &&
        std::isfinite(result.params.beta) && result.params.omega > 0.0 &&
        result.params.alpha >= 0.0 && result.params.gamma >= 0.0 &&
        result.params.beta >= 0.0 && result.params.alpha +
                                             0.5 * result.params.gamma +
                                             result.params.beta <
                                         1.0;
    return result;
}

std::vector<double> forecast_gjr_garch(const gjr_garch_result& result,
                                       const std::size_t horizon) {
    if (horizon < 1U) {
        throw input_error{"horizon must be >= 1"};
    }
    std::vector<double> forecasts(horizon, nan);
    if (result.conditional_variances.empty()) {
        return forecasts;
    }
    const double last_variance = result.conditional_variances.back();
    const double last_eps = result.residuals.back() * std::sqrt(last_variance);
    const double indicator = last_eps < 0.0 ? 1.0 : 0.0;
    const double persistence = result.params.alpha +
                               0.5 * result.params.gamma + result.params.beta;
    forecasts[0] = result.params.omega +
                   (result.params.alpha + result.params.gamma * indicator) *
                       last_eps * last_eps +
                   result.params.beta * last_variance;
    for (std::size_t step = 1; step < horizon; ++step) {
        forecasts[step] = result.params.omega + persistence * forecasts[step - 1];
    }
    return forecasts;
}

garch_forecast_result forecast_volatility(const dataframe& returns,
                                          const garch_model model,
                                          const std::size_t horizon,
                                          const std::string_view column) {
    if (horizon < 1U) {
        throw input_error{"horizon must be >= 1"};
    }
    garch_forecast_result result{};
    if (model == garch_model::garch) {
        const auto fit = fit_garch11(returns, false, column);
        result.forecasts = forecast_garch11(fit, horizon);
        result.log_likelihood = fit.log_likelihood;
        result.converged = fit.converged;
    } else if (model == garch_model::egarch) {
        const auto fit = fit_egarch(returns, column);
        result.forecasts = forecast_egarch(fit, horizon);
        result.log_likelihood = fit.log_likelihood;
        result.converged = fit.converged;
    } else {
        const auto fit = fit_gjr_garch(returns, column);
        result.forecasts = forecast_gjr_garch(fit, horizon);
        result.log_likelihood = fit.log_likelihood;
        result.converged = fit.converged;
    }
    return result;
}

garch_conditional_estimate conditional_var(const dataframe& returns,
                                           const garch_model model,
                                           const double alpha,
                                           const std::size_t horizon,
                                           const std::string_view column) {
    const auto forecast =
        forecast_volatility(returns, model, horizon, column);
    double total = 0.0;
    for (const double value : forecast.forecasts) {
        total += value;
    }
    const double quantile =
        alpha > 0.0 && alpha < 1.0 ? normal_ppf(alpha) : nan;
    garch_conditional_estimate result{};
    result.value = quantile * std::sqrt(total);
    result.conditional_variances = forecast.forecasts;
    result.converged = forecast.converged;
    return result;
}

garch_conditional_estimate conditional_es(const dataframe& returns,
                                          const garch_model model,
                                          const double alpha,
                                          const std::size_t horizon,
                                          const std::string_view column) {
    const auto forecast =
        forecast_volatility(returns, model, horizon, column);
    double total = 0.0;
    for (const double value : forecast.forecasts) {
        total += value;
    }
    const double quantile =
        alpha > 0.0 && alpha < 1.0 ? normal_ppf(alpha) : nan;
    garch_conditional_estimate result{};
    result.value = -std::sqrt(total) * normal_pdf(quantile) / alpha;
    result.conditional_variances = forecast.forecasts;
    result.converged = forecast.converged;
    return result;
}

}  // namespace fincore
