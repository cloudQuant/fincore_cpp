#include <fincore/attribution/style.hpp>

#include <fincore/detail/numeric.hpp>
#include <fincore/error.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <new>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fincore {
namespace {

constexpr std::size_t minimum_volatility_observations{20U};
constexpr std::size_t maximum_volatility_window{60U};

struct validated_inputs {
    std::vector<std::string> asset_names;
    std::vector<std::span<const double>> asset_returns;
};

struct style_weights {
    std::string_view name;
    std::vector<double> weights;
};

void require_finite(const double value, const char* const message) {
    if (!std::isfinite(value)) {
        throw input_error{message};
    }
}

void require_finite_values(const std::span<const double> values,
                           const char* const message) {
    for (const double value : values) {
        require_finite(value, message);
    }
}

void validate_optional_values(const std::span<const double> values,
                              const std::size_t asset_count,
                              const char* const message) {
    if (values.empty()) {
        return;
    }
    if (values.size() != asset_count) {
        throw input_error{message};
    }
    require_finite_values(values, message);
}

void validate_options(const style_analysis_options& options,
                      const std::size_t asset_count) {
    validate_optional_values(options.market_caps, asset_count,
                             "style analysis market caps must match assets and be finite");
    validate_optional_values(options.book_to_price, asset_count,
                             "style analysis book-to-price values must match assets and be finite");
    validate_optional_values(options.value_scores, asset_count,
                             "style analysis value scores must match assets and be finite");
    for (const double quantile : options.size_quantiles) {
        if (!std::isfinite(quantile) || quantile < 0.0 || quantile > 1.0) {
            throw input_error{"style analysis size quantiles must be finite values in [0, 1]"};
        }
    }
}

validated_inputs validate_asset_returns(const dataframe& asset_returns,
                                        const style_analysis_options& options) {
    const auto& index = asset_returns.get_index();
    if (index.empty()) {
        throw input_error{"style analysis requires at least one observation"};
    }

    const auto shape = asset_returns.shape();
    const auto double_columns = asset_returns.get_columns_info<double>();
    if (shape.second == 0U || double_columns.size() != shape.second) {
        throw input_error{"style analysis assets must contain only double columns"};
    }

    std::unordered_set<std::string> seen_names;
    seen_names.reserve(double_columns.size());
    std::vector<std::string> asset_names;
    std::vector<std::span<const double>> values;
    asset_names.reserve(double_columns.size());
    values.reserve(double_columns.size());
    for (const auto& info : double_columns) {
        const auto& raw_name = std::get<0>(info);
        if (raw_name.empty()) {
            throw input_error{"style analysis asset names must not be empty"};
        }
        if (raw_name.find('\0') != std::string::npos) {
            throw input_error{"style analysis asset names must not contain an embedded NUL"};
        }
        const std::string name{raw_name.c_str()};
        if (!seen_names.insert(name).second) {
            throw input_error{"style analysis asset names must be distinct"};
        }

        const auto& column = asset_returns.get_column<double>(name.c_str());
        if (column.size() != index.size()) {
            throw input_error{
                "style analysis asset columns must match the frame index length"};
        }
        require_finite_values(column, "style analysis returns must be finite");
        asset_names.push_back(name);
        values.emplace_back(column.data(), column.size());
    }
    validate_options(options, values.size());
    return {std::move(asset_names), std::move(values)};
}

std::vector<double> normalized_mask(std::vector<double> values) {
    double total{0.0};
    for (double& value : values) {
        require_finite(value, "style analysis exposure is not finite");
        value = std::max(value, 0.0);
        total += value;
        require_finite(total, "style analysis exposure total is not finite");
    }
    if (total > 0.0) {
        for (double& value : values) {
            value /= total;
        }
    }
    return values;
}

std::vector<double> mask_at_least(const std::span<const double> values,
                                  const double threshold) {
    std::vector<double> result;
    result.reserve(values.size());
    for (const double value : values) {
        result.push_back(value >= threshold ? 1.0 : 0.0);
    }
    return result;
}

std::vector<double> mask_below(const std::span<const double> values,
                               const double threshold) {
    std::vector<double> result;
    result.reserve(values.size());
    for (const double value : values) {
        result.push_back(value < threshold ? 1.0 : 0.0);
    }
    return result;
}

std::vector<double> mask_mid(const std::span<const double> values,
                             const double lower,
                             const double upper) {
    std::vector<double> result;
    result.reserve(values.size());
    for (const double value : values) {
        result.push_back(value >= lower && value < upper ? 1.0 : 0.0);
    }
    return result;
}

double checked_median(const std::span<const double> values,
                      const char* const message) {
    const double value = detail::linear_percentile(values, .5);
    require_finite(value, message);
    return value;
}

double compounded_return(const std::span<const double> returns,
                         const std::size_t start,
                         const std::size_t count) {
    double wealth{1.0};
    for (std::size_t offset{0U}; offset < count; ++offset) {
        const double multiplier{1.0 + returns[start + offset]};
        require_finite(multiplier, "style analysis momentum multiplier is not finite");
        wealth *= multiplier;
        require_finite(wealth, "style analysis momentum wealth is not finite");
    }
    const double result{wealth - 1.0};
    require_finite(result, "style analysis momentum return is not finite");
    return result;
}

double population_standard_deviation(const std::span<const double> values) {
    double mean{0.0};
    double sum_of_squares{0.0};
    std::size_t count{0U};
    for (const double value : values) {
        ++count;
        const double delta{value - mean};
        require_finite(delta, "style analysis volatility intermediate is not finite");
        mean += delta / static_cast<double>(count);
        require_finite(mean, "style analysis volatility intermediate is not finite");
        const double adjusted_delta{value - mean};
        const double increment{delta * adjusted_delta};
        require_finite(increment, "style analysis volatility intermediate is not finite");
        sum_of_squares += increment;
        require_finite(sum_of_squares,
                       "style analysis volatility intermediate is not finite");
    }
    const double variance{sum_of_squares / static_cast<double>(count)};
    require_finite(variance, "style analysis volatility is not finite");
    const double result{std::sqrt(variance)};
    require_finite(result, "style analysis volatility is not finite");
    return result;
}

std::vector<double> rolling_volatilities(const std::span<const double> returns) {
    std::vector<double> values;
    if (returns.size() < minimum_volatility_observations) {
        return values;
    }
    values.reserve(returns.size() - minimum_volatility_observations + 1U);
    for (std::size_t end{minimum_volatility_observations}; end <= returns.size();
         ++end) {
        const std::size_t start{
            end > maximum_volatility_window ? end - maximum_volatility_window : 0U};
        values.push_back(population_standard_deviation(
            returns.subspan(start, end - start)));
    }
    return values;
}

std::vector<timestamp> ordinal_index(const std::size_t count) {
    std::vector<timestamp> result;
    result.reserve(count);
    for (std::size_t position{0U}; position < count; ++position) {
        if (position > static_cast<std::size_t>(
                           std::numeric_limits<timestamp>::max())) {
            throw input_error{"style analysis ordinal index is not representable"};
        }
        result.push_back(static_cast<timestamp>(position));
    }
    return result;
}

std::vector<style_weights> calculate_styles(
    const std::vector<std::span<const double>>& asset_returns,
    const style_analysis_options& options) {
    const std::size_t asset_count{asset_returns.size()};
    std::vector<style_weights> styles;
    styles.reserve(options.market_caps.empty() ? 7U : 9U);

    const auto append = [&styles](const std::string_view name,
                                  std::vector<double> weights) {
        styles.push_back({name, normalized_mask(std::move(weights))});
    };

    if (options.market_caps.empty()) {
        append(style_equal_weight_column, std::vector<double>(asset_count, 1.0));
    } else {
        const double upper{detail::linear_percentile(
            options.market_caps, options.size_quantiles[1U])};
        require_finite(upper, "style analysis size percentile is not finite");
        const double selected_lower{detail::linear_percentile(
            options.market_caps, options.size_quantiles[0U])};
        require_finite(selected_lower, "style analysis size percentile is not finite");
        append(style_large_column, mask_at_least(options.market_caps, upper));
        append(style_mid_column, mask_mid(options.market_caps, selected_lower, upper));
        append(style_small_column, mask_below(options.market_caps, selected_lower));
    }

    const std::size_t observation_count{asset_returns.front().size()};
    const std::size_t lookback{std::min(options.momentum_window, observation_count)};
    std::vector<double> momentum_returns;
    momentum_returns.reserve(asset_count);
    for (const std::span<const double> values : asset_returns) {
        momentum_returns.push_back(compounded_return(
            values, observation_count - lookback, lookback));
    }
    const double momentum_threshold{checked_median(
        momentum_returns, "style analysis momentum median is not finite")};
    append(style_winner_column, mask_at_least(momentum_returns, momentum_threshold));
    append(style_loser_column, mask_below(momentum_returns, momentum_threshold));

    if (options.value_scores.empty() && options.book_to_price.empty()) {
        append(style_value_column, std::vector<double>(asset_count, 1.0));
        append(style_growth_column, std::vector<double>(asset_count, 1.0));
    } else {
        const std::span<const double> selected_scores{
            options.value_scores.empty() ? options.book_to_price : options.value_scores};
        const double threshold{checked_median(
            selected_scores, "style analysis value median is not finite")};
        append(style_value_column, mask_at_least(selected_scores, threshold));
        append(style_growth_column, mask_below(selected_scores, threshold));
    }

    std::vector<double> high_mask;
    std::vector<double> low_mask;
    high_mask.reserve(asset_count);
    low_mask.reserve(asset_count);
    for (const std::span<const double> values : asset_returns) {
        const auto volatility = rolling_volatilities(values);
        if (volatility.empty()) {
            high_mask.push_back(0.0);
            low_mask.push_back(0.0);
            continue;
        }
        const double median{checked_median(
            volatility, "style analysis volatility median is not finite")};
        high_mask.push_back(volatility.back() >= median ? 1.0 : 0.0);
        low_mask.push_back(volatility.back() < median ? 1.0 : 0.0);
    }
    append(style_high_column, std::move(high_mask));
    append(style_low_column, std::move(low_mask));
    return styles;
}

double style_return(const std::vector<std::span<const double>>& asset_returns,
                    const std::span<const double> weights) {
    double total_weight{0.0};
    for (const double weight : weights) {
        total_weight += weight;
        require_finite(total_weight, "style analysis weight total is not finite");
    }
    if (total_weight == 0.0) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    double total{0.0};
    const std::size_t observation_count{asset_returns.front().size()};
    for (std::size_t row{0U}; row < observation_count; ++row) {
        double daily_return{0.0};
        for (std::size_t asset{0U}; asset < asset_returns.size(); ++asset) {
            const double contribution{asset_returns[asset][row] * weights[asset]};
            require_finite(contribution, "style analysis style return is not finite");
            daily_return += contribution;
            require_finite(daily_return, "style analysis style return is not finite");
        }
        total += daily_return;
        require_finite(total, "style analysis style return is not finite");
    }
    const double result{total / static_cast<double>(observation_count)};
    require_finite(result, "style analysis style return is not finite");
    return result;
}

std::vector<double> overall_returns(
    const std::vector<std::span<const double>>& asset_returns) {
    const std::size_t asset_count{asset_returns.size()};
    const std::size_t observation_count{asset_returns.front().size()};
    const double weight{1.0 / static_cast<double>(asset_count)};
    require_finite(weight, "style analysis equal weight is not finite");

    std::vector<double> values;
    values.reserve(observation_count);
    for (std::size_t row{0U}; row < observation_count; ++row) {
        double value{0.0};
        for (const std::span<const double> asset : asset_returns) {
            const double contribution{asset[row] * weight};
            require_finite(contribution, "style analysis overall return is not finite");
            value += contribution;
            require_finite(value, "style analysis overall return is not finite");
        }
        values.push_back(value);
    }
    return values;
}

style_analysis_result materialize_result(
    const dataframe& asset_returns,
    std::vector<std::string> asset_names,
    const std::vector<std::span<const double>>& values,
    std::vector<style_weights> styles) {
    std::vector<std::string> style_names;
    std::vector<double> style_returns;
    style_names.reserve(styles.size());
    style_returns.reserve(styles.size());
    for (const style_weights& style : styles) {
        style_names.emplace_back(style.name);
        style_returns.push_back(style_return(values, style.weights));
    }

    dataframe exposures;
    exposures.load_index(ordinal_index(asset_names.size()));
    exposures.load_column<std::string>(style_asset_column.data(), std::move(asset_names),
                                       hmdf::nan_policy::dont_pad_with_nans);
    for (auto& style : styles) {
        exposures.load_column<double>(style.name.data(), std::move(style.weights),
                                      hmdf::nan_policy::dont_pad_with_nans);
    }

    dataframe summary;
    summary.load_index(ordinal_index(style_names.size()));
    summary.load_column<std::string>(style_name_column.data(), std::move(style_names),
                                     hmdf::nan_policy::dont_pad_with_nans);
    summary.load_column<double>(style_return_column.data(), std::move(style_returns),
                                hmdf::nan_policy::dont_pad_with_nans);

    dataframe overall;
    const auto& input_index = asset_returns.get_index();
    overall.load_index(std::vector<timestamp>{input_index.begin(), input_index.end()});
    overall.load_column<double>(style_return_column.data(), overall_returns(values),
                                hmdf::nan_policy::dont_pad_with_nans);
    return {std::move(exposures), std::move(summary), std::move(overall)};
}

style_analysis_result style_analysis_impl(const dataframe& asset_returns,
                                          const style_analysis_options& options) {
    auto inputs = validate_asset_returns(asset_returns, options);
    auto styles = calculate_styles(inputs.asset_returns, options);
    return materialize_result(asset_returns, std::move(inputs.asset_names),
                              inputs.asset_returns, std::move(styles));
}

}  // namespace

style_analysis_result style_analysis(const dataframe& asset_returns,
                                     const style_analysis_options& options) {
    try {
        return style_analysis_impl(asset_returns, options);
    } catch (const std::bad_alloc&) {
        throw input_error{"style analysis allocation failed"};
    } catch (const std::length_error&) {
        throw input_error{"style analysis allocation is not representable"};
    }
}

}  // namespace fincore
