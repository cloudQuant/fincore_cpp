#include <fincore/metrics/stats.hpp>

#include <fincore/detail/numeric.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <span>
#include <vector>

namespace fincore {
namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();
constexpr double minimum_standard_deviation = 1.0e-15;

struct aligned_finite_pairs {
  std::vector<timestamp> index;
  std::vector<double> returns;
  std::vector<double> benchmark;
};

struct central_moments {
  double second;
  double third;
  double fourth;
};

aligned_finite_pairs align_finite_pairs(
    const dataframe& returns,
    const dataframe& benchmark,
    std::string_view returns_column_name,
    std::string_view benchmark_column_name) {
  auto [aligned_returns, aligned_benchmark] = align_on_common_index(
      returns, returns_column_name, benchmark, benchmark_column_name);
  const auto return_values =
      require_double_column(aligned_returns, returns_column_name);
  const auto benchmark_values =
      require_double_column(aligned_benchmark, benchmark_column_name);
  const auto& index = aligned_returns.get_index();

  aligned_finite_pairs pairs;
  pairs.index.reserve(index.size());
  pairs.returns.reserve(return_values.size());
  pairs.benchmark.reserve(benchmark_values.size());
  for (std::size_t position = 0; position < return_values.size(); ++position) {
    if (std::isfinite(return_values[position]) &&
        std::isfinite(benchmark_values[position])) {
      pairs.index.push_back(index[position]);
      pairs.returns.push_back(return_values[position]);
      pairs.benchmark.push_back(benchmark_values[position]);
    }
  }
  return pairs;
}

central_moments calculate_central_moments(std::span<const double> values) {
  const double average = detail::mean(values);
  double second_total = 0.0;
  double third_total = 0.0;
  double fourth_total = 0.0;
  for (const double value : values) {
    const double deviation = value - average;
    const double squared = deviation * deviation;
    second_total += squared;
    third_total += squared * deviation;
    fourth_total += squared * squared;
  }
  const double count = static_cast<double>(values.size());
  return {second_total / count, third_total / count, fourth_total / count};
}

double safe_pearson_correlation(std::span<const double> first,
                                std::span<const double> second) {
  if (first.size() != second.size() || first.size() < 2) {
    return nan;
  }

  const double first_mean = detail::mean(first);
  const double second_mean = detail::mean(second);
  if (!std::isfinite(first_mean) || !std::isfinite(second_mean)) {
    return nan;
  }

  double first_square_total = 0.0;
  double second_square_total = 0.0;
  double cross_total = 0.0;
  for (std::size_t position = 0; position < first.size(); ++position) {
    const double first_deviation = first[position] - first_mean;
    const double second_deviation = second[position] - second_mean;
    first_square_total += first_deviation * first_deviation;
    second_square_total += second_deviation * second_deviation;
    cross_total += first_deviation * second_deviation;
  }
  const double count = static_cast<double>(first.size());
  const double first_variance = first_square_total / count;
  const double second_variance = second_square_total / count;
  if (!(first_variance >= 0.0) || !(second_variance >= 0.0) ||
      !std::isfinite(first_variance) || !std::isfinite(second_variance)) {
    return nan;
  }

  const double first_standard_deviation = std::sqrt(first_variance);
  const double second_standard_deviation = std::sqrt(second_variance);
  if (first_standard_deviation < minimum_standard_deviation ||
      second_standard_deviation < minimum_standard_deviation ||
      !std::isfinite(first_standard_deviation) ||
      !std::isfinite(second_standard_deviation)) {
    return nan;
  }

  const double covariance = cross_total / count;
  const double correlation =
      covariance / (first_standard_deviation * second_standard_deviation);
  return std::isfinite(correlation) ? correlation : nan;
}

}  // namespace

double skewness(const dataframe& returns, std::string_view column) {
  const auto values = detail::finite_values(require_double_column(returns, column));
  if (values.size() < 3) {
    return nan;
  }

  const auto moments = calculate_central_moments(values);
  if (!(moments.second > 0.0) || !std::isfinite(moments.second) ||
      !std::isfinite(moments.third)) {
    return nan;
  }
  const double denominator = moments.second * std::sqrt(moments.second);
  if (!(denominator > 0.0) || !std::isfinite(denominator)) {
    return nan;
  }
  const double result = moments.third / denominator;
  return std::isfinite(result) ? result : nan;
}

double kurtosis(const dataframe& returns, std::string_view column) {
  const auto values = detail::finite_values(require_double_column(returns, column));
  if (values.size() < 4) {
    return nan;
  }

  const auto moments = calculate_central_moments(values);
  if (!(moments.second > 0.0) || !std::isfinite(moments.second) ||
      !std::isfinite(moments.fourth)) {
    return nan;
  }
  const double denominator = moments.second * moments.second;
  if (!(denominator > 0.0) || !std::isfinite(denominator)) {
    return nan;
  }
  const double result = moments.fourth / denominator - 3.0;
  return std::isfinite(result) ? result : nan;
}

double serial_correlation(const dataframe& returns,
                          std::ptrdiff_t lag,
                          std::string_view column) {
  const auto values = detail::finite_values(require_double_column(returns, column));
  if (lag < 1) {
    return nan;
  }

  const auto lag_count = static_cast<std::size_t>(lag);
  if (values.size() <= lag_count || values.size() - lag_count < 2) {
    return nan;
  }
  const auto pair_count = values.size() - lag_count;
  return safe_pearson_correlation(
      std::span<const double>{values.data() + lag_count, pair_count},
      std::span<const double>{values.data(), pair_count});
}

double win_rate(const dataframe& returns, std::string_view column) {
  const auto values = detail::finite_values(require_double_column(returns, column));
  if (values.empty()) {
    return nan;
  }
  std::size_t wins = 0;
  for (const double value : values) {
    if (value > 0.0) {
      ++wins;
    }
  }
  return static_cast<double>(wins) / static_cast<double>(values.size());
}

double loss_rate(const dataframe& returns, std::string_view column) {
  const auto values = detail::finite_values(require_double_column(returns, column));
  if (values.empty()) {
    return nan;
  }
  std::size_t losses = 0;
  for (const double value : values) {
    if (value < 0.0) {
      ++losses;
    }
  }
  return static_cast<double>(losses) / static_cast<double>(values.size());
}

double safe_correlation(const dataframe& returns,
                        const dataframe& benchmark,
                        std::string_view returns_column_name,
                        std::string_view benchmark_column_name) {
  const auto pairs = align_finite_pairs(returns, benchmark, returns_column_name,
                                        benchmark_column_name);
  return safe_pearson_correlation(pairs.returns, pairs.benchmark);
}

double relative_win_rate(const dataframe& returns,
                         const dataframe& benchmark,
                         std::string_view returns_column_name,
                         std::string_view benchmark_column_name) {
  const auto pairs = align_finite_pairs(returns, benchmark, returns_column_name,
                                        benchmark_column_name);
  if (pairs.returns.empty()) {
    return nan;
  }
  std::size_t wins = 0;
  for (std::size_t position = 0; position < pairs.returns.size(); ++position) {
    if (pairs.returns[position] > pairs.benchmark[position]) {
      ++wins;
    }
  }
  return static_cast<double>(wins) / static_cast<double>(pairs.returns.size());
}

double capm_r_squared(const dataframe& returns,
                      const dataframe& benchmark,
                      std::string_view returns_column_name,
                      std::string_view benchmark_column_name) {
  const auto pairs = align_finite_pairs(returns, benchmark, returns_column_name,
                                        benchmark_column_name);
  const double correlation = safe_pearson_correlation(pairs.returns, pairs.benchmark);
  if (!std::isfinite(correlation)) {
    return nan;
  }
  const double result = correlation * correlation;
  return std::isfinite(result) ? std::clamp(result, 0.0, 1.0) : nan;
}

double tracking_difference(const dataframe& returns,
                           const dataframe& benchmark,
                           std::string_view returns_column_name,
                           std::string_view benchmark_column_name) {
  const auto pairs = align_finite_pairs(returns, benchmark, returns_column_name,
                                        benchmark_column_name);
  if (pairs.index.empty()) {
    return nan;
  }
  const auto paired_returns =
      make_series(pairs.index, returns_column_name, pairs.returns);
  const auto paired_benchmark =
      make_series(pairs.index, benchmark_column_name, pairs.benchmark);
  const double strategy_return =
      cumulative_return(paired_returns, returns_column_name);
  const double benchmark_return =
      cumulative_return(paired_benchmark, benchmark_column_name);
  const double result = strategy_return - benchmark_return;
  return std::isfinite(result) ? result : nan;
}

}  // namespace fincore
