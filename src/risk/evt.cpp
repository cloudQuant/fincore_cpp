#include <fincore/risk/evt.hpp>

#include <fincore/detail/numeric.hpp>
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

constexpr double euler_gamma =
    0.577215664901532860606512090082402431;

std::vector<double> gev_block_extremes(const std::vector<double>& values,
                                       const std::size_t block_size,
                                       const evt_tail tail) {
    const std::size_t n_blocks = values.size() / block_size;
    if (n_blocks == 0U) {
        throw input_error{
            "block_size must leave at least one complete block"};
    }
    std::vector<double> extremes;
    extremes.reserve(n_blocks);
    for (std::size_t block = 0U; block < n_blocks; ++block) {
        const std::size_t start = block * block_size;
        double extreme =
            tail == evt_tail::lower ? -values[start] : values[start];
        for (std::size_t offset = 1U; offset < block_size; ++offset) {
            const double value = values[start + offset];
            const double candidate =
                tail == evt_tail::lower ? -value : value;
            extreme = std::max(extreme, candidate);
        }
        extremes.push_back(extreme);
    }
    return extremes;
}

constexpr double log_maximum_double = 709.782712893384;

double genextreme_negative_loglikelihood(
    const std::vector<double>& extremes, const double shape,
    const double location, const double scale) {
    if (!std::isfinite(shape) || !(scale > 0.0) ||
        !std::isfinite(scale)) {
        return std::numeric_limits<double>::infinity();
    }
    double total = 0.0;
    std::size_t unsupported = 0U;
    for (const double value : extremes) {
        const double standardized = (value - location) / scale;
        const double support = 1.0 - shape * standardized;
        if (!std::isfinite(support) || support <= 0.0) {
            ++unsupported;
            continue;
        }
        double log_density = 0.0;
        if (std::fabs(shape) < 1e-8) {
            const double exponential = std::exp(-standardized);
            log_density = -standardized - exponential;
        } else {
            log_density =
                -std::pow(support, 1.0 / shape) +
                (1.0 / shape - 1.0) * std::log(support);
        }
        if (!std::isfinite(log_density)) {
            ++unsupported;
            continue;
        }
        total += log_density;
    }
    // scipy.stats._nnlf_and_penalty drops unsupported observations and adds
    // _LOGXMAX * 100 per dropped point; the scale term still counts all n.
    return -total + static_cast<double>(extremes.size()) * std::log(scale) +
           static_cast<double>(unsupported) * log_maximum_double * 100.0;
}

double regularized_gamma_p(const double shape, const double value) {
    if (!(shape > 0.0) || !(value >= 0.0) || !std::isfinite(shape) ||
        !std::isfinite(value)) {
        return nan;
    }
    if (value == 0.0) {
        return 0.0;
    }
    const double log_factor =
        -value + shape * std::log(value) - std::lgamma(shape);
    if (value < shape + 1.0) {
        double sum = 1.0 / shape;
        double term = sum;
        double shifted = shape;
        for (int iteration = 1; iteration < 10000; ++iteration) {
            shifted += 1.0;
            term *= value / shifted;
            sum += term;
            if (std::fabs(term) < std::fabs(sum) * 1e-16) {
                break;
            }
        }
        return sum * std::exp(log_factor);
    }
    double b = value + 1.0 - shape;
    double c = 1.0 / 1e-300;
    double d = 1.0 / b;
    double fraction = d;
    for (int iteration = 1; iteration < 10000; ++iteration) {
        const double coefficient =
            -static_cast<double>(iteration) *
            (static_cast<double>(iteration) - shape);
        b += 2.0;
        d = coefficient * d + b;
        if (std::fabs(d) < 1e-300) {
            d = 1e-300;
        }
        c = b + coefficient / c;
        if (std::fabs(c) < 1e-300) {
            c = 1e-300;
        }
        d = 1.0 / d;
        const double delta = d * c;
        fraction *= delta;
        if (std::fabs(delta - 1.0) < 1e-16) {
            break;
        }
    }
    return 1.0 - std::exp(log_factor) * fraction;
}

double lower_incomplete_gamma(const double shape, const double value) {
    return regularized_gamma_p(shape, value) * std::tgamma(shape);
}

double exponential_integral_e1(const double value) {
    if (!(value > 0.0) || !std::isfinite(value)) {
        return nan;
    }
    if (value < 1.0) {
        double result = -euler_gamma - std::log(value);
        double power = -value;
        double factorial = 1.0;
        for (int order = 1; order <= 200; ++order) {
            factorial *= static_cast<double>(order);
            const double term =
                power / (static_cast<double>(order) * factorial);
            result -= term;
            power *= -value;
            if (std::fabs(term) < 1e-18 * std::fabs(result)) {
                break;
            }
        }
        return result;
    }
    double b = value + 1.0;
    double c = 1e300;
    double d = 1.0 / b;
    double fraction = d;
    for (int iteration = 1; iteration <= 500; ++iteration) {
        const double coefficient =
            -static_cast<double>(iteration) * iteration;
        b += 2.0;
        d = 1.0 / (coefficient * d + b);
        c = b + coefficient / c;
        const double delta = d * c;
        fraction *= delta;
        if (std::fabs(delta - 1.0) < 1e-16) {
            break;
        }
    }
    return std::exp(-value) * fraction;
}

double gumbel_expected_shortfall(const double mu, const double sigma,
                                 const double alpha) {
    const double t = -std::log1p(-alpha);
    if (t < 1e-5) {
        double series = 0.0;
        double power = 1.0;
        double factorial = 1.0;
        for (int order = 1; order <= 16; ++order) {
            power *= -t;
            factorial *= static_cast<double>(order);
            series -= power /
                      (static_cast<double>(order) * factorial);
        }
        const double scaled =
            std::expm1(-t) / alpha * std::log(t) + series / alpha;
        return mu + sigma * scaled;
    }
    const double numerator =
        euler_gamma + std::exp(-t) * std::log(t) +
        exponential_integral_e1(t);
    return mu + sigma * numerator / alpha;
}

struct scipy_simplex_result {
    std::vector<double> parameters;
    double value;
    std::size_t function_calls;
    bool converged;
};

scipy_simplex_result scipy_style_nelder_mead(
    const std::function<double(const std::vector<double>&)>& objective,
    const std::vector<double>& start, const std::size_t max_function_calls,
    const std::size_t max_iterations, const double xatol,
    const double fatol) {
    constexpr double rho = 1.0;
    constexpr double chi = 2.0;
    constexpr double psi = 0.5;
    constexpr double sigma = 0.5;
    constexpr double non_zero_delta = 0.05;
    constexpr double zero_delta = 0.00025;

    const std::size_t dimensions = start.size();
    std::vector<std::vector<double>> simplex(dimensions + 1U, start);
    for (std::size_t index = 0; index < dimensions; ++index) {
        simplex[index + 1U][index] =
            start[index] != 0.0 ? (1.0 + non_zero_delta) * start[index]
                                : zero_delta;
    }
    std::vector<double> values(dimensions + 1U, 0.0);
    std::size_t calls = 0U;
    for (std::size_t index = 0; index <= dimensions; ++index) {
        values[index] = objective(simplex[index]);
        ++calls;
    }
    const auto sort_simplex = [&] {
        std::vector<std::size_t> order(dimensions + 1U);
        for (std::size_t index = 0; index <= dimensions; ++index) {
            order[index] = index;
        }
        std::sort(order.begin(), order.end(),
                  [&values](const std::size_t left,
                            const std::size_t right) {
                      return values[left] < values[right];
                  });
        std::vector<std::vector<double>> sorted_simplex(dimensions + 1U);
        std::vector<double> sorted_values(dimensions + 1U);
        for (std::size_t index = 0; index <= dimensions; ++index) {
            sorted_simplex[index] = simplex[order[index]];
            sorted_values[index] = values[order[index]];
        }
        simplex = std::move(sorted_simplex);
        values = std::move(sorted_values);
    };
    sort_simplex();

    scipy_simplex_result result{start, values.front(), calls, false};
    std::size_t iterations = 1U;
    while (calls < max_function_calls && iterations < max_iterations) {
        double maximum_step = 0.0;
        double maximum_change = 0.0;
        for (std::size_t vertex = 1U; vertex <= dimensions; ++vertex) {
            for (std::size_t index = 0; index < dimensions; ++index) {
                maximum_step = std::max(
                    maximum_step,
                    std::fabs(simplex[vertex][index] -
                              simplex[0][index]));
            }
            if (std::isfinite(values[vertex]) &&
                std::isfinite(values[0])) {
                maximum_change = std::max(
                    maximum_change,
                    std::fabs(values[vertex] - values[0]));
            } else {
                maximum_change =
                    std::numeric_limits<double>::infinity();
            }
        }
        if (maximum_step <= xatol && maximum_change <= fatol) {
            result.converged = true;
            break;
        }

        std::vector<double> centroid(dimensions, 0.0);
        for (std::size_t vertex = 0; vertex < dimensions; ++vertex) {
            for (std::size_t index = 0; index < dimensions; ++index) {
                centroid[index] += simplex[vertex][index];
            }
        }
        for (double& value : centroid) {
            value /= static_cast<double>(dimensions);
        }
        const auto reflected = [&] {
            std::vector<double> point(dimensions);
            for (std::size_t index = 0; index < dimensions; ++index) {
                point[index] = (1.0 + rho) * centroid[index] -
                               rho * simplex.back()[index];
            }
            return point;
        }();
        const double reflected_value = objective(reflected);
        ++calls;
        bool shrink = false;
        if (reflected_value < values.front()) {
            const auto expanded = [&] {
                std::vector<double> point(dimensions);
                for (std::size_t index = 0; index < dimensions; ++index) {
                    point[index] =
                        (1.0 + rho * chi) * centroid[index] -
                        rho * chi * simplex.back()[index];
                }
                return point;
            }();
            const double expanded_value = objective(expanded);
            ++calls;
            if (expanded_value < reflected_value) {
                simplex.back() = expanded;
                values.back() = expanded_value;
            } else {
                simplex.back() = reflected;
                values.back() = reflected_value;
            }
        } else if (reflected_value < values[dimensions - 1U]) {
            simplex.back() = reflected;
            values.back() = reflected_value;
        } else if (reflected_value < values.back()) {
            const auto contracted = [&] {
                std::vector<double> point(dimensions);
                for (std::size_t index = 0; index < dimensions; ++index) {
                    point[index] = (1.0 + psi * rho) * centroid[index] -
                                   psi * rho * simplex.back()[index];
                }
                return point;
            }();
            const double contracted_value = objective(contracted);
            ++calls;
            if (contracted_value <= reflected_value) {
                simplex.back() = contracted;
                values.back() = contracted_value;
            } else {
                shrink = true;
            }
        } else {
            const auto contracted = [&] {
                std::vector<double> point(dimensions);
                for (std::size_t index = 0; index < dimensions; ++index) {
                    point[index] = (1.0 - psi) * centroid[index] +
                                   psi * simplex.back()[index];
                }
                return point;
            }();
            const double contracted_value = objective(contracted);
            ++calls;
            if (contracted_value < values.back()) {
                simplex.back() = contracted;
                values.back() = contracted_value;
            } else {
                shrink = true;
            }
        }
        if (shrink) {
            for (std::size_t vertex = 1U; vertex <= dimensions; ++vertex) {
                for (std::size_t index = 0; index < dimensions; ++index) {
                    simplex[vertex][index] =
                        simplex[0][index] +
                        sigma * (simplex[vertex][index] -
                                 simplex[0][index]);
                }
                values[vertex] = objective(simplex[vertex]);
                ++calls;
            }
        }
        ++iterations;
        sort_simplex();
        result.parameters = simplex.front();
        result.value = values.front();
        result.function_calls = calls;
    }
    result.parameters = simplex.front();
    result.value = values.front();
    result.function_calls = calls;
    return result;
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

gev_parameters gev_fit(const dataframe& data,
                       const std::optional<std::size_t> block_size,
                       const evt_tail tail,
                       const std::string_view column) {
    if (column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    const auto raw_values = require_double_column(data, column);
    std::vector<double> values;
    values.reserve(raw_values.size());
    for (const double value : raw_values) {
        if (!std::isnan(value)) {
            values.push_back(value);
        }
    }
    if (values.empty()) {
        throw input_error{"GEV fit requires at least one observation"};
    }

    const std::size_t block =
        block_size.has_value()
            ? *block_size
            : static_cast<std::size_t>(
                  std::sqrt(static_cast<double>(values.size())));
    if (block == 0U) {
        throw input_error{"block_size must be a positive integer"};
    }
    const auto extremes = gev_block_extremes(values, block, tail);

    const double mean_extreme =
        detail::mean(std::span<const double>(extremes));
    const double variance =
        detail::population_variance(std::span<const double>(extremes));
    if (!std::isfinite(variance) || variance <= 0.0) {
        throw input_error{
            "GEV fit requires positive finite block-extreme variation"};
    }

    const double scale_skew = std::sqrt(variance);
    double third_moment = 0.0;
    for (const double value : extremes) {
        third_moment +=
            (value - mean_extreme) * (value - mean_extreme) *
            (value - mean_extreme);
    }
    third_moment /= static_cast<double>(extremes.size());
    const double skewness =
        extremes.size() > 2U
            ? third_moment / (scale_skew * scale_skew * scale_skew) *
                  std::sqrt(static_cast<double>(extremes.size()) *
                            static_cast<double>(extremes.size() - 1U)) /
                  static_cast<double>(extremes.size() - 2U)
            : 0.0;

    // scipy.stats.genextreme._fitstart picks shape c = +0.5 for negative
    // skew and -0.5 otherwise, then loc/scale from the method of moments.
    const double shape = skewness < 0.0 ? 0.5 : -0.5;
    const double standard_mean =
        (1.0 - std::tgamma(1.0 + shape)) / shape;
    const double variance_shape = 1.0 + 2.0 * shape;
    double standard_variance =
        std::numeric_limits<double>::infinity();
    if (variance_shape > 0.0) {
        standard_variance =
            (std::tgamma(variance_shape) -
             std::tgamma(1.0 + shape) * std::tgamma(1.0 + shape)) /
            (shape * shape);
    }
    double starting_scale = std::sqrt(variance / standard_variance);
    double starting_location =
        mean_extreme - starting_scale * standard_mean;
    if (!std::isfinite(starting_location)) {
        starting_location = 0.0;
    }
    if (!(std::isfinite(starting_scale) && starting_scale > 0.0)) {
        starting_scale = 1.0;
    }

    const double data_min = *std::min_element(extremes.begin(),
                                              extremes.end());
    const double data_max = *std::max_element(extremes.begin(),
                                              extremes.end());
    const double data_width = data_max - data_min;
    const double lower_support =
        shape > 0.0 ? -std::numeric_limits<double>::infinity() : 1.0 / shape;
    const double upper_support =
        shape > 0.0 ? 1.0 / shape : std::numeric_limits<double>::infinity();
    const double lower_bound =
        starting_location + lower_support * starting_scale;
    const double upper_bound =
        starting_location + upper_support * starting_scale;
    if (!(lower_bound < data_min && data_max < upper_bound)) {
        if (shape > 0.0) {
            starting_location =
                data_max - upper_support + 0.1 * data_width;
            starting_scale = 1.0;
        } else {
            starting_location =
                data_min - lower_support - 0.1 * data_width;
            starting_scale = 1.0;
        }
    }

    const auto objective =
        [&extremes](const std::vector<double>& point) {
            return genextreme_negative_loglikelihood(
                extremes, point[0], point[1], point[2]);
        };
    // scipy.stats.genextreme.fit runs fmin (Nelder-Mead) with xtol=ftol=1e-4
    // and maxiter=maxfun=200*N from the moment-matched start above.
    const auto best = scipy_style_nelder_mead(
        objective,
        std::vector<double>{shape, starting_location, starting_scale},
        600U, 600U, 1e-4, 1e-4);
    if (!std::isfinite(best.value) || best.parameters.size() != 3U) {
        throw input_error{
            "GEV fit did not converge to a finite likelihood"};
    }
    const double xi = -best.parameters[0];
    const double mu = best.parameters[1];
    const double sigma = best.parameters[2];
    if (!std::isfinite(xi) || !std::isfinite(mu) ||
        !std::isfinite(sigma) || sigma <= 0.0) {
        throw input_error{
            "GEV fit returned non-finite parameters or non-positive scale"};
    }
    return {xi, mu, sigma, extremes.size()};
}

double evt_var(const dataframe& data, const double alpha,
               const evt_model model, const evt_tail tail,
               const std::optional<double> threshold,
               const std::optional<std::size_t> block_size,
               const std::string_view column) {
    validate_alpha(alpha);
    if (model == evt_model::gev) {
        const auto params = gev_fit(data, block_size, tail, column);
        if (!std::isfinite(params.xi) || !std::isfinite(params.mu) ||
            !std::isfinite(params.sigma) || params.sigma <= 0.0) {
            throw input_error{"GEV fit returned non-finite parameters or non-positive scale"};
        }
        const double tail_probability = -std::log1p(-alpha);
        const double var_magnitude =
            std::fabs(params.xi) < 1e-10
                ? params.mu -
                      params.sigma * std::log(tail_probability)
                : params.mu +
                      (params.sigma / params.xi) *
                          (std::pow(tail_probability, -params.xi) - 1.0);
        if (!std::isfinite(var_magnitude)) {
            throw input_error{"GEV VaR is not finite for the fitted parameters and alpha"};
        }
        return tail == evt_tail::lower ? -var_magnitude : var_magnitude;
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
    validate_alpha(alpha);
    if (model == evt_model::gev) {
        const auto params = gev_fit(data, block_size, tail, column);
        if (!std::isfinite(params.xi) || !std::isfinite(params.mu) ||
            !std::isfinite(params.sigma) || params.sigma <= 0.0) {
            throw input_error{"GEV fit returned non-finite parameters or non-positive scale"};
        }
        const double tail_probability = -std::log1p(-alpha);
        double cvar_magnitude = 0.0;
        if (std::fabs(params.xi) < 1e-10) {
            cvar_magnitude = gumbel_expected_shortfall(
                params.mu, params.sigma, alpha);
        } else if (params.xi < 1.0) {
            const double lower_gamma = lower_incomplete_gamma(
                1.0 - params.xi, tail_probability);
            cvar_magnitude =
                params.mu +
                (params.sigma / params.xi) *
                    (lower_gamma / alpha - 1.0);
        } else {
            throw input_error{"CVaR infinite for xi >= 1"};
        }
        if (!std::isfinite(cvar_magnitude)) {
            throw input_error{"GEV CVaR is not finite for the fitted parameters"};
        }
        return tail == evt_tail::lower ? -cvar_magnitude
                                       : cvar_magnitude;
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
    if (model == evt_model::gev) {
        const auto params = gev_fit(returns, block_size, tail, column);
        const double var =
            evt_var(returns, alpha, model, tail, threshold, block_size,
                    column);
        const double cvar =
            evt_cvar(returns, alpha, model, tail, threshold, block_size,
                     column);
        extreme_risk_result result{var, cvar, params.xi, nan, 0U, alpha};
        result.location = params.mu;
        result.scale = params.sigma;
        result.n_blocks = params.n_blocks;
        return result;
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
