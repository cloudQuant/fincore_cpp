#include <fincore/attribution/factor_prediction.hpp>

#include <fincore/error.hpp>

#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <typeindex>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fincore {
namespace {

struct validated_regression {
    double alpha;
    std::vector<double> coefficients;
};

void require_finite(const double value, const std::string_view context) {
    if (!std::isfinite(value)) {
        throw input_error{std::string{"factor regression prediction "} +
                          std::string{context} + " is not finite"};
    }
}

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
        if (name.find('\0') != std::string_view::npos) {
            throw input_error{"factor column names must not contain an embedded NUL"};
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

template <typename Columns>
bool has_expected_column(const Columns& columns,
                         const char* name,
                         const std::type_index type,
                         const std::size_t expected_size) {
    for (const auto& column : columns) {
        if (std::get<0>(column) == name &&
            std::get<1>(column) == expected_size &&
            std::get<2>(column) == type) {
            return true;
        }
    }
    return false;
}

validated_regression validate_regression(
    const factor_regression_result& regression,
    const std::vector<std::string>& factor_names) {
    require_finite(regression.alpha, "alpha");

    const std::size_t factor_count = factor_names.size();
    if (factor_count == std::numeric_limits<std::size_t>::max()) {
        throw input_error{"factor regression coefficient table is too large"};
    }
    const std::size_t expected_size = factor_count + 1U;
    const auto& coefficient_index = regression.coefficients.get_index();
    if (coefficient_index.size() != expected_size ||
        regression.coefficients.shape().second != 2U) {
        throw input_error{"factor regression coefficients are malformed"};
    }

    const auto columns = regression.coefficients.get_columns_info<std::string, double>();
    if (columns.size() != 2U ||
        !has_expected_column(columns, factor_regression_factor_column.data(),
                             typeid(std::string), expected_size) ||
        !has_expected_column(columns, factor_regression_coefficient_column.data(),
                             typeid(double), expected_size)) {
        throw input_error{"factor regression coefficients are malformed"};
    }

    for (std::size_t row = 0; row < expected_size; ++row) {
        if (row > static_cast<std::size_t>(std::numeric_limits<timestamp>::max()) ||
            coefficient_index[row] != static_cast<timestamp>(row)) {
            throw input_error{"factor regression coefficient index is malformed"};
        }
    }

    const auto& labels = regression.coefficients.get_column<std::string>(
        factor_regression_factor_column.data());
    const auto& coefficient_values = regression.coefficients.get_column<double>(
        factor_regression_coefficient_column.data());
    if (labels.size() != expected_size || coefficient_values.size() != expected_size ||
        labels[0] != factor_regression_alpha_name) {
        throw input_error{"factor regression coefficients are malformed"};
    }
    for (std::size_t factor = 0; factor < factor_count; ++factor) {
        if (labels[factor + 1U] != factor_names[factor]) {
            throw input_error{"factor regression coefficient labels do not match selectors"};
        }
    }
    for (const double coefficient : coefficient_values) {
        require_finite(coefficient, "coefficient");
    }
    if (regression.alpha != coefficient_values[0]) {
        throw input_error{"factor regression alpha does not match coefficients"};
    }

    return {regression.alpha,
            std::vector<double>{coefficient_values.begin(), coefficient_values.end()}};
}

std::vector<std::span<const double>> validate_factor_values(
    const dataframe& factor_returns,
    const std::vector<std::string>& factor_names) {
    std::vector<std::span<const double>> factor_values;
    factor_values.reserve(factor_names.size());
    for (const std::string& name : factor_names) {
        const auto values = require_double_column(factor_returns, name);
        for (const double value : values) {
            require_finite(value, "selected factor value");
        }
        factor_values.push_back(values);
    }
    return factor_values;
}

}  // namespace

dataframe factor_regression_predictions(
    const factor_regression_result& regression,
    const dataframe& factor_returns,
    const std::span<const std::string_view> factor_column_names) {
    const auto factor_names = checked_factor_names(factor_column_names);
    const auto checked_regression = validate_regression(regression, factor_names);
    const auto factor_values = validate_factor_values(factor_returns, factor_names);

    const auto& factor_index = factor_returns.get_index();
    std::vector<double> predictions;
    predictions.reserve(factor_index.size());
    for (std::size_t row = 0; row < factor_index.size(); ++row) {
        double prediction = checked_regression.alpha;
        for (std::size_t factor = 0; factor < factor_values.size(); ++factor) {
            const double contribution =
                checked_regression.coefficients[factor + 1U] * factor_values[factor][row];
            require_finite(contribution, "coefficient product");
            prediction += contribution;
            require_finite(prediction, "running prediction");
        }
        predictions.push_back(prediction);
    }

    dataframe result;
    result.load_index(std::vector<timestamp>{factor_index.begin(), factor_index.end()});
    result.load_column<double>(factor_prediction_column.data(), std::move(predictions),
                               hmdf::nan_policy::dont_pad_with_nans);
    return result;
}

}  // namespace fincore
