#include <fincore/detail/optimization.hpp>

#include <fincore/error.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <string>

namespace fincore::detail {
namespace {

void validate_options(const nelder_mead_options& options) {
    if (!std::isfinite(options.function_tolerance) ||
        options.function_tolerance < 0.0 ||
        !std::isfinite(options.simplex_tolerance) ||
        options.simplex_tolerance < 0.0) {
        throw input_error{
            "Nelder-Mead tolerances must be finite and non-negative"};
    }
}

void validate_options(const projected_gradient_options& options) {
    if (!std::isfinite(options.gradient_tolerance) ||
        options.gradient_tolerance < 0.0 ||
        !std::isfinite(options.step_tolerance) ||
        options.step_tolerance <= 0.0) {
        throw input_error{
            "projected-gradient tolerances must be finite, with a positive step tolerance"};
    }
}

void validate_finite(const std::vector<double>& values,
                     const char* description) {
    if (std::any_of(values.begin(), values.end(),
                    [](const double value) { return !std::isfinite(value); })) {
        throw input_error{std::string{description} +
                          " must contain only finite values"};
    }
}

void validate_hessian_symmetry(const std::vector<double>& hessian,
                               const std::size_t dimensions) {
    for (std::size_t row = 0; row < dimensions; ++row) {
        for (std::size_t column = row + 1U; column < dimensions; ++column) {
            const double left = hessian[row * dimensions + column];
            const double right = hessian[column * dimensions + row];
            const double scale =
                std::max({1.0, std::fabs(left), std::fabs(right)});
            if (std::fabs(left - right) > 1e-12 * scale) {
                throw input_error{"quadratic Hessian must be symmetric"};
            }
        }
    }
}

void validate_quadratic_dimensions(const std::vector<double>& hessian,
                                   const std::vector<double>& linear,
                                   const std::vector<double>& point) {
    const std::size_t dimensions = linear.size();
    if (dimensions == 0U || point.size() != dimensions ||
        dimensions > std::numeric_limits<std::size_t>::max() / dimensions ||
        hessian.size() != dimensions * dimensions) {
        throw input_error{
            "quadratic inputs must have a non-empty, matching vector and square Hessian"};
    }
    validate_finite(hessian, "Hessian");
    validate_finite(linear, "linear term");
    validate_finite(point, "quadratic point");
    validate_hessian_symmetry(hessian, dimensions);
}

double objective_of(const std::function<double(const std::vector<double>&)>& fn,
                    const std::vector<double>& point) {
    if (std::any_of(point.begin(), point.end(),
                    [](const double value) { return !std::isfinite(value); })) {
        return std::numeric_limits<double>::infinity();
    }
    const double value = fn(point);
    return std::isfinite(value) ? value : std::numeric_limits<double>::infinity();
}

double quadratic_objective_unchecked(const std::vector<double>& hessian,
                                     const std::vector<double>& linear,
                                     const std::vector<double>& point) {
    const std::size_t dimensions = linear.size();
    double value = 0.0;
    for (std::size_t row = 0; row < dimensions; ++row) {
        for (std::size_t column = 0; column < dimensions; ++column) {
            value += 0.5 * point[row] * hessian[row * dimensions + column] *
                     point[column];
        }
        value += linear[row] * point[row];
    }
    return value;
}

}  // namespace

nelder_mead_result minimize_nelder_mead(
    const std::function<double(const std::vector<double>&)>& objective,
    std::vector<double> start, const nelder_mead_options& options) {
    if (!objective) {
        throw input_error{"Nelder-Mead objective must be callable"};
    }
    if (start.empty()) {
        throw input_error{
            "Nelder-Mead start must contain at least one parameter"};
    }
    validate_finite(start, "Nelder-Mead start");
    validate_options(options);

    nelder_mead_result result;
    const std::size_t dimensions = start.size();

    std::vector<std::vector<double>> simplex(dimensions + 1U);
    simplex[0] = start;
    for (std::size_t index = 0; index < dimensions; ++index) {
        simplex[index + 1U] = start;
        simplex[index + 1U][index] =
            start[index] != 0.0 ? start[index] * 1.05 : 0.05;
    }
    std::vector<double> values(dimensions + 1U, 0.0);
    for (std::size_t index = 0; index <= dimensions; ++index) {
        values[index] = objective_of(objective, simplex[index]);
    }

    constexpr double reflection = 1.0;
    constexpr double expansion = 2.0;
    constexpr double contraction = 0.5;
    constexpr double shrink = 0.5;
    for (std::size_t iteration = 0; iteration < options.max_iterations;
         ++iteration) {
        std::vector<std::size_t> order(dimensions + 1U);
        std::iota(order.begin(), order.end(), 0U);
        std::sort(order.begin(), order.end(),
                  [&values](const std::size_t left, const std::size_t right) {
                      return values[left] < values[right];
                  });
        std::vector<std::vector<double>> sorted(dimensions + 1U);
        std::vector<double> sorted_values(dimensions + 1U);
        for (std::size_t index = 0; index <= dimensions; ++index) {
            sorted[index] = simplex[order[index]];
            sorted_values[index] = values[order[index]];
        }
        simplex = std::move(sorted);
        values = std::move(sorted_values);

        const double spread = values.back() - values.front();
        double maximum_span = 0.0;
        for (std::size_t index = 0; index < dimensions; ++index) {
            double low = simplex[0][index];
            double high = simplex[0][index];
            for (std::size_t vertex = 1; vertex <= dimensions; ++vertex) {
                low = std::min(low, simplex[vertex][index]);
                high = std::max(high, simplex[vertex][index]);
            }
            maximum_span = std::max(maximum_span, high - low);
        }
        result.iterations = iteration + 1U;
        if (std::isfinite(spread) &&
            spread <= options.function_tolerance &&
            maximum_span <= options.simplex_tolerance) {
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
                point[index] = centroid[index] +
                               reflection * (centroid[index] -
                                             simplex.back()[index]);
            }
            return point;
        }();
        const double reflected_value = objective_of(objective, reflected);
        if (reflected_value < values.front()) {
            const auto expanded = [&] {
                std::vector<double> point(dimensions);
                for (std::size_t index = 0; index < dimensions; ++index) {
                    point[index] = centroid[index] +
                                   expansion * (reflected[index] -
                                                centroid[index]);
                }
                return point;
            }();
            const double expanded_value = objective_of(objective, expanded);
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
        } else {
            const auto contracted = [&] {
                std::vector<double> point(dimensions);
                for (std::size_t index = 0; index < dimensions; ++index) {
                    point[index] = centroid[index] +
                                   contraction * (simplex.back()[index] -
                                                  centroid[index]);
                }
                return point;
            }();
            const double contracted_value = objective_of(objective, contracted);
            if (contracted_value < values.back()) {
                simplex.back() = contracted;
                values.back() = contracted_value;
            } else {
                for (std::size_t vertex = 1; vertex <= dimensions; ++vertex) {
                    for (std::size_t index = 0; index < dimensions; ++index) {
                        simplex[vertex][index] =
                            simplex[0][index] +
                            shrink * (simplex[vertex][index] -
                                      simplex[0][index]);
                    }
                    values[vertex] = objective_of(objective, simplex[vertex]);
                }
            }
        }
    }

    const auto best = std::min_element(values.begin(), values.end());
    const auto best_index =
        static_cast<std::size_t>(std::distance(values.begin(), best));
    result.parameters = simplex[best_index];
    result.value = values[best_index];
    if (!std::isfinite(result.value)) {
        result.value = std::numeric_limits<double>::infinity();
        result.converged = false;
    }
    return result;
}

std::vector<double> project_onto_simplex(const std::vector<double>& values,
                                         const double lower,
                                         const double upper,
                                         const double target_sum) {
    if (values.empty()) {
        throw input_error{"simplex projection requires at least one value"};
    }
    validate_finite(values, "simplex values");
    if (!std::isfinite(lower) || !std::isfinite(upper) ||
        !std::isfinite(target_sum) || lower > upper) {
        throw input_error{
            "simplex bounds and target must be finite, with lower <= upper"};
    }
    const long double minimum_sum =
        static_cast<long double>(values.size()) * lower;
    const long double maximum_sum =
        static_cast<long double>(values.size()) * upper;
    if (static_cast<long double>(target_sum) < minimum_sum ||
        static_cast<long double>(target_sum) > maximum_sum) {
        throw input_error{"simplex target sum is infeasible for the bounds"};
    }

    std::vector<double> result(values.size(), 0.0);
    if (static_cast<long double>(target_sum) == minimum_sum) {
        std::fill(result.begin(), result.end(), lower);
        return result;
    }
    if (static_cast<long double>(target_sum) == maximum_sum) {
        std::fill(result.begin(), result.end(), upper);
        return result;
    }

    const auto [minimum, maximum] =
        std::minmax_element(values.begin(), values.end());
    double low = lower - *maximum;
    double high = upper - *minimum;
    if (!std::isfinite(low) || !std::isfinite(high)) {
        throw input_error{"simplex projection numeric range is too large"};
    }
    for (int iteration = 0; iteration < 200; ++iteration) {
        const double lambda = std::midpoint(low, high);
        long double total = 0.0L;
        for (const double value : values) {
            total += static_cast<long double>(
                std::clamp(value + lambda, lower, upper));
        }
        if (total > static_cast<long double>(target_sum)) {
            high = lambda;
        } else {
            low = lambda;
        }
    }
    const double lambda = std::midpoint(low, high);
    for (std::size_t index = 0; index < values.size(); ++index) {
        result[index] = std::clamp(values[index] + lambda, lower, upper);
    }
    return result;
}

double quadratic_objective(const std::vector<double>& hessian,
                           const std::vector<double>& linear,
                           const std::vector<double>& point) {
    validate_quadratic_dimensions(hessian, linear, point);
    return quadratic_objective_unchecked(hessian, linear, point);
}

projected_gradient_result minimize_quadratic_on_simplex(
    const std::vector<double>& hessian, const std::vector<double>& linear,
    std::vector<double> start, const projected_gradient_options& options) {
    validate_quadratic_dimensions(hessian, linear, start);
    validate_options(options);

    projected_gradient_result result;
    const std::size_t dimensions = linear.size();
    std::vector<double> point = project_onto_simplex(start);
    double value = quadratic_objective_unchecked(hessian, linear, point);
    result.parameters = point;
    result.value = value;
    if (!std::isfinite(value)) {
        result.projected_gradient_norm =
            std::numeric_limits<double>::infinity();
        return result;
    }

    const auto gradient_at = [&](const std::vector<double>& at) {
        std::vector<double> gradient(dimensions, 0.0);
        for (std::size_t row = 0; row < dimensions; ++row) {
            gradient[row] = linear[row];
            for (std::size_t column = 0; column < dimensions; ++column) {
                gradient[row] += hessian[row * dimensions + column] * at[column];
            }
        }
        return gradient;
    };
    long double hessian_row_sum = 0.0L;
    double hessian_lipschitz_bound = 1.0;
    for (std::size_t row = 0; row < dimensions; ++row) {
        hessian_row_sum = 0.0L;
        for (std::size_t column = 0; column < dimensions; ++column) {
            hessian_row_sum +=
                std::fabs(static_cast<long double>(hessian[row * dimensions + column]));
        }
        if (hessian_row_sum >
            static_cast<long double>(std::numeric_limits<double>::max())) {
            hessian_lipschitz_bound = std::numeric_limits<double>::max();
            break;
        }
        hessian_lipschitz_bound =
            std::max(hessian_lipschitz_bound,
                     static_cast<double>(hessian_row_sum));
    }
    const double stationarity_step = 1.0 / hessian_lipschitz_bound;
    const auto projected_gradient_norm_at =
        [&](const std::vector<double>& at, const double trial_step) {
            const auto gradient = gradient_at(at);
            std::vector<double> raw(dimensions, 0.0);
            for (std::size_t index = 0; index < dimensions; ++index) {
                raw[index] = at[index] - trial_step * gradient[index];
                if (!std::isfinite(raw[index])) {
                    return std::numeric_limits<double>::infinity();
                }
            }
            const auto projected = project_onto_simplex(raw);
            double squared_norm = 0.0;
            for (std::size_t index = 0; index < dimensions; ++index) {
                const double difference = at[index] - projected[index];
                squared_norm += difference * difference;
            }
            return std::sqrt(squared_norm) / trial_step;
        };
    result.projected_gradient_norm =
        projected_gradient_norm_at(point, stationarity_step);
    if (options.max_iterations == 0U) {
        return result;
    }

    double step = stationarity_step;
    double current_norm = result.projected_gradient_norm;
    for (std::size_t iteration = 0; iteration < options.max_iterations;
         ++iteration) {
        const auto gradient = gradient_at(point);
        const auto candidate_for = [&](const double trial_step) {
            std::vector<double> raw(dimensions);
            for (std::size_t index = 0; index < dimensions; ++index) {
                raw[index] = point[index] - trial_step * gradient[index];
                if (!std::isfinite(raw[index])) {
                    return std::vector<double>{};
                }
            }
            return project_onto_simplex(raw);
        };
        std::vector<double> candidate = candidate_for(step);
        double candidate_value = candidate.empty()
                                     ? std::numeric_limits<double>::infinity()
                                     : quadratic_objective_unchecked(
                                           hessian, linear, candidate);
        double candidate_norm =
            candidate.empty()
                ? std::numeric_limits<double>::infinity()
                : projected_gradient_norm_at(candidate, stationarity_step);
        const double roundoff_margin =
            8.0 * std::numeric_limits<double>::epsilon() *
            std::max(1.0, std::fabs(value));
        const auto acceptable = [&] {
            return std::isfinite(candidate_value) &&
                   (candidate_value <= value ||
                    (candidate_value <= value + roundoff_margin &&
                     candidate_norm < current_norm));
        };
        std::size_t backtracks = 0;
        while (!acceptable() && backtracks < 60U) {
            step *= 0.5;
            candidate = candidate_for(step);
            candidate_value =
                candidate.empty()
                    ? std::numeric_limits<double>::infinity()
                    : quadratic_objective_unchecked(hessian, linear,
                                                    candidate);
            candidate_norm =
                candidate.empty()
                    ? std::numeric_limits<double>::infinity()
                    : projected_gradient_norm_at(candidate, stationarity_step);
            ++backtracks;
        }
        if (!acceptable()) {
            break;
        }
        const bool unchanged = candidate == point;
        const double norm = candidate_norm;
        result.iterations = iteration + 1U;
        result.projected_gradient_norm = norm;
        result.value = candidate_value;
        result.parameters = candidate;
        if (norm <= options.gradient_tolerance) {
            result.converged = true;
            point = std::move(candidate);
            value = candidate_value;
            break;
        }
        point = std::move(candidate);
        value = candidate_value;
        current_norm = norm;
        if (unchanged) {
            break;
        }
        if (step <= options.step_tolerance) {
            result.parameters = point;
            result.value = value;
            break;
        }
        step = std::min(step * 1.5, 10.0);
    }
    result.parameters = point;
    result.value = value;
    return result;
}

}  // namespace fincore::detail
