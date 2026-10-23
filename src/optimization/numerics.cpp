#include <fincore/detail/optimization.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace fincore::detail {
namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();

double objective_of(const std::function<double(const std::vector<double>&)>& fn,
                    const std::vector<double>& point) {
    const double value = fn(point);
    return std::isnan(value) ? std::numeric_limits<double>::infinity() : value;
}

}  // namespace

nelder_mead_result minimize_nelder_mead(
    const std::function<double(const std::vector<double>&)>& objective,
    std::vector<double> start, const nelder_mead_options& options) {
    nelder_mead_result result;
    const std::size_t dimensions = start.size();
    if (dimensions == 0U) {
        result.parameters = start;
        result.value = objective_of(objective, start);
        result.converged = true;
        return result;
    }

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

        const double spread =
            values.back() - values.front();
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
        result.iterations = iteration;
        if (spread <= options.function_tolerance &&
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

    result.parameters = simplex.front();
    result.value = values.front();
    return result;
}

std::vector<double> project_onto_simplex(const std::vector<double>& values,
                                         const double lower,
                                         const double upper,
                                         const double target_sum) {
    std::vector<double> result(values.size(), 0.0);
    if (values.empty()) {
        return result;
    }
    double low = -1.0;
    double high = 1.0;
    const double magnitude = *std::max_element(
        values.begin(), values.end(), [](const double left, const double right) {
            return std::fabs(left) < std::fabs(right);
        });
    low = -std::fabs(magnitude) - std::fabs(target_sum) - 1.0;
    high = std::fabs(magnitude) + std::fabs(target_sum) + 1.0;
    for (int iteration = 0; iteration < 200; ++iteration) {
        const double lambda = (low + high) / 2.0;
        double total = 0.0;
        for (const double value : values) {
            total += std::clamp(value + lambda, lower, upper);
        }
        if (total > target_sum) {
            high = lambda;
        } else {
            low = lambda;
        }
    }
    const double lambda = (low + high) / 2.0;
    for (std::size_t index = 0; index < values.size(); ++index) {
        result[index] = std::clamp(values[index] + lambda, lower, upper);
    }
    return result;
}

double quadratic_objective(const std::vector<double>& hessian,
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

projected_gradient_result minimize_quadratic_on_simplex(
    const std::vector<double>& hessian, const std::vector<double>& linear,
    std::vector<double> start, const projected_gradient_options& options) {
    projected_gradient_result result;
    const std::size_t dimensions = linear.size();
    if (dimensions == 0U) {
        result.converged = true;
        return result;
    }
    std::vector<double> point = project_onto_simplex(start);
    double value = quadratic_objective(hessian, linear, point);
    double step = 1.0;
    for (std::size_t iteration = 0; iteration < options.max_iterations;
         ++iteration) {
        std::vector<double> gradient(dimensions, 0.0);
        for (std::size_t row = 0; row < dimensions; ++row) {
            gradient[row] = linear[row];
            for (std::size_t column = 0; column < dimensions; ++column) {
                gradient[row] +=
                    hessian[row * dimensions + column] * point[column];
            }
        }
        const auto candidate_for = [&](const double trial_step) {
            std::vector<double> raw(dimensions);
            for (std::size_t index = 0; index < dimensions; ++index) {
                raw[index] = point[index] - trial_step * gradient[index];
            }
            return project_onto_simplex(raw);
        };
        std::vector<double> candidate = candidate_for(step);
        double candidate_value =
            quadratic_objective(hessian, linear, candidate);
        std::size_t backtracks = 0;
        while (candidate_value > value && backtracks < 60U) {
            step *= 0.5;
            candidate = candidate_for(step);
            candidate_value = quadratic_objective(hessian, linear, candidate);
            ++backtracks;
        }
        std::vector<double> projected_gradient(dimensions, 0.0);
        for (std::size_t index = 0; index < dimensions; ++index) {
            projected_gradient[index] = point[index] - candidate[index];
        }
        double norm = 0.0;
        for (const double component : projected_gradient) {
            norm += component * component;
        }
        norm = std::sqrt(norm) / std::max(step, options.step_tolerance);
        result.iterations = iteration;
        result.projected_gradient_norm = norm;
        result.value = candidate_value;
        result.parameters = candidate;
        if (norm <= options.gradient_tolerance || step <= options.step_tolerance) {
            result.converged = true;
            point = std::move(candidate);
            value = candidate_value;
            break;
        }
        point = std::move(candidate);
        value = candidate_value;
        step = std::min(step * 1.5, 10.0);
    }
    result.parameters = point;
    result.value = value;
    return result;
}

}  // namespace fincore::detail
