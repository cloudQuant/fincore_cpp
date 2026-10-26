#include <fincore/attribution/factor_regression.hpp>

#include <fincore/error.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fincore {
namespace {

struct validated_inputs {
    std::vector<std::string> factor_names;
    std::span<const double> returns;
    std::vector<std::span<const double>> factors;
};

// The output frame is keyed by these same names plus the reserved alpha column, so
// a factor may not be called alpha: it would otherwise arrive both as an input and
// as the result. Names are compared as owned strings and must be distinct before
// anything is read from the frames.
std::vector<std::string> checked_factor_names(
    const std::span<const std::string_view> factor_column_names) {
    if (factor_column_names.empty()) {
        throw input_error{"factor column names must not be empty"};
    }

    std::unordered_set<std::string> seen;
    seen.reserve(factor_column_names.size());
    std::vector<std::string> names;
    names.reserve(factor_column_names.size());
    for (const std::string_view name : factor_column_names) {
        if (name.empty()) {
            throw input_error{"factor column names must not contain an empty name"};
        }
        if (name == factor_regression_alpha_name) {
            throw input_error{"factor column names must not contain alpha"};
        }

        const std::string owned_name{name};
        if (!seen.insert(owned_name).second) {
            throw input_error{"factor column names must be distinct"};
        }
        names.push_back(owned_name);
    }
    return names;
}

void require_unique_index(const dataframe& frame) {
    std::unordered_set<timestamp> seen;
    seen.reserve(frame.get_index().size());
    for (const timestamp value : frame.get_index()) {
        if (!seen.insert(value).second) {
            throw input_error{"factor regression indexes must not contain duplicates"};
        }
    }
}

void require_matching_indexes(const dataframe& returns,
                              const dataframe& factor_returns) {
    require_unique_index(returns);
    require_unique_index(factor_returns);
    if (returns.get_index() != factor_returns.get_index()) {
        throw input_error{"factor regression indexes must match exactly"};
    }
}

void require_finite_values(const std::span<const double> values) {
    for (const double value : values) {
        if (!std::isfinite(value)) {
            throw input_error{"factor regression selected values must be finite"};
        }
    }
}

bool values_are_exactly_constant(const std::span<const double> values) {
    const double first_value = values.front();
    for (const double value : values) {
        if (value != first_value) {
            return false;
        }
    }
    return true;
}

void require_finite_intermediate(const double value) {
    if (!std::isfinite(value)) {
        throw input_error{"factor regression numerical intermediate is not finite"};
    }
}

validated_inputs validate_inputs(
    const dataframe& returns,
    const dataframe& factor_returns,
    const std::span<const std::string_view> factor_column_names,
    const std::string_view returns_column_name) {
    auto factor_names = checked_factor_names(factor_column_names);
    require_matching_indexes(returns, factor_returns);

    const auto return_values = require_double_column(returns, returns_column_name);
    require_finite_values(return_values);

    std::vector<std::span<const double>> factor_values;
    factor_values.reserve(factor_names.size());
    for (const std::string& name : factor_names) {
        const auto values = require_double_column(factor_returns, name);
        require_finite_values(values);
        factor_values.push_back(values);
    }

    if (returns.get_index().size() < factor_names.size() + 1U) {
        throw input_error{"factor regression requires at least K + 1 observations"};
    }
    return {std::move(factor_names), return_values, std::move(factor_values)};
}

// Householder QR on the design itself, unlike the normal-equation solves used by
// the timing and factor-beta paths: forming X'X would square the condition number
// of the design, and this factor exposure is read as a coefficient rather than as
// a ratio that cancels. The rank test is scaled by epsilon times the larger
// dimension, and a reflection whose denominator vanishes is reported as rank
// deficiency rather than divided through.
std::vector<double> solve_householder_qr(std::vector<double> matrix,
                                         std::vector<double> response,
                                         const std::size_t row_count,
                                         const std::size_t column_count,
                                         const double input_scale) {
    const double dimension = static_cast<double>(
        row_count > column_count ? row_count : column_count);
    const double relative_rank_tolerance =
        std::numeric_limits<double>::epsilon() * dimension;
    require_finite_intermediate(relative_rank_tolerance);

    for (std::size_t column = 0; column < column_count; ++column) {
        const std::size_t diagonal = column * column_count + column;
        double tail_norm = 0.0;
        for (std::size_t row = column + 1U; row < row_count; ++row) {
            tail_norm = std::hypot(tail_norm, matrix[row * column_count + column]);
            require_finite_intermediate(tail_norm);
        }

        const double leading = matrix[diagonal];
        require_finite_intermediate(leading);
        if (tail_norm != 0.0) {
            const double beta = -std::copysign(std::hypot(leading, tail_norm), leading);
            require_finite_intermediate(beta);
            const double denominator = leading - beta;
            require_finite_intermediate(denominator);
            if (denominator == 0.0) {
                throw input_error{"factor regression design is rank deficient"};
            }
            const double tau = (beta - leading) / beta;
            require_finite_intermediate(tau);

            matrix[diagonal] = beta;
            for (std::size_t row = column + 1U; row < row_count; ++row) {
                matrix[row * column_count + column] /= denominator;
                require_finite_intermediate(matrix[row * column_count + column]);
            }

            for (std::size_t target = column + 1U; target < column_count; ++target) {
                double projection = matrix[column * column_count + target];
                for (std::size_t row = column + 1U; row < row_count; ++row) {
                    projection += matrix[row * column_count + column] *
                                  matrix[row * column_count + target];
                    require_finite_intermediate(projection);
                }
                projection *= tau;
                require_finite_intermediate(projection);
                matrix[column * column_count + target] -= projection;
                require_finite_intermediate(matrix[column * column_count + target]);
                for (std::size_t row = column + 1U; row < row_count; ++row) {
                    matrix[row * column_count + target] -=
                        matrix[row * column_count + column] * projection;
                    require_finite_intermediate(matrix[row * column_count + target]);
                }
            }

            double response_projection = response[column];
            for (std::size_t row = column + 1U; row < row_count; ++row) {
                response_projection += matrix[row * column_count + column] * response[row];
                require_finite_intermediate(response_projection);
            }
            response_projection *= tau;
            require_finite_intermediate(response_projection);
            response[column] -= response_projection;
            require_finite_intermediate(response[column]);
            for (std::size_t row = column + 1U; row < row_count; ++row) {
                response[row] -= matrix[row * column_count + column] * response_projection;
                require_finite_intermediate(response[row]);
            }
        }

        const double diagonal_magnitude = std::abs(matrix[diagonal]);
        require_finite_intermediate(diagonal_magnitude);
        if (!(diagonal_magnitude / input_scale > relative_rank_tolerance)) {
            throw input_error{"factor regression design is rank deficient"};
        }
    }

    std::vector<double> coefficients(column_count, 0.0);
    for (std::size_t offset = 0; offset < column_count; ++offset) {
        const std::size_t row = column_count - 1U - offset;
        double value = response[row];
        for (std::size_t column = row + 1U; column < column_count; ++column) {
            value -= matrix[row * column_count + column] * coefficients[column];
            require_finite_intermediate(value);
        }
        value /= matrix[row * column_count + row];
        require_finite_intermediate(value);
        coefficients[row] = value;
    }
    return coefficients;
}

dataframe make_coefficients_frame(const std::vector<std::string>& factor_names,
                                  std::vector<double> coefficients) {
    std::vector<timestamp> index;
    index.reserve(coefficients.size());
    for (std::size_t row = 0; row < coefficients.size(); ++row) {
        if (row > static_cast<std::size_t>(std::numeric_limits<timestamp>::max())) {
            throw input_error{"factor regression coefficient index is not representable"};
        }
        index.push_back(static_cast<timestamp>(row));
    }

    std::vector<std::string> labels;
    labels.reserve(coefficients.size());
    labels.emplace_back(factor_regression_alpha_name);
    labels.insert(labels.end(), factor_names.begin(), factor_names.end());

    dataframe result;
    result.load_index(std::move(index));
    result.load_column<std::string>(factor_regression_factor_column.data(),
                                    std::move(labels),
                                    hmdf::nan_policy::dont_pad_with_nans);
    result.load_column<double>(factor_regression_coefficient_column.data(),
                               std::move(coefficients),
                               hmdf::nan_policy::dont_pad_with_nans);
    return result;
}

dataframe make_return_frame(const std::vector<timestamp>& index,
                            const std::string_view column_name,
                            std::vector<double> values) {
    dataframe result;
    result.load_index(std::vector<timestamp>{index});
    result.load_column<double>(column_name.data(), std::move(values),
                               hmdf::nan_policy::dont_pad_with_nans);
    return result;
}

}  // namespace

factor_regression_result fit_factor_regression(
    const dataframe& returns,
    const dataframe& factor_returns,
    const std::span<const std::string_view> factor_column_names,
    const std::string_view returns_column_name) {
    const auto inputs = validate_inputs(returns, factor_returns, factor_column_names,
                                        returns_column_name);
    const std::size_t row_count = returns.get_index().size();
    const std::size_t column_count = inputs.factor_names.size() + 1U;
    if (row_count > std::numeric_limits<std::size_t>::max() / column_count) {
        throw input_error{"factor regression design is too large"};
    }

    std::vector<double> design(row_count * column_count, 1.0);
    double input_scale = 1.0;
    for (std::size_t row = 0; row < row_count; ++row) {
        for (std::size_t factor = 0; factor < inputs.factors.size(); ++factor) {
            const double value = inputs.factors[factor][row];
            design[row * column_count + factor + 1U] = value;
            input_scale = std::max(input_scale, std::abs(value));
        }
    }
    require_finite_intermediate(input_scale);

    const auto original_design = design;
    const auto coefficients = solve_householder_qr(
        std::move(design), std::vector<double>{inputs.returns.begin(), inputs.returns.end()},
        row_count, column_count, input_scale);

    std::vector<double> fitted_values;
    std::vector<double> residual_values;
    fitted_values.reserve(row_count);
    residual_values.reserve(row_count);
    double response_total = 0.0;
    for (std::size_t row = 0; row < row_count; ++row) {
        double fitted = 0.0;
        for (std::size_t column = 0; column < column_count; ++column) {
            fitted += original_design[row * column_count + column] * coefficients[column];
            require_finite_intermediate(fitted);
        }
        const double residual = inputs.returns[row] - fitted;
        require_finite_intermediate(residual);
        fitted_values.push_back(fitted);
        residual_values.push_back(residual);
        response_total += inputs.returns[row];
        require_finite_intermediate(response_total);
    }

    const double response_mean = response_total / static_cast<double>(row_count);
    require_finite_intermediate(response_mean);
    double sse = 0.0;
    double sst = 0.0;
    for (std::size_t row = 0; row < row_count; ++row) {
        sse += residual_values[row] * residual_values[row];
        sst += (inputs.returns[row] - response_mean) *
               (inputs.returns[row] - response_mean);
        require_finite_intermediate(sse);
        require_finite_intermediate(sst);
    }
    // An exactly constant series has no variance to explain, so the statistic is
    // reported as zero rather than as a perfect fit even though every residual is
    // zero; the test is on equality with the first value, so a factor or return
    // varying by one representable step counts as varying.
    const double r_squared = values_are_exactly_constant(inputs.returns)
                                 ? 0.0
                                 : (sst > 0.0 ? 1.0 - sse / sst : 0.0);
    require_finite_intermediate(r_squared);

    return {coefficients[0], r_squared,
            make_coefficients_frame(inputs.factor_names, coefficients),
            make_return_frame(returns.get_index(), factor_regression_fitted_column,
                              std::move(fitted_values)),
            make_return_frame(returns.get_index(), factor_regression_residual_column,
                              std::move(residual_values))};
}

}  // namespace fincore
