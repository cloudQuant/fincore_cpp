#include <fincore/calendar.hpp>
#include <fincore/metrics/drawdown.hpp>
#include <fincore/metrics/stats.hpp>

#include <fincore/detail/numeric.hpp>
#include <fincore/detail/stutzer.hpp>

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
  // One pass yields all three moments, each divided by n: the population form the
  // skew and excess-kurtosis ratios below are built from, whereas the
  // sample-adjusted estimators live with the ratio helpers.
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
  // A standard deviation under the floor is treated as no dispersion rather than
  // as a tiny one: dividing by it would report a correlation near +/-1 for two
  // series that only differ by rounding noise, so the honest answer is NaN.
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

double r_cubed(const dataframe& returns,
               const dataframe& benchmark,
               std::string_view returns_column_name,
               std::string_view benchmark_column_name) {
  if (returns_column_name.find('\0') != std::string_view::npos) {
    throw input_error{"returns column name must not contain an embedded NUL"};
  }
  if (benchmark_column_name.find('\0') != std::string_view::npos) {
    throw input_error{
        "benchmark column name must not contain an embedded NUL"};
  }
  const double correlation = safe_correlation(
      returns, benchmark, returns_column_name, benchmark_column_name);
  if (std::isnan(correlation)) {
    return nan;
  }
  return correlation * correlation * correlation;
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

// Above thirty periods the lags are spaced logarithmically at thirty points with
// the last pinned to the requested maximum; rounding can fold two positions onto
// the same integer, which is why the list is sorted and deduplicated and can come
// back shorter than thirty entries.
std::vector<std::size_t> hurst_lags(std::size_t max_lag) {
    std::vector<std::size_t> lags;
    const std::size_t span = max_lag - 1;
    if (span > 30) {
        const double start_log = std::log10(2.0);
        const double stop_log = std::log10(static_cast<double>(max_lag));
        const double step = (stop_log - start_log) / 29.0;
        for (std::size_t position = 0; position < 30; ++position) {
            const double value =
                std::pow(10.0, start_log + step * static_cast<double>(position));
            lags.push_back(static_cast<std::size_t>(value));
        }
        lags.back() = max_lag;
        std::sort(lags.begin(), lags.end());
        lags.erase(std::unique(lags.begin(), lags.end()), lags.end());
    } else {
        for (std::size_t lag = 2; lag <= max_lag; ++lag) {
            lags.push_back(lag);
        }
    }
    return lags;
}

double hurst_exponent(const dataframe& returns, std::string_view column) {
    constexpr std::size_t minimum_length = 8;
    if (column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    const auto raw_values = require_double_column(returns, column);
    if (raw_values.size() < minimum_length) {
        return nan;
    }

    std::vector<double> values;
    values.reserve(raw_values.size());
    for (const double value : raw_values) {
        if (!std::isnan(value)) {
            values.push_back(value);
        }
    }
    if (values.size() < minimum_length) {
        return nan;
    }

    const double count = static_cast<double>(values.size());
    double average = 0.0;
    for (const double value : values) {
        average += value;
    }
    average /= count;

    double running = 0.0;
    double maximum = -std::numeric_limits<double>::infinity();
    double minimum = std::numeric_limits<double>::infinity();
    for (const double value : values) {
        running += value - average;
        maximum = std::max(maximum, running);
        minimum = std::min(minimum, running);
    }
    const double range = maximum - minimum;

    double squared_total = 0.0;
    for (const double value : values) {
        const double deviation = value - average;
        squared_total += deviation * deviation;
    }
    const double deviation = std::sqrt(squared_total / (count - 1.0));
    if (deviation == 0.0 || range == 0.0) {
        return nan;
    }

    const std::size_t size = values.size();
    const std::size_t max_lag = std::max<std::size_t>(size / 3, 3);
    struct lag_ratio {
        double lag;
        double ratio;
    };
    std::vector<lag_ratio> ratios;
    for (const std::size_t lag : hurst_lags(max_lag)) {
        const std::size_t subseries_count = size / lag;
        if (subseries_count < 1) {
            continue;
        }
        double ratio_total = 0.0;
        std::size_t valid_count = 0;
        for (std::size_t row = 0; row < subseries_count; ++row) {
            const std::size_t offset = row * lag;
            double block_mean = 0.0;
            for (std::size_t index = 0; index < lag; ++index) {
                block_mean += values[offset + index];
            }
            block_mean /= static_cast<double>(lag);

            double cumulative = 0.0;
            double block_max = -std::numeric_limits<double>::infinity();
            double block_min = std::numeric_limits<double>::infinity();
            double block_squared = 0.0;
            for (std::size_t index = 0; index < lag; ++index) {
                const double block_deviation = values[offset + index] - block_mean;
                cumulative += block_deviation;
                block_max = std::max(block_max, cumulative);
                block_min = std::min(block_min, cumulative);
                block_squared += block_deviation * block_deviation;
            }
            const double block_range = block_max - block_min;
            const double block_deviation =
                std::sqrt(block_squared / static_cast<double>(lag - 1));
            if (block_deviation > 0.0 && block_range > 0.0) {
                ratio_total += block_range / block_deviation;
                ++valid_count;
            }
        }
        if (valid_count > 0) {
            ratios.push_back({static_cast<double>(lag),
                              ratio_total / static_cast<double>(valid_count)});
        }
    }

    if (ratios.size() < 2) {
        double fallback =
            0.5 + std::log(range / deviation) / std::log(2.0 * count);
        fallback = std::max(0.0, std::min(1.0, fallback));
        return fallback;
    }

    double lag_mean = 0.0;
    double ratio_mean = 0.0;
    for (const auto& entry : ratios) {
        lag_mean += std::log(entry.lag);
        ratio_mean += std::log(entry.ratio);
    }
    lag_mean /= static_cast<double>(ratios.size());
    ratio_mean /= static_cast<double>(ratios.size());

    double covariance = 0.0;
    double lag_variance = 0.0;
    for (const auto& entry : ratios) {
        const double lag_delta = std::log(entry.lag) - lag_mean;
        const double ratio_delta = std::log(entry.ratio) - ratio_mean;
        covariance += lag_delta * ratio_delta;
        lag_variance += lag_delta * lag_delta;
    }
    if (lag_variance == 0.0) {
        return nan;
    }
    double hurst = covariance / lag_variance;
    hurst = std::max(0.0, std::min(1.0, hurst));
    return hurst;
}

double stutzer_index(const dataframe& returns,
                     double target_return,
                     std::string_view column) {
    if (column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    const auto raw_values = require_double_column(returns, column);
    if (raw_values.size() < 2) {
        return nan;
    }

    std::vector<double> excess_returns;
    excess_returns.reserve(raw_values.size());
    for (const double value : raw_values) {
        const double excess = value - target_return;
        if (!std::isnan(excess)) {
            excess_returns.push_back(excess);
        }
    }
    if (excess_returns.size() < 2) {
        return nan;
    }

    double total = 0.0;
    for (const double value : excess_returns) {
        total += value;
    }
    const double mean_excess =
        total / static_cast<double>(excess_returns.size());
    if (std::fabs(mean_excess) < 1e-15) {
        return 0.0;
    }

    double lower = mean_excess > 0.0 ? -50.0 : 1e-10;
    double upper = mean_excess > 0.0 ? -1e-10 : 50.0;
    constexpr double inverse_golden = 0.6180339887498949;
    double left = upper - inverse_golden * (upper - lower);
    double right = lower + inverse_golden * (upper - lower);
    double left_value = detail::stutzer_objective(left, excess_returns);
    double right_value = detail::stutzer_objective(right, excess_returns);
    for (int iteration = 0; iteration < 200; ++iteration) {
        if (upper - lower <= 1e-12) {
            break;
        }
        if (left_value < right_value) {
            upper = right;
            right = left;
            right_value = left_value;
            left = upper - inverse_golden * (upper - lower);
            left_value = detail::stutzer_objective(left, excess_returns);
        } else {
            lower = left;
            left = right;
            left_value = right_value;
            right = lower + inverse_golden * (upper - lower);
            right_value = detail::stutzer_objective(right, excess_returns);
        }
    }

    double index_of_precision =
        -detail::stutzer_objective((lower + upper) / 2.0, excess_returns);
    if (index_of_precision < 0.0) {
        index_of_precision = 0.0;
    }
    const double sign = mean_excess > 0.0 ? 1.0 : -1.0;
    return sign * std::sqrt(2.0 * index_of_precision);
}

double r_cubed_turtle(const dataframe& returns,
                      double annualization,
                      std::string_view column) {
    if (column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    const auto values = require_double_column(returns, column);
    if (values.size() < 2) {
        return nan;
    }

    std::vector<double> nav;
    nav.reserve(values.size());
    double wealth = 1.0;
    for (const double value : values) {
        if (std::isnan(value)) {
            nav.push_back(nan);
        } else {
            wealth *= 1.0 + value;
            nav.push_back(wealth);
        }
    }

    double time_total = 0.0;
    double nav_total = 0.0;
    std::size_t count = 0;
    for (std::size_t position = 0; position < nav.size(); ++position) {
        if (!std::isnan(nav[position])) {
            time_total += static_cast<double>(position);
            nav_total += nav[position];
            ++count;
        }
    }
    if (count < 2) {
        return nan;
    }
    const double time_mean = time_total / static_cast<double>(count);
    const double nav_mean = nav_total / static_cast<double>(count);
    double covariance = 0.0;
    double time_variance = 0.0;
    for (std::size_t position = 0; position < nav.size(); ++position) {
        if (std::isnan(nav[position])) {
            continue;
        }
        const double time_delta =
            static_cast<double>(position) - time_mean;
        covariance += time_delta * (nav[position] - nav_mean);
        time_variance += time_delta * time_delta;
    }
    if (time_variance == 0.0) {
        return nan;
    }
    const double rar = covariance / time_variance * annualization;

    const auto& index = returns.get_index();
    std::vector<int> years;
    for (const timestamp point : index) {
        const int year = utc_year(point);
        if (std::find(years.begin(), years.end(), year) == years.end()) {
            years.push_back(year);
        }
    }
    if (years.empty()) {
        return nan;
    }

    std::vector<double> maximum_drawdowns;
    maximum_drawdowns.reserve(years.size());
    for (const int year : years) {
        std::vector<timestamp> year_index;
        std::vector<double> year_values;
        for (std::size_t position = 0; position < index.size(); ++position) {
            if (utc_year(index[position]) == year) {
                year_index.push_back(index[position]);
                year_values.push_back(values[position]);
            }
        }
        if (year_values.empty()) {
            continue;
        }
        const auto year_frame =
            make_series(std::move(year_index), column, std::move(year_values));
        maximum_drawdowns.push_back(
            std::fabs(max_drawdown(year_frame, column)));
    }
    if (maximum_drawdowns.empty()) {
        return nan;
    }
    double total = 0.0;
    for (const double drawdown : maximum_drawdowns) {
        total += drawdown;
    }
    const double average =
        total / static_cast<double>(maximum_drawdowns.size());
    if (average < 1e-15) {
        return rar > 0.0 ? std::numeric_limits<double>::infinity() : nan;
    }
    return rar / average;
}

}  // namespace fincore
