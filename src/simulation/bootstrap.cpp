#include <fincore/simulation/bootstrap.hpp>

#include <fincore/error.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fincore {
namespace {

constexpr double annualization_scale = 15.874507866387544;

std::string checked_column_name(const std::string_view column) {
    if (column.empty()) {
        throw input_error{"bootstrap column name must not be empty"};
    }
    if (column.find('\0') != std::string_view::npos) {
        throw input_error{"bootstrap column name must not contain an embedded NUL"};
    }
    return std::string{column};
}

void validate_statistic(const bootstrap_statistic statistic) {
    switch (statistic) {
    case bootstrap_statistic::mean:
    case bootstrap_statistic::sample_standard_deviation:
    case bootstrap_statistic::median:
    case bootstrap_statistic::sharpe_ratio:
    case bootstrap_statistic::sortino_ratio:
    case bootstrap_statistic::minimum:
    case bootstrap_statistic::maximum:
        return;
    }
    throw input_error{"unknown bootstrap statistic"};
}

std::vector<double> retained_values(const std::span<const double> values) {
    std::vector<double> retained;
    retained.reserve(values.size());
    for (const double value : values) {
        if (std::isnan(value)) {
            continue;
        }
        if (!std::isfinite(value)) {
            throw input_error{"bootstrap returns must be finite or NaN"};
        }
        retained.push_back(value);
    }
    if (retained.empty()) {
        throw input_error{"bootstrap requires at least one non-NaN return"};
    }
    return retained;
}

void validate_output_shape(const std::size_t resample_count) {
    if (resample_count >
        static_cast<std::size_t>(std::numeric_limits<timestamp>::max())) {
        throw input_error{"bootstrap output index is not representable"};
    }
    if (resample_count > std::vector<timestamp>{}.max_size() ||
        resample_count > std::vector<double>{}.max_size()) {
        throw input_error{"bootstrap output allocation is not representable"};
    }
}

void validate_confidence_alpha(const double alpha) {
    if (!std::isfinite(alpha) || alpha <= 0.0 || alpha >= 1.0) {
        throw input_error{"bootstrap confidence alpha must be finite and in (0, 1)"};
    }
}

std::vector<double> finite_bootstrap_values(const dataframe& distribution) {
    const auto values = require_double_column(distribution, bootstrap_statistic_column);
    if (values.empty()) {
        throw input_error{"bootstrap confidence interval requires at least one resample"};
    }

    std::vector<double> copied{values.begin(), values.end()};
    for (const double value : copied) {
        if (!std::isfinite(value)) {
            throw input_error{"bootstrap confidence interval requires finite statistics"};
        }
    }
    return copied;
}

double checked_linear_percentile(const std::vector<double>& ordered, const double percentile) {
    if (ordered.empty() || !std::isfinite(percentile) || percentile < 0.0 ||
        percentile > 1.0) {
        throw input_error{"bootstrap confidence percentile is invalid"};
    }

    const double last_position = static_cast<double>(ordered.size() - 1U);
    const double position = percentile * last_position;
    if (!std::isfinite(last_position) || !std::isfinite(position) || position < 0.0 ||
        position > last_position) {
        throw input_error{"bootstrap confidence percentile position overflowed"};
    }

    const double lower_position = std::floor(position);
    if (!std::isfinite(lower_position) || lower_position < 0.0 ||
        lower_position > static_cast<double>(std::numeric_limits<std::size_t>::max())) {
        throw input_error{"bootstrap confidence percentile index overflowed"};
    }
    const auto lower = static_cast<std::size_t>(lower_position);
    if (lower >= ordered.size()) {
        throw input_error{"bootstrap confidence percentile index is invalid"};
    }
    if (lower == ordered.size() - 1U) {
        return ordered[lower];
    }

    const double weight = position - lower_position;
    const double lower_component = ordered[lower] * (1.0 - weight);
    const double upper_component = ordered[lower + 1U] * weight;
    const double interpolated = lower_component + upper_component;
    if (!std::isfinite(weight) || !std::isfinite(lower_component) ||
        !std::isfinite(upper_component) || !std::isfinite(interpolated)) {
        throw input_error{"bootstrap confidence percentile interpolation overflowed"};
    }
    return interpolated;
}

std::vector<timestamp> ordinal_index(const std::size_t count) {
    std::vector<timestamp> index;
    index.reserve(count);
    for (std::size_t row = 0U; row < count; ++row) {
        index.push_back(static_cast<timestamp>(row));
    }
    return index;
}

std::mt19937_64 make_engine(const std::optional<std::uint64_t> seed) {
    if (seed.has_value()) {
        return std::mt19937_64{*seed};
    }
    std::random_device device;
    return std::mt19937_64{static_cast<std::uint64_t>(device())};
}

std::size_t random_index(std::mt19937_64& engine, const std::size_t bound) {
    if (bound == 0U || bound > std::numeric_limits<std::uint64_t>::max()) {
        throw input_error{"bootstrap sample size is not representable"};
    }
    const auto bound_as_word = static_cast<std::uint64_t>(bound);
    const std::uint64_t threshold = (std::uint64_t{0} - bound_as_word) % bound_as_word;
    std::uint64_t word = 0U;
    do {
        word = engine();
    } while (word < threshold);
    return static_cast<std::size_t>(word % bound_as_word);
}

double checked_mean(const std::vector<double>& values) {
    double mean = 0.0;
    for (std::size_t position = 0U; position < values.size(); ++position) {
        const double count = static_cast<double>(position + 1U);
        if (!std::isfinite(count)) {
            throw input_error{"bootstrap observation count is not finite"};
        }
        const double delta = values[position] - mean;
        if (!std::isfinite(delta)) {
            throw input_error{"bootstrap mean calculation overflowed"};
        }
        mean += delta / count;
        if (!std::isfinite(mean)) {
            throw input_error{"bootstrap mean calculation overflowed"};
        }
    }
    return mean;
}

double checked_sample_standard_deviation(const std::vector<double>& values) {
    if (values.size() < 2U) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    double mean = 0.0;
    double sum_of_squared_deviations = 0.0;
    for (std::size_t position = 0U; position < values.size(); ++position) {
        const double count = static_cast<double>(position + 1U);
        if (!std::isfinite(count)) {
            throw input_error{"bootstrap observation count is not finite"};
        }
        const double delta = values[position] - mean;
        if (!std::isfinite(delta)) {
            throw input_error{"bootstrap variance calculation overflowed"};
        }
        const double next_mean = mean + delta / count;
        const double next_delta = values[position] - next_mean;
        const double increment = delta * next_delta;
        const double next_sum = sum_of_squared_deviations + increment;
        if (!std::isfinite(next_mean) || !std::isfinite(next_delta) ||
            !std::isfinite(increment) || !std::isfinite(next_sum)) {
            throw input_error{"bootstrap variance calculation overflowed"};
        }
        mean = next_mean;
        sum_of_squared_deviations = next_sum;
    }

    const double denominator = static_cast<double>(values.size() - 1U);
    const double variance = sum_of_squared_deviations / denominator;
    if (!std::isfinite(variance) || variance < 0.0) {
        throw input_error{"bootstrap variance is not finite"};
    }
    const double standard_deviation = std::sqrt(variance);
    if (!std::isfinite(standard_deviation)) {
        throw input_error{"bootstrap standard deviation is not finite"};
    }
    return standard_deviation;
}

double checked_ratio(const std::vector<double>& numerator_values,
                     const std::vector<double>& deviation_values) {
    if (numerator_values.size() < 2U) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const double mean = checked_mean(numerator_values);
    const double standard_deviation = checked_sample_standard_deviation(deviation_values);
    if (standard_deviation == 0.0) {
        return mean == 0.0 ? std::numeric_limits<double>::quiet_NaN()
                           : std::numeric_limits<double>::infinity();
    }
    const double ratio = mean / standard_deviation;
    const double annualized_ratio = ratio * annualization_scale;
    if (!std::isfinite(ratio) || !std::isfinite(annualized_ratio)) {
        throw input_error{"bootstrap ratio calculation overflowed"};
    }
    return annualized_ratio;
}

double statistic_value(const std::vector<double>& sample,
                       const bootstrap_statistic statistic) {
    switch (statistic) {
    case bootstrap_statistic::mean:
        return checked_mean(sample);
    case bootstrap_statistic::sample_standard_deviation:
        return checked_sample_standard_deviation(sample);
    case bootstrap_statistic::median: {
        auto ordered = sample;
        std::sort(ordered.begin(), ordered.end());
        const std::size_t middle = ordered.size() / 2U;
        if (ordered.size() % 2U != 0U) {
            return ordered[middle];
        }
        return ordered[middle - 1U] / 2.0 + ordered[middle] / 2.0;
    }
    case bootstrap_statistic::sharpe_ratio:
        return checked_ratio(sample, sample);
    case bootstrap_statistic::sortino_ratio: {
        auto downside = sample;
        for (double& value : downside) {
            if (value > 0.0) {
                value = 0.0;
            }
        }
        return checked_ratio(sample, downside);
    }
    case bootstrap_statistic::minimum:
        return *std::min_element(sample.begin(), sample.end());
    case bootstrap_statistic::maximum:
        return *std::max_element(sample.begin(), sample.end());
    }
    throw input_error{"unknown bootstrap statistic"};
}

}  // namespace

dataframe bootstrap_distribution(const dataframe& returns,
                                 const std::size_t resample_count,
                                 const bootstrap_statistic statistic,
                                 const std::optional<std::uint64_t> seed,
                                 const std::string_view column) {
    try {
        const auto column_name = checked_column_name(column);
        validate_statistic(statistic);
        const auto retained = retained_values(require_double_column(returns, column_name));
        validate_output_shape(resample_count);

        std::vector<double> results;
        results.reserve(resample_count);
        std::vector<double> sample(retained.size());
        auto engine = make_engine(seed);
        for (std::size_t row = 0U; row < resample_count; ++row) {
            for (double& value : sample) {
                value = retained[random_index(engine, retained.size())];
            }
            results.push_back(statistic_value(sample, statistic));
        }

        dataframe result;
        result.load_index(ordinal_index(resample_count));
        result.load_column<double>(bootstrap_statistic_column.data(), std::move(results),
                                   hmdf::nan_policy::dont_pad_with_nans);
        return result;
    } catch (const std::bad_alloc&) {
        throw input_error{"bootstrap allocation failed"};
    } catch (const std::length_error&) {
        throw input_error{"bootstrap allocation is not representable"};
    }
}

bootstrap_confidence_interval_result bootstrap_confidence_interval(
    const dataframe& returns,
    const std::size_t resample_count,
    const double alpha,
    const bootstrap_statistic statistic,
    const std::optional<std::uint64_t> seed,
    const std::string_view column) {
    validate_confidence_alpha(alpha);
    try {
        const auto distribution = bootstrap_distribution(returns, resample_count, statistic,
                                                         seed, column);
        auto values = finite_bootstrap_values(distribution);
        std::sort(values.begin(), values.end());
        return {checked_linear_percentile(values, alpha / 2.0),
                checked_linear_percentile(values, 1.0 - alpha / 2.0)};
    } catch (const std::bad_alloc&) {
        throw input_error{"bootstrap confidence interval allocation failed"};
    } catch (const std::length_error&) {
        throw input_error{"bootstrap confidence interval allocation is not representable"};
    }
}

}  // namespace fincore
