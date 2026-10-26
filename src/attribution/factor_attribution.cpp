#include <fincore/attribution/factor_attribution.hpp>

#include <fincore/error.hpp>

#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fincore {
namespace {

struct validated_selectors {
    std::vector<std::string> factor_names;
    std::string returns_column_name;
};

void require_finite(const double value, const std::string_view context) {
    if (!std::isfinite(value)) {
        throw input_error{std::string{"factor attribution "} +
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

std::string checked_returns_column_name(const std::string_view returns_column_name) {
    if (returns_column_name.empty()) {
        throw input_error{"returns column name must not be empty"};
    }
    if (returns_column_name.find('\0') != std::string_view::npos) {
        throw input_error{"returns column name must not contain an embedded NUL"};
    }
    return std::string{returns_column_name};
}

validated_selectors validate_selectors(
    const std::span<const std::string_view> factor_column_names,
    const std::string_view returns_column_name,
    const double risk_free_rate) {
    require_finite(risk_free_rate, "risk-free rate");
    return {checked_factor_names(factor_column_names),
            checked_returns_column_name(returns_column_name)};
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

// Re-checked after every addition rather than at the end, so an accumulation that
// leaves the finite range is reported with the context of the statistic it was
// feeding instead of surfacing as a NaN the caller has to trace back.
double checked_arithmetic_mean(const std::span<const double> values,
                               const std::string_view context) {
    double total = 0.0;
    for (const double value : values) {
        total += value;
        require_finite(total, context);
    }
    const double mean = total / static_cast<double>(values.size());
    require_finite(mean, context);
    return mean;
}

// The decomposition frames are labelled by row position rather than by the dates
// they were computed from, which is what lets several per-date results be
// concatenated into one frame without a duplicated index.
void append_ordinal_index(std::vector<timestamp>& index,
                          const std::size_t row_count) {
    index.reserve(row_count);
    for (std::size_t row = 0; row < row_count; ++row) {
        if (row > static_cast<std::size_t>(std::numeric_limits<timestamp>::max())) {
            throw input_error{"factor attribution index is not representable"};
        }
        index.push_back(static_cast<timestamp>(row));
    }
}

}  // namespace

dataframe factor_attribution_decomposition(
    const dataframe& returns,
    const dataframe& factor_returns,
    const std::span<const std::string_view> factor_column_names,
    const double risk_free_rate,
    const std::string_view returns_column_name) {
    const auto selectors = validate_selectors(
        factor_column_names, returns_column_name, risk_free_rate);
    const auto factor_name_views = make_factor_name_views(selectors.factor_names);

    const auto regression = fit_factor_regression(
        returns, factor_returns, factor_name_views, selectors.returns_column_name);

    const std::size_t factor_count = selectors.factor_names.size();
    if (factor_count > std::numeric_limits<std::size_t>::max() - 4U) {
        throw input_error{"factor attribution result is too large"};
    }
    const std::size_t result_row_count = factor_count + 4U;
    const auto& coefficients = regression.coefficients.get_column<double>(
        factor_regression_coefficient_column.data());
    if (coefficients.size() != factor_count + 1U) {
        throw input_error{"factor attribution regression coefficients are malformed"};
    }
    require_finite(regression.alpha, "alpha");

    std::vector<std::string> components;
    std::vector<double> contributions;
    components.reserve(result_row_count);
    contributions.reserve(result_row_count);
    components.emplace_back(factor_regression_alpha_name);
    contributions.push_back(regression.alpha);

    double specific_return = 0.0;
    for (std::size_t factor = 0; factor < factor_count; ++factor) {
        const auto factor_values = require_double_column(
            factor_returns, selectors.factor_names[factor]);
        const double factor_mean = checked_arithmetic_mean(
            factor_values, "factor mean");
        const double contribution = coefficients[factor + 1U] * factor_mean;
        require_finite(contribution, "factor contribution");
        specific_return += contribution;
        require_finite(specific_return, "specific return");
        components.emplace_back(selectors.factor_names[factor] + "_attribution");
        contributions.push_back(contribution);
    }

    const auto return_values = require_double_column(
        returns, selectors.returns_column_name);
    const double return_mean = checked_arithmetic_mean(return_values, "return mean");
    const double common_return = return_mean - risk_free_rate;
    require_finite(common_return, "common return");
    const double after_alpha = common_return - regression.alpha;
    require_finite(after_alpha, "unexplained return");
    const double unexplained = after_alpha - specific_return;
    require_finite(unexplained, "unexplained return");

    components.emplace_back("specific_return");
    contributions.push_back(specific_return);
    components.emplace_back("common_return");
    contributions.push_back(common_return);
    components.emplace_back("unexplained");
    contributions.push_back(unexplained);

    std::vector<timestamp> index;
    append_ordinal_index(index, result_row_count);
    dataframe result;
    result.load_index(std::move(index));
    result.load_column<std::string>(factor_attribution_component_column.data(),
                                    std::move(components),
                                    hmdf::nan_policy::dont_pad_with_nans);
    result.load_column<double>(factor_attribution_contribution_column.data(),
                               std::move(contributions),
                               hmdf::nan_policy::dont_pad_with_nans);
    return result;
}

}  // namespace fincore
