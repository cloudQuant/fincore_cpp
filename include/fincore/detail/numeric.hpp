#pragma once

#include <algorithm>
#include <array>
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
  if (values.empty() || !std::isfinite(cutoff) || cutoff < 0.0 ||
      cutoff > 1.0) {
    return nan;
  }

  std::vector<double> sorted_values{values.begin(), values.end()};
  std::sort(sorted_values.begin(), sorted_values.end());
  const double position =
      cutoff * static_cast<double>(sorted_values.size() - 1);
  const auto lower = static_cast<std::size_t>(std::floor(position));
  const auto upper = static_cast<std::size_t>(std::ceil(position));
  return sorted_values[lower] +
         (position - static_cast<double>(lower)) *
             (sorted_values[upper] - sorted_values[lower]);
}

inline double inverse_standard_normal_cdf(double confidence) {
  if (!std::isfinite(confidence) || confidence < 0.0 || confidence > 1.0) {
    return nan;
  }
  if (confidence == 0.0) {
    return -std::numeric_limits<double>::infinity();
  }
  if (confidence == 1.0) {
    return std::numeric_limits<double>::infinity();
  }

  constexpr std::array<double, 8> central_numerator{
      3.3871328727963666080, 133.14166789178437745, 1971.5909503065514427,
      13731.693765509461125, 45921.953931549871457, 67265.770927008700853,
      33430.575583588128105, 2509.0809287301226727};
  constexpr std::array<double, 8> central_denominator{1.0,
                                                      42.313330701600911252,
                                                      687.18700749205790830,
                                                      5394.1960214247511077,
                                                      21213.794301586595867,
                                                      39307.895800092710610,
                                                      28729.085735721942674,
                                                      5226.4952788528545610};
  constexpr std::array<double, 8> intermediate_numerator{
      1.42343711074968357734,   4.63033784615654529590,
      5.76949722146069140550,   3.64784832476320460504,
      1.27045825245236838258,   0.241780725177450611770,
      0.0227238449892691845833, 0.00077454501427834140764};
  constexpr std::array<double, 8> intermediate_denominator{
      1.0,
      2.05319162663775882187,
      1.67638483018380384940,
      0.68976733498510000455,
      0.14810397642748007459,
      0.0151986665636164571966,
      0.0005475938084995344946,
      0.00000000105075007164441684324};
  constexpr std::array<double, 8> tail_numerator{
      6.65790464350110377720,      5.46378491116411436990,
      1.78482653991729133580,      0.29656057182850489123,
      0.026532189526576123093,     0.00124266094738807843860,
      0.0000271155556874348757815, 0.000000201033439929228813265};
  constexpr std::array<double, 8> tail_denominator{
      1.0,
      0.599832206555887937690,
      0.136929880922735805310,
      0.0148753612908506148525,
      0.000786869131145613259100,
      0.0000184631831751005468180,
      0.000000142151175831644588870,
      0.00000000000000204426310338993978564};

  const auto evaluate = [](const std::array<double, 8> &coefficients,
                           double value) {
    double result = coefficients.back();
    for (std::size_t index = coefficients.size() - 1; index > 0; --index) {
      result = result * value + coefficients[index - 1];
    }
    return result;
  };

  const double centered_confidence = confidence - 0.5;
  if (std::abs(centered_confidence) <= 0.425) {
    const double squared = 0.180625 - centered_confidence * centered_confidence;
    return centered_confidence * evaluate(central_numerator, squared) /
           evaluate(central_denominator, squared);
  }

  double tail_confidence =
      centered_confidence < 0.0 ? confidence : 1.0 - confidence;
  tail_confidence = std::sqrt(-std::log(tail_confidence));
  const bool intermediate = tail_confidence <= 5.0;
  const double shifted = tail_confidence - (intermediate ? 1.6 : 5.0);
  const double result = intermediate
                            ? evaluate(intermediate_numerator, shifted) /
                                  evaluate(intermediate_denominator, shifted)
                            : evaluate(tail_numerator, shifted) /
                                  evaluate(tail_denominator, shifted);
  return centered_confidence < 0.0 ? -result : result;
}

} // namespace fincore::detail
