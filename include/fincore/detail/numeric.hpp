#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <vector>

namespace fincore::detail {

inline constexpr double nan = std::numeric_limits<double>::quiet_NaN();

inline std::vector<double> finite_values(std::span<const double> values) {
    std::vector<double> result;
    result.reserve(values.size());
    for (const double value : values) {
        if (std::isfinite(value)) {
            result.push_back(value);
        }
    }
    return result;
}

inline double mean(std::span<const double> values) {
    if (values.empty()) {
        return nan;
    }

    double total = 0.0;
    for (const double value : values) {
        total += value;
    }
    return total / static_cast<double>(values.size());
}

inline double population_variance(std::span<const double> values) {
    if (values.empty()) {
        return nan;
    }

    const double average = mean(values);
    double squared_deviations = 0.0;
    for (const double value : values) {
        const double deviation = value - average;
        squared_deviations += deviation * deviation;
    }
    return squared_deviations / static_cast<double>(values.size());
}

inline double sample_variance(std::span<const double> values) {
    if (values.size() < 2) {
        return nan;
    }

    const double average = mean(values);
    double squared_deviations = 0.0;
    for (const double value : values) {
        const double deviation = value - average;
        squared_deviations += deviation * deviation;
    }
    return squared_deviations / static_cast<double>(values.size() - 1);
}

struct finite_pairs {
    std::vector<double> first;
    std::vector<double> second;
};

inline finite_pairs filter_finite_pairs(std::span<const double> first,
                                        std::span<const double> second) {
    finite_pairs result;
    if (first.size() != second.size()) {
        return result;
    }

    result.first.reserve(first.size());
    result.second.reserve(second.size());
    for (std::size_t position = 0; position < first.size(); ++position) {
        if (std::isfinite(first[position]) && std::isfinite(second[position])) {
            result.first.push_back(first[position]);
            result.second.push_back(second[position]);
        }
    }
    return result;
}

inline double linear_percentile(std::span<const double> values, double cutoff) {
    if (values.empty() || !std::isfinite(cutoff) || cutoff < 0.0 || cutoff > 1.0) {
        return nan;
    }

    std::vector<double> sorted_values{values.begin(), values.end()};
    std::sort(sorted_values.begin(), sorted_values.end());
    const double position = cutoff * static_cast<double>(sorted_values.size() - 1);
    const auto lower = static_cast<std::size_t>(std::floor(position));
    const auto upper = static_cast<std::size_t>(std::ceil(position));
    return sorted_values[lower] + (position - static_cast<double>(lower)) *
                                      (sorted_values[upper] - sorted_values[lower]);
}

}  // namespace fincore::detail
