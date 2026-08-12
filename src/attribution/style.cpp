#include <fincore/attribution/style.hpp>

#include <fincore/detail/numeric.hpp>
#include <fincore/error.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <new>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
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

struct performance_returns {
    std::vector<std::string> asset_names;
    std::vector<std::span<const double>> values;
    std::size_t row_count;
};

struct performance_style {
    std::string output_name;
    std::vector<double> aligned_weights;
};

[[nodiscard]] bool is_empty_shaped(const dataframe& frame) {
    const auto shape = frame.shape();
    return shape.first == 0U || shape.second == 0U;
}

std::string copy_external_name(const dataframe::ColNameType& raw_name,
                               const char* const empty_message,
                               const char* const nul_message) {
    const std::string name{raw_name.begin(), raw_name.end()};
    if (name.empty()) {
        throw input_error{empty_message};
    }
    if (name.find('\0') != std::string::npos) {
        throw input_error{nul_message};
    }
    return name;
}

performance_returns validate_performance_returns(const dataframe& asset_returns) {
    const auto& index = asset_returns.get_index();
    const auto shape = asset_returns.shape();
    const auto double_columns = asset_returns.get_columns_info<double>();
    if (shape.first != index.size() || double_columns.size() != shape.second) {
        throw input_error{
            "style performance returns must contain only double columns"};
    }
    if (index.size() - 1U > static_cast<std::size_t>(
                               std::numeric_limits<timestamp>::max())) {
        throw input_error{"style performance ordinal index is not representable"};
    }
    if (index.size() > std::vector<timestamp>{}.max_size() ||
        double_columns.size() > std::vector<std::string>{}.max_size() ||
        double_columns.size() >
            std::vector<std::span<const double>>{}.max_size()) {
        throw input_error{"style performance returns are not representable"};
    }

    performance_returns result;
    result.row_count = index.size();
    result.asset_names.reserve(double_columns.size());
    result.values.reserve(double_columns.size());
    std::unordered_set<std::string> seen_names;
    seen_names.reserve(double_columns.size());
    for (const auto& info : double_columns) {
        std::string name = copy_external_name(
            std::get<0>(info),
            "style performance return asset names must not be empty",
            "style performance return asset names must not contain an embedded NUL");
        if (!seen_names.insert(name).second) {
            throw input_error{
                "style performance return asset names must be distinct"};
        }
        const auto& values = asset_returns.get_column<double>(name.c_str());
        if (values.size() != index.size()) {
            throw input_error{
                "style performance return columns must match the frame index length"};
        }
        require_finite_values(values,
                              "style performance returns must be finite");
        result.asset_names.push_back(std::move(name));
        result.values.emplace_back(values.data(), values.size());
    }
    return result;
}

std::string performance_output_name(const std::string& style_name) {
    constexpr std::string_view suffix{"_return"};
    const std::size_t capacity = dataframe::ColNameType::capacity();
    if (capacity == 0U || style_name.size() >= capacity ||
        suffix.size() >= capacity - style_name.size()) {
        throw input_error{
            "style performance output column name is not representable"};
    }
    return style_name + std::string{suffix};
}

std::vector<performance_style> validate_and_align_performance_styles(
    const dataframe& style_exposures,
    const std::vector<std::string>& return_asset_names) {
    const auto& index = style_exposures.get_index();
    const auto shape = style_exposures.shape();
    const auto string_columns = style_exposures.get_columns_info<std::string>();
    const auto double_columns = style_exposures.get_columns_info<double>();
    if (shape.first != index.size() || string_columns.size() != 1U ||
        double_columns.empty() ||
        string_columns.size() + double_columns.size() != shape.second) {
        throw input_error{
            "style performance exposures must contain one asset string column and double style columns"};
    }
    for (std::size_t row{0U}; row < index.size(); ++row) {
        if (row > static_cast<std::size_t>(
                      std::numeric_limits<timestamp>::max()) ||
            index[row] != static_cast<timestamp>(row)) {
            throw input_error{
                "style performance exposure index must be ordinal"};
        }
    }

    const std::string asset_column_name = copy_external_name(
        std::get<0>(string_columns.front()),
        "style performance exposure asset column must not be empty",
        "style performance exposure asset column must not contain an embedded NUL");
    if (asset_column_name != style_asset_column) {
        throw input_error{
            "style performance exposures must contain exactly one asset string column"};
    }
    const auto& asset_identities =
        style_exposures.get_column<std::string>(asset_column_name.c_str());
    if (asset_identities.size() != index.size()) {
        throw input_error{
            "style performance exposure asset identities must match the frame index length"};
    }

    std::unordered_map<std::string, std::size_t> exposure_rows;
    exposure_rows.reserve(asset_identities.size());
    for (std::size_t row{0U}; row < asset_identities.size(); ++row) {
        const std::string& identity = asset_identities[row];
        if (identity.empty()) {
            throw input_error{
                "style performance exposure asset identities must not be empty"};
        }
        if (identity.find('\0') != std::string::npos) {
            throw input_error{
                "style performance exposure asset identities must not contain an embedded NUL"};
        }
        if (!exposure_rows.emplace(identity, row).second) {
            throw input_error{
                "style performance exposure asset identities must be distinct"};
        }
    }

    if (double_columns.size() > std::vector<performance_style>{}.max_size()) {
        throw input_error{"style performance style count is not representable"};
    }
    if (!return_asset_names.empty() &&
        double_columns.size() >
            std::vector<double>{}.max_size() / return_asset_names.size()) {
        throw input_error{"style performance aligned weights are not representable"};
    }

    std::vector<performance_style> styles;
    styles.reserve(double_columns.size());
    std::unordered_set<std::string> output_names;
    output_names.reserve(double_columns.size());
    for (const auto& info : double_columns) {
        const std::string style_name = copy_external_name(
            std::get<0>(info),
            "style performance style names must not be empty",
            "style performance style names must not contain an embedded NUL");
        const std::string output_name = performance_output_name(style_name);
        if (!output_names.insert(output_name).second) {
            throw input_error{
                "style performance output column names must be distinct"};
        }

        const auto& raw_weights =
            style_exposures.get_column<double>(style_name.c_str());
        if (raw_weights.size() != index.size()) {
            throw input_error{
                "style performance style columns must match the frame index length"};
        }
        require_finite_values(raw_weights,
                              "style performance weights must be finite");

        std::vector<double> aligned_weights(return_asset_names.size(), 0.0);
        double total{0.0};
        for (std::size_t asset{0U}; asset < return_asset_names.size(); ++asset) {
            const auto found = exposure_rows.find(return_asset_names[asset]);
            if (found != exposure_rows.end()) {
                aligned_weights[asset] = raw_weights[found->second];
            }
            total += aligned_weights[asset];
            require_finite(total,
                           "style performance aligned weight total is not finite");
        }
        if (total > 0.0) {
            for (double& weight : aligned_weights) {
                weight /= total;
                require_finite(
                    weight,
                    "style performance normalized weight is not finite");
            }
        } else {
            std::fill(aligned_weights.begin(), aligned_weights.end(), 0.0);
        }
        styles.push_back({output_name, std::move(aligned_weights)});
    }
    return styles;
}

void validate_performance_output_shape(const std::size_t row_count,
                                       const std::size_t style_count) {
    if (row_count > std::vector<double>{}.max_size() ||
        style_count > std::vector<std::vector<double>>{}.max_size()) {
        throw input_error{"style performance output is not representable"};
    }
    if (row_count != 0U &&
        style_count > std::vector<double>{}.max_size() / row_count) {
        throw input_error{"style performance output is not representable"};
    }
}

dataframe analyze_performance_by_style_impl(
    const dataframe& asset_returns,
    const dataframe& style_exposures) {
    if (is_empty_shaped(asset_returns) || is_empty_shaped(style_exposures) ||
        style_exposures.get_columns_info<double>().empty()) {
        return {};
    }

    const auto returns = validate_performance_returns(asset_returns);
    auto styles = validate_and_align_performance_styles(
        style_exposures, returns.asset_names);
    validate_performance_output_shape(returns.row_count, styles.size());

    std::vector<std::vector<double>> style_returns(styles.size());
    for (std::size_t style{0U}; style < styles.size(); ++style) {
        auto& output = style_returns[style];
        output.reserve(returns.row_count);
        for (std::size_t row{0U}; row < returns.row_count; ++row) {
            double row_return{0.0};
            for (std::size_t asset{0U}; asset < returns.values.size(); ++asset) {
                const double product =
                    returns.values[asset][row] * styles[style].aligned_weights[asset];
                require_finite(product,
                               "style performance weighted product is not finite");
                row_return += product;
                require_finite(row_return,
                               "style performance row return is not finite");
            }
            output.push_back(row_return);
        }
    }

    dataframe result;
    result.load_index(ordinal_index(returns.row_count));
    for (std::size_t style{0U}; style < styles.size(); ++style) {
        result.load_column<double>(styles[style].output_name.c_str(),
                                   std::move(style_returns[style]),
                                   hmdf::nan_policy::dont_pad_with_nans);
    }
    return result;
}

struct tilt_inputs {
    std::vector<std::string> asset_names;
    std::vector<std::span<const double>> values;
};

constexpr std::array<std::string_view, 8U> tilt_suffixes{
    "_large", "_small", "_winner", "_loser",
    "_value", "_growth", "_high_vol", "_low_vol"};

tilt_inputs validate_tilt_inputs(const dataframe& asset_returns) {
    const auto& index = asset_returns.get_index();
    const auto shape = asset_returns.shape();
    const auto double_columns = asset_returns.get_columns_info<double>();
    if (shape.first != index.size() || double_columns.size() != shape.second) {
        throw input_error{"style tilts require only double return columns"};
    }
    if (index.size() > std::vector<timestamp>{}.max_size() ||
        double_columns.size() > std::vector<std::string>{}.max_size() ||
        double_columns.size() >
            std::vector<std::span<const double>>{}.max_size()) {
        throw input_error{"style tilts input is not representable"};
    }

    tilt_inputs result;
    result.asset_names.reserve(double_columns.size());
    result.values.reserve(double_columns.size());
    std::unordered_set<std::string> seen_names;
    seen_names.reserve(double_columns.size());
    for (const auto& info : double_columns) {
        std::string name = copy_external_name(
            std::get<0>(info),
            "style tilt asset names must not be empty",
            "style tilt asset names must not contain an embedded NUL");
        if (!seen_names.insert(name).second) {
            throw input_error{"style tilt asset names must be distinct"};
        }
        const auto& values = asset_returns.get_column<double>(name.c_str());
        if (values.size() != index.size()) {
            throw input_error{
                "style tilt return columns must match the frame index length"};
        }
        require_finite_values(values, "style tilt returns must be finite");
        result.asset_names.push_back(std::move(name));
        result.values.emplace_back(values.data(), values.size());
    }
    return result;
}

std::string tilt_output_name(const std::string& asset_name,
                             const std::string_view suffix) {
    const std::size_t capacity = dataframe::ColNameType::capacity();
    if (capacity == 0U || asset_name.size() >= capacity ||
        suffix.size() >= capacity - asset_name.size()) {
        throw input_error{"style tilt output column name is not representable"};
    }
    return asset_name + std::string{suffix};
}

std::vector<std::string> tilt_output_names(
    const std::vector<std::string>& asset_names) {
    if (asset_names.size() > std::numeric_limits<std::size_t>::max() /
                                 tilt_suffixes.size()) {
        throw input_error{"style tilt column count is not representable"};
    }
    const std::size_t column_count{asset_names.size() * tilt_suffixes.size()};
    if (column_count > std::vector<std::string>{}.max_size()) {
        throw input_error{"style tilt column count is not representable"};
    }

    std::vector<std::string> names;
    names.reserve(column_count);
    std::unordered_set<std::string> seen_names;
    seen_names.reserve(column_count);
    for (std::size_t phase{0U}; phase < tilt_suffixes.size(); phase += 2U) {
        for (const std::string& asset_name : asset_names) {
            for (std::size_t member{0U}; member < 2U; ++member) {
                std::string name =
                    tilt_output_name(asset_name, tilt_suffixes[phase + member]);
                if (!seen_names.insert(name).second) {
                    throw input_error{
                        "style tilt output column names must be distinct"};
                }
                names.push_back(std::move(name));
            }
        }
    }
    return names;
}

void validate_tilt_output_shape(const std::size_t row_count,
                                const std::size_t column_count) {
    if (row_count > std::vector<double>{}.max_size() ||
        column_count > std::vector<std::vector<double>>{}.max_size()) {
        throw input_error{"style tilt output is not representable"};
    }
    if (row_count != 0U &&
        column_count > std::vector<double>{}.max_size() / row_count) {
        throw input_error{"style tilt output is not representable"};
    }
}

std::vector<double> average_ranks(const std::span<const double> values) {
    std::vector<std::pair<double, std::size_t>> ordered;
    ordered.reserve(values.size());
    for (std::size_t position{0U}; position < values.size(); ++position) {
        ordered.emplace_back(values[position], position);
    }
    std::stable_sort(
        ordered.begin(), ordered.end(),
        [](const auto& left, const auto& right) {
            return left.first < right.first;
        });

    std::vector<double> ranks(values.size());
    std::size_t begin{0U};
    while (begin < ordered.size()) {
        std::size_t end{begin + 1U};
        while (end < ordered.size() &&
               ordered[end].first == ordered[begin].first) {
            ++end;
        }
        const double rank{
            (static_cast<double>(begin + 1U) + static_cast<double>(end)) /
            2.0};
        require_finite(rank, "style tilt rank is not finite");
        for (std::size_t position{begin}; position < end; ++position) {
            ranks[ordered[position].second] = rank;
        }
        begin = end;
    }
    return ranks;
}

double sample_standard_deviation(const std::span<const double> values,
                                 const std::size_t start,
                                 const std::size_t count) {
    if (count < 2U) {
        throw input_error{
            "style tilt volatility requires at least two observations"};
    }
    double mean{0.0};
    double sum_of_squares{0.0};
    for (std::size_t offset{0U}; offset < count; ++offset) {
        const double value{values[start + offset]};
        const double delta{value - mean};
        require_finite(delta,
                       "style tilt volatility intermediate is not finite");
        mean += delta / static_cast<double>(offset + 1U);
        require_finite(mean,
                       "style tilt volatility intermediate is not finite");
        const double adjusted_delta{value - mean};
        const double increment{delta * adjusted_delta};
        require_finite(increment,
                       "style tilt volatility intermediate is not finite");
        sum_of_squares += increment;
        require_finite(sum_of_squares,
                       "style tilt volatility intermediate is not finite");
    }
    const double variance{sum_of_squares / static_cast<double>(count - 1U)};
    require_finite(variance, "style tilt volatility is not finite");
    const double result{std::sqrt(variance)};
    require_finite(result, "style tilt volatility is not finite");
    return result;
}

std::size_t tilt_column_index(const std::size_t phase,
                              const std::size_t asset,
                              const std::size_t member,
                              const std::size_t asset_count) {
    return phase * asset_count * 2U + asset * 2U + member;
}

dataframe calculate_style_tilts_impl(const dataframe& asset_returns,
                                     const std::size_t window) {
    const auto shape = asset_returns.shape();
    const auto& index = asset_returns.get_index();
    if (shape.first == 0U || shape.second == 0U || index.empty()) {
        return {};
    }

    const std::size_t row_count{index.size()};
    if (row_count < 3U) {
        return {};
    }
    const std::size_t effective_window{std::min(window, row_count - 1U)};
    if (effective_window < 2U) {
        return {};
    }

    const tilt_inputs inputs = validate_tilt_inputs(asset_returns);
    const std::size_t asset_count{inputs.asset_names.size()};
    auto output_names = tilt_output_names(inputs.asset_names);
    const std::size_t output_rows{row_count - effective_window};
    validate_tilt_output_shape(output_rows, output_names.size());

    std::vector<std::vector<double>> output_columns(
        output_names.size(), std::vector<double>(output_rows));
    std::vector<double> latest(asset_count);
    std::vector<double> cumulative(asset_count);
    std::vector<double> volatilities(asset_count);
    const double asset_count_as_double{static_cast<double>(asset_count)};
    require_finite(asset_count_as_double,
                   "style tilt asset count is not representable");

    for (std::size_t output_row{0U}; output_row < output_rows; ++output_row) {
        const std::size_t input_row{effective_window + output_row};
        const std::size_t momentum_count{std::min(window, input_row)};
        const std::size_t momentum_start{input_row - momentum_count};
        const std::size_t volatility_count{
            std::min(maximum_volatility_window, input_row)};
        const std::size_t volatility_start{input_row - volatility_count};

        for (std::size_t asset{0U}; asset < asset_count; ++asset) {
            latest[asset] = inputs.values[asset][input_row - 1U];
            cumulative[asset] = compounded_return(
                inputs.values[asset], momentum_start, momentum_count);
            volatilities[asset] = sample_standard_deviation(
                inputs.values[asset], volatility_start, volatility_count);
        }

        const auto size_ranks = average_ranks(latest);
        const auto value_ranks = average_ranks(cumulative);
        const double volatility_median{checked_median(
            volatilities, "style tilt volatility median is not finite")};
        const double one_third{asset_count_as_double / 3.0};
        const double two_thirds{2.0 * asset_count_as_double / 3.0};
        const double one_half{asset_count_as_double / 2.0};

        for (std::size_t asset{0U}; asset < asset_count; ++asset) {
            output_columns[tilt_column_index(0U, asset, 0U, asset_count)]
                          [output_row] = size_ranks[asset] <= one_third ? 1.0 : 0.0;
            output_columns[tilt_column_index(0U, asset, 1U, asset_count)]
                          [output_row] = size_ranks[asset] > two_thirds ? 1.0 : 0.0;
            output_columns[tilt_column_index(1U, asset, 0U, asset_count)]
                          [output_row] = cumulative[asset] > 0.0 ? 1.0 : 0.0;
            output_columns[tilt_column_index(1U, asset, 1U, asset_count)]
                          [output_row] = cumulative[asset] <= 0.0 ? 1.0 : 0.0;
            output_columns[tilt_column_index(2U, asset, 0U, asset_count)]
                          [output_row] = value_ranks[asset] <= one_half ? 1.0 : 0.0;
            output_columns[tilt_column_index(2U, asset, 1U, asset_count)]
                          [output_row] = value_ranks[asset] > one_half ? 1.0 : 0.0;
            output_columns[tilt_column_index(3U, asset, 0U, asset_count)]
                          [output_row] =
                volatilities[asset] >= volatility_median ? 1.0 : 0.0;
            output_columns[tilt_column_index(3U, asset, 1U, asset_count)]
                          [output_row] =
                volatilities[asset] < volatility_median ? 1.0 : 0.0;
        }
    }

    if (output_rows > std::vector<timestamp>{}.max_size()) {
        throw input_error{"style tilt output index is not representable"};
    }
    dataframe result;
    result.load_index(std::vector<timestamp>{
        index.begin() + static_cast<std::ptrdiff_t>(effective_window),
        index.end()});
    for (std::size_t column{0U}; column < output_names.size(); ++column) {
        result.load_column<double>(output_names[column].c_str(),
                                   std::move(output_columns[column]),
                                   hmdf::nan_policy::dont_pad_with_nans);
    }
    return result;
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

dataframe analyze_performance_by_style(const dataframe& asset_returns) {
    try {
        if (is_empty_shaped(asset_returns)) {
            return {};
        }
        const style_analysis_result analysis = style_analysis(asset_returns);
        return analyze_performance_by_style(asset_returns, analysis.exposures);
    } catch (const input_error&) {
        throw;
    } catch (const std::bad_alloc&) {
        throw input_error{"style performance allocation failed"};
    } catch (const std::length_error&) {
        throw input_error{
            "style performance allocation is not representable"};
    } catch (const std::exception&) {
        throw input_error{"style performance input is malformed"};
    }
}

dataframe analyze_performance_by_style(const dataframe& asset_returns,
                                       const dataframe& style_exposures) {
    try {
        return analyze_performance_by_style_impl(asset_returns,
                                                 style_exposures);
    } catch (const input_error&) {
        throw;
    } catch (const std::bad_alloc&) {
        throw input_error{"style performance allocation failed"};
    } catch (const std::length_error&) {
        throw input_error{
            "style performance allocation is not representable"};
    } catch (const std::exception&) {
        throw input_error{"style performance input is malformed"};
    }
}

dataframe calculate_style_tilts(const dataframe& asset_returns,
                                const std::size_t window) {
    try {
        return calculate_style_tilts_impl(asset_returns, window);
    } catch (const input_error&) {
        throw;
    } catch (const std::bad_alloc&) {
        throw input_error{"style tilt allocation failed"};
    } catch (const std::length_error&) {
        throw input_error{"style tilt allocation is not representable"};
    } catch (const std::exception&) {
        throw input_error{"style tilt input is malformed"};
    }
}

}  // namespace fincore
