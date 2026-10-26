#include <fincore/attribution/factor_exposures.hpp>

#include <fincore/error.hpp>

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
    std::string returns_column_name;
    std::span<const double> returns;
    std::vector<std::span<const double>> factors;
};

// Every module in this family re-declares the same selector rules as a private
// helper instead of sharing one, so a rule changed here has to be mirrored in the
// regression, prediction and decomposition entry points or the three will accept
// different inputs for the same coefficient table.
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

std::string checked_returns_column_name(const std::string_view returns_column_name) {
    if (returns_column_name.find('\0') != std::string_view::npos) {
        throw input_error{"returns column name must not contain an embedded NUL"};
    }
    return std::string{returns_column_name};
}

void require_unique_index(const dataframe& frame) {
    std::unordered_set<timestamp> seen;
    seen.reserve(frame.get_index().size());
    for (const timestamp value : frame.get_index()) {
        if (!seen.insert(value).second) {
            throw input_error{"factor exposure indexes must not contain duplicates"};
        }
    }
}

void require_matching_indexes(const dataframe& returns,
                              const dataframe& factor_returns) {
    require_unique_index(returns);
    require_unique_index(factor_returns);
    if (returns.get_index() != factor_returns.get_index()) {
        throw input_error{"factor exposure indexes must match exactly"};
    }
}

void require_finite_values(const std::span<const double> values) {
    for (const double value : values) {
        if (!std::isfinite(value)) {
            throw input_error{"factor exposure selected values must be finite"};
        }
    }
}

validated_inputs validate_inputs(
    const dataframe& returns,
    const dataframe& factor_returns,
    const std::span<const std::string_view> factor_column_names,
    const std::string_view returns_column_name) {
    auto factor_names = checked_factor_names(factor_column_names);
    auto owned_returns_column_name = checked_returns_column_name(returns_column_name);
    require_matching_indexes(returns, factor_returns);

    const auto return_values = require_double_column(returns, owned_returns_column_name);
    std::vector<std::span<const double>> factor_values;
    factor_values.reserve(factor_names.size());
    for (const std::string& name : factor_names) {
        factor_values.push_back(require_double_column(factor_returns, name));
    }

    require_finite_values(return_values);
    for (const std::span<const double> values : factor_values) {
        require_finite_values(values);
    }
    return {std::move(factor_names), std::move(owned_returns_column_name), return_values,
            std::move(factor_values)};
}

dataframe materialize_returns_slice(const std::vector<timestamp>& index,
                                    const std::span<const double> values,
                                    const std::size_t start,
                                    const std::size_t count,
                                    const std::string& returns_column_name) {
    dataframe result;
    result.load_index(std::vector<timestamp>{index.begin() + start,
                                             index.begin() + start + count});
    result.load_column<double>(
        returns_column_name.c_str(),
        std::vector<double>{values.begin() + start, values.begin() + start + count},
        hmdf::nan_policy::dont_pad_with_nans);
    return result;
}

dataframe materialize_factor_slice(
    const std::vector<timestamp>& index,
    const std::vector<std::string>& factor_names,
    const std::vector<std::span<const double>>& factor_values,
    const std::size_t start,
    const std::size_t count) {
    dataframe result;
    result.load_index(std::vector<timestamp>{index.begin() + start,
                                             index.begin() + start + count});
    for (std::size_t factor = 0; factor < factor_names.size(); ++factor) {
        const auto values = factor_values[factor];
        result.load_column<double>(
            factor_names[factor].c_str(),
            std::vector<double>{values.begin() + start, values.begin() + start + count},
            hmdf::nan_policy::dont_pad_with_nans);
    }
    return result;
}

std::vector<std::string_view> make_factor_name_views(
    const std::vector<std::string>& factor_names) {
    std::vector<std::string_view> views;
    views.reserve(factor_names.size());
    for (const std::string& name : factor_names) {
        views.push_back(name);
    }
    return views;
}

}  // namespace

dataframe rolling_factor_exposures(
    const dataframe& returns,
    const dataframe& factor_returns,
    const std::span<const std::string_view> factor_column_names,
    const std::optional<std::size_t> rolling_window,
    const std::string_view returns_column_name) {
    if (rolling_window && *rolling_window == 0U) {
        throw input_error{"rolling window must be positive"};
    }

    const auto inputs = validate_inputs(returns, factor_returns, factor_column_names,
                                        returns_column_name);
    const auto& index = returns.get_index();
    const std::size_t factor_count = inputs.factor_names.size();
    const std::size_t output_column_count = factor_count + 1U;
    const std::size_t minimum_observations = output_column_count;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    std::vector<std::vector<double>> exposure_values(
        output_column_count, std::vector<double>(index.size(), nan));
    const auto factor_name_views = make_factor_name_views(inputs.factor_names);

    for (std::size_t row = 0; row < index.size(); ++row) {
        const std::size_t end = row + 1U;
        const std::size_t start =
            rolling_window && end > *rolling_window ? end - *rolling_window : 0U;
        const std::size_t observation_count = end - start;
        if (observation_count < minimum_observations) {
            continue;
        }

        const auto selected_returns = materialize_returns_slice(
            index, inputs.returns, start, observation_count, inputs.returns_column_name);
        const auto selected_factors = materialize_factor_slice(
            index, inputs.factor_names, inputs.factors, start, observation_count);
        const auto regression = fit_factor_regression(
            selected_returns, selected_factors, factor_name_views,
            inputs.returns_column_name);
        const auto& coefficients = regression.coefficients.get_column<double>(
            factor_regression_coefficient_column.data());
        exposure_values[0][row] = regression.alpha;
        for (std::size_t factor = 0; factor < factor_count; ++factor) {
            exposure_values[factor + 1U][row] = coefficients[factor + 1U];
        }
    }

    dataframe result;
    result.load_index(std::vector<timestamp>{index});
    result.load_column<double>(factor_regression_alpha_name.data(),
                               std::move(exposure_values[0]),
                               hmdf::nan_policy::dont_pad_with_nans);
    for (std::size_t factor = 0; factor < factor_count; ++factor) {
        result.load_column<double>(inputs.factor_names[factor].c_str(),
                                   std::move(exposure_values[factor + 1U]),
                                   hmdf::nan_policy::dont_pad_with_nans);
    }
    return result;
}

}  // namespace fincore
