#include <fincore/risk/evt.hpp>

#include <fincore/detail/optimization.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace fincore {
namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();

std::vector<double> finite_values(const dataframe& data,
                                  const std::string_view column) {
    if (column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    const auto values = require_double_column(data, column);
    std::vector<double> result;
    result.reserve(values.size());
    for (const double value : values) {
        if (std::isfinite(value)) {
            result.push_back(value);
        }
    }
    return result;
}

std::vector<double> tail_magnitudes(const std::vector<double>& values,
                                    const evt_tail tail,
                                    const bool for_hill) {
    std::vector<double> magnitudes;
    if (tail == evt_tail::lower) {
        for (const double value : values) {
            if (value < 0.0) {
                magnitudes.push_back(-value);
            }
        }
        if (for_hill) {
            return magnitudes;
        }
        if (magnitudes.empty()) {
            throw input_error{"No negative returns in data; need at least some losses for GPD fitting"};
        }
        return magnitudes;
    }
    for (const double value : values) {
        if (value > 0.0) {
            magnitudes.push_back(value);
        }
    }
    if (for_hill) {
        return magnitudes;
    }
    if (magnitudes.empty()) {
        throw input_error{"No positive returns in data; need at least some gains for GPD fitting"};
    }
    return magnitudes;
}

double linear_percentile(std::vector<double> values, const double percentile) {
    std::sort(values.begin(), values.end());
    if (values.empty()) {
        return nan;
    }
    const double virtual_index =
        static_cast<double>(values.size() - 1) * (percentile / 100.0);
    const auto lower = static_cast<std::size_t>(std::floor(virtual_index));
    const auto upper = static_cast<std::size_t>(std::ceil(virtual_index));
    if (lower == upper) {
        return values[lower];
    }
    const double difference = values[upper] - values[lower];
    const double fraction = virtual_index - static_cast<double>(lower);
    return fraction < 0.5 ? values[lower] + difference * fraction
                          : values[upper] - difference * (1.0 - fraction);
}

double checked_threshold(const std::optional<double> threshold,
                         const std::vector<double>& magnitudes) {
    if (!threshold.has_value()) {
        return linear_percentile(magnitudes, 90.0);
    }
    const double value = *threshold;
    if (!std::isfinite(value) || value <= 0.0) {
        throw input_error{"threshold must be finite and positive"};
    }
    return value;
}

double default_pot_threshold(const std::vector<double>& values,
                             const evt_tail tail, const double alpha) {
    std::vector<double> magnitudes;
    for (const double value : values) {
        if (tail == evt_tail::lower && value < 0.0) {
            magnitudes.push_back(-value);
        } else if (tail == evt_tail::upper && value > 0.0) {
            magnitudes.push_back(value);
        }
    }
    const std::size_t required = std::max<std::size_t>(
        10U, static_cast<std::size_t>(std::floor(alpha *
                                                 static_cast<double>(values.size()))) + 1U);
    if (magnitudes.size() < required) {
        throw input_error{"Not enough observations in the selected tail to choose a GPD threshold covering alpha"};
    }
    std::sort(magnitudes.begin(), magnitudes.end());
    const double conventional = linear_percentile(magnitudes, 90.0);
    std::size_t above = 0;
    for (const double magnitude : magnitudes) {
        if (magnitude > conventional) {
            ++above;
        }
    }
    if (above >= required) {
        return conventional;
    }
    const double selected = magnitudes[magnitudes.size() - required];
    const double threshold = std::nextafter(selected,
                                            -std::numeric_limits<double>::infinity());
    if (!std::isfinite(threshold) || threshold <= 0.0) {
        throw input_error{"automatic GPD threshold must be finite and positive"};
    }
    return threshold;
}

void validate_alpha(const double alpha) {
    if (!std::isfinite(alpha) || !(alpha > 0.0 && alpha < 1.0)) {
        throw input_error{"alpha must be finite and in (0, 1)"};
    }
}

}  // namespace

hill_estimate hill_estimator(const dataframe& data,
                             const std::optional<double> threshold,
                             const evt_tail tail,
                             const std::string_view column) {
    const auto values = finite_values(data, column);
    const auto magnitudes = tail_magnitudes(values, tail, true);
    const double used_threshold = checked_threshold(threshold, magnitudes);
    std::vector<double> observations;
    for (const double magnitude : magnitudes) {
        if (magnitude > used_threshold) {
            observations.push_back(magnitude);
        }
    }
    if (observations.size() < 10U) {
        throw input_error{"Not enough exceedances for Hill estimation (need >= 10)"};
    }
    double total = 0.0;
    for (const double observation : observations) {
        total += std::log(observation / used_threshold);
    }
    return {total / static_cast<double>(observations.size()),
            std::move(observations)};
}

gpd_parameters gpd_fit(const dataframe& data,
                       const std::optional<double> threshold,
                       const gpd_estimation_method method,
                       const evt_tail tail,
                       const std::string_view column) {
    const auto values = finite_values(data, column);
    const auto magnitudes = tail_magnitudes(values, tail, false);
    const double used_threshold = checked_threshold(threshold, magnitudes);
    std::vector<double> excesses;
    for (const double magnitude : magnitudes) {
        if (magnitude > used_threshold) {
            excesses.push_back(magnitude - used_threshold);
        }
    }
    if (excesses.size() < 10U) {
        throw input_error{"Not enough exceedances for GPD fitting (need >= 10)"};
    }

    double xi = 0.0;
    double beta = 0.0;
    if (method == gpd_estimation_method::mle) {
        double mean = 0.0;
        for (const double value : excesses) {
            mean += value;
        }
        mean /= static_cast<double>(excesses.size());
        double variance = 0.0;
        for (const double value : excesses) {
            variance += (value - mean) * (value - mean);
        }
        variance /= static_cast<double>(excesses.size());
        const double initial_beta = std::sqrt(variance);

        // The source minimizes the raw GPD log-likelihood; reproduce that
        // objective (recorded source behaviour) with bounds xi in [-0.5, 1],
        // beta >= 1e-6 and L-BFGS-B-equivalent tolerances.
        const auto objective = [&excesses](const std::vector<double>& params) {
            const double xi_value = -0.5 + 1.5 / (1.0 + std::exp(-params[0]));
            const double beta_value =
                1e-6 + std::exp(params[1]);
            if (beta_value <= 0.0) {
                return 1e10;
            }
            double total = 0.0;
            for (const double excess : excesses) {
                const double z = 1.0 + xi_value * excess / beta_value;
                if (z <= 0.0) {
                    return 1e10;
                }
                if (std::fabs(xi_value) < 1e-10) {
                    total += std::log(beta_value) + excess / beta_value;
                } else {
                    total += std::log(beta_value) +
                             (1.0 + 1.0 / xi_value) * std::log(z);
                }
            }
            return total;
        };
        const auto inverse = [](const double value, const double lower,
                                const double upper) {
            const double unit = (value - lower) / (upper - lower);
            return std::log(std::min(std::max(unit, 1e-9), 1.0 - 1e-9) /
                            (1.0 - std::min(std::max(unit, 1e-9), 1.0 - 1e-9)));
        };
        detail::nelder_mead_options options;
        options.function_tolerance = 1e-4;
        options.simplex_tolerance = 1e-4;
        options.max_iterations = 600U;
        const auto optimized = detail::minimize_nelder_mead(
            objective, {inverse(0.1, -0.5, 1.0),
                        std::log(std::max(initial_beta - 1e-6, 1e-9))},
            options);
        xi = -0.5 + 1.5 / (1.0 + std::exp(-optimized.parameters[0]));
        beta = 1e-6 + std::exp(optimized.parameters[1]);
        beta = std::fabs(beta);
    } else if (method == gpd_estimation_method::pwm) {
        std::vector<double> sorted(excesses);
        std::sort(sorted.begin(), sorted.end());
        const double count = static_cast<double>(sorted.size());
        double b0 = 0.0;
        for (const double value : sorted) {
            b0 += value;
        }
        b0 /= count;
        double b1 = 0.0;
        for (std::size_t position = 0; position < sorted.size(); ++position) {
            b1 += static_cast<double>(position) / (count - 1.0) *
                  sorted[position];
        }
        b1 /= count;
        const double l2 = 2.0 * b1 - b0;
        if (!std::isfinite(l2) || l2 <= 0.0) {
            throw input_error{"GPD PWM requires a positive finite second L-moment"};
        }
        xi = 2.0 - b0 / l2;
        beta = b0 * (1.0 - xi);
        if (!std::isfinite(xi) || !std::isfinite(beta) || beta <= 0.0) {
            throw input_error{"GPD PWM produced an invalid finite-scale fit"};
        }
    } else {
        throw input_error{"Unknown method"};
    }
    return {xi, beta, used_threshold, excesses.size()};
}

double evt_var(const dataframe& data, const double alpha,
               const evt_model model, const evt_tail tail,
               const std::optional<double> threshold,
               const std::optional<std::size_t> block_size,
               const std::string_view column) {
    static_cast<void>(block_size);
    validate_alpha(alpha);
    if (model != evt_model::gpd) {
        throw input_error{"GEV model is not available in this build"};
    }
    const auto values = finite_values(data, column);
    const double pot_threshold =
        threshold.has_value() ? *threshold : default_pot_threshold(values, tail, alpha);
    const auto params = gpd_fit(data, pot_threshold, gpd_estimation_method::mle,
                                tail, column);
    if (!std::isfinite(params.xi) || !std::isfinite(params.beta) ||
        !std::isfinite(params.threshold) || params.beta <= 0.0 ||
        params.threshold <= 0.0) {
        throw input_error{"GPD fit returned non-finite parameters or non-positive scale/threshold"};
    }
    const double exceed_probability =
        static_cast<double>(params.n_exceed) / static_cast<double>(values.size());
    if (alpha > exceed_probability) {
        throw input_error{"GPD POT alpha exceeds the fitted threshold exceedance probability; choose alpha <= n_exceed / n_total or fit a lower threshold"};
    }
    const double ratio = alpha / exceed_probability;
    const double var_magnitude =
        std::fabs(params.xi) < 1e-10
            ? params.threshold - params.beta * std::log(ratio)
            : params.threshold + (params.beta / params.xi) *
                                     (std::pow(ratio, -params.xi) - 1.0);
    if (!std::isfinite(var_magnitude)) {
        throw input_error{"GPD VaR is not finite for the fitted parameters and alpha"};
    }
    return tail == evt_tail::lower ? -var_magnitude : var_magnitude;
}

double evt_cvar(const dataframe& data, const double alpha,
                const evt_model model, const evt_tail tail,
                const std::optional<double> threshold,
                const std::optional<std::size_t> block_size,
                const std::string_view column) {
    static_cast<void>(block_size);
    validate_alpha(alpha);
    if (model != evt_model::gpd) {
        throw input_error{"GEV model is not available in this build"};
    }
    const auto values = finite_values(data, column);
    const double pot_threshold =
        threshold.has_value() ? *threshold : default_pot_threshold(values, tail, alpha);
    const auto params = gpd_fit(data, pot_threshold, gpd_estimation_method::mle,
                                tail, column);
    if (!std::isfinite(params.xi) || !std::isfinite(params.beta) ||
        !std::isfinite(params.threshold) || params.beta <= 0.0 ||
        params.threshold <= 0.0) {
        throw input_error{"GPD fit returned non-finite parameters or non-positive scale/threshold"};
    }
    const double var = evt_var(data, alpha, model, tail, params.threshold,
                               block_size, column);
    const double var_magnitude = tail == evt_tail::lower ? -var : var;
    double cvar_magnitude = 0.0;
    if (std::fabs(params.xi) < 1e-10) {
        cvar_magnitude = var_magnitude + params.beta;
    } else if (params.xi < 1.0) {
        cvar_magnitude = var_magnitude +
                         (params.beta + params.xi *
                                            (var_magnitude - params.threshold)) /
                             (1.0 - params.xi);
    } else {
        throw input_error{"CVaR infinite for xi >= 1"};
    }
    return tail == evt_tail::lower ? -cvar_magnitude : cvar_magnitude;
}

extreme_risk_result extreme_risk(
    const dataframe& returns, const double alpha, const evt_tail tail,
    const evt_model model, const std::optional<double> threshold,
    const std::optional<std::size_t> block_size, const std::string_view column) {
    validate_alpha(alpha);
    if (model != evt_model::gpd) {
        throw input_error{"GEV model is not available in this build"};
    }
    const auto values = finite_values(returns, column);
    const double pot_threshold =
        threshold.has_value() ? *threshold : default_pot_threshold(values, tail, alpha);
    const auto params = gpd_fit(returns, pot_threshold,
                                gpd_estimation_method::mle, tail, column);
    const double var = evt_var(returns, alpha, model, tail, params.threshold,
                               block_size, column);
    const double cvar = evt_cvar(returns, alpha, model, tail, params.threshold,
                                 block_size, column);
    return {var, cvar, params.xi, params.threshold, params.n_exceed, alpha};
}

}  // namespace fincore
