#include <fincore/metrics/ratios.hpp>

#include <fincore/detail/numeric.hpp>
#include <fincore/metrics/risk.hpp>
#include <fincore/metrics/stats.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <span>
#include <vector>

namespace fincore {
namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();
constexpr double minimum_tracking_error = 1.0e-15;

struct finite_aligned_pairs {
    std::vector<timestamp> index;
    std::vector<double> returns;
    std::vector<double> benchmark;
};

std::vector<double> valid_values(std::span<const double> values) {
    std::vector<double> result;
    result.reserve(values.size());
    for (const double value : values) {
        if (!std::isnan(value)) {
            result.push_back(value);
        }
    }
    return result;
}

double mean(const std::vector<double>& values) {
    double total = 0.0;
    for (const double value : values) {
        total += value;
    }
    return total / static_cast<double>(values.size());
}

struct biased_moments {
    double skew;
    double kurtosis;
};

biased_moments nan_only_biased_moments(std::span<const double> raw_values) {
    const auto values = valid_values(raw_values);
    const double count = static_cast<double>(values.size());
    const double average = mean(values);

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

    const double second = second_total / count;
    const double third = third_total / count;
    const double fourth = fourth_total / count;
    const double epsilon_mean =
        std::numeric_limits<double>::epsilon() * average;
    if (second <= epsilon_mean * epsilon_mean) {
        return {nan, nan};
    }

    return {third / (second * std::sqrt(second)),
            fourth / (second * second) - 3.0};
}

double finite_quotient(double numerator, double denominator) {
    if (denominator == 0.0) {
        return nan;
    }
    const double result = numerator / denominator;
    return std::isfinite(result) ? result : nan;
}

finite_aligned_pairs align_finite_pairs(
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

    finite_aligned_pairs pairs;
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

std::vector<double> active_returns(const finite_aligned_pairs& pairs) {
    std::vector<double> active;
    active.reserve(pairs.returns.size());
    for (std::size_t position = 0; position < pairs.returns.size(); ++position) {
        active.push_back(pairs.returns[position] - pairs.benchmark[position]);
    }
    return active;
}

double sample_standard_deviation(std::span<const double> values) {
    if (values.size() < 2) {
        return nan;
    }
    const double variance = detail::sample_variance(values);
    if (!(variance >= 0.0) || !std::isfinite(variance)) {
        return nan;
    }
    const double result = std::sqrt(variance);
    return std::isfinite(result) ? result : nan;
}

finite_aligned_pairs select_benchmark_sign(const finite_aligned_pairs& pairs,
                                            bool positive) {
    finite_aligned_pairs selected;
    selected.index.reserve(pairs.index.size());
    selected.returns.reserve(pairs.returns.size());
    selected.benchmark.reserve(pairs.benchmark.size());
    for (std::size_t position = 0; position < pairs.index.size(); ++position) {
        const bool include = positive ? pairs.benchmark[position] > 0.0
                                      : pairs.benchmark[position] < 0.0;
        if (include) {
            selected.index.push_back(pairs.index[position]);
            selected.returns.push_back(pairs.returns[position]);
            selected.benchmark.push_back(pairs.benchmark[position]);
        }
    }
    return selected;
}

double capture_from_pairs(const finite_aligned_pairs& pairs,
                          double annualization,
                          std::string_view returns_column_name,
                          std::string_view benchmark_column_name) {
    if (pairs.index.empty()) {
        return nan;
    }

    const auto paired_returns =
        make_series(pairs.index, returns_column_name, pairs.returns);
    const auto paired_benchmark =
        make_series(pairs.index, benchmark_column_name, pairs.benchmark);
    const double annual_returns =
        annual_return(paired_returns, annualization, returns_column_name);
    const double annual_benchmark =
        annual_return(paired_benchmark, annualization, benchmark_column_name);
    if (!std::isfinite(annual_returns) || !std::isfinite(annual_benchmark) ||
        annual_benchmark == 0.0) {
        return nan;
    }
    return finite_quotient(annual_returns, annual_benchmark);
}

}  // namespace

double sharpe_ratio(const dataframe& returns,
                    double risk_free,
                    double annualization,
                    std::string_view column) {
    const auto values = valid_values(require_double_column(returns, column));
    if (values.size() < 2) {
        return nan;
    }

    std::vector<double> excess;
    excess.reserve(values.size());
    for (const double value : values) {
        excess.push_back(value - risk_free);
    }
    const double average = mean(excess);
    double squared_deviations = 0.0;
    for (const double value : excess) {
        const double deviation = value - average;
        squared_deviations += deviation * deviation;
    }
    const double sample_standard_deviation =
        std::sqrt(squared_deviations / static_cast<double>(excess.size() - 1));
    return average * std::sqrt(annualization) / sample_standard_deviation;
}

double adjusted_sharpe_ratio(const dataframe& returns,
                             double risk_free,
                             std::string_view column) {
    if (column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    const auto raw_values = require_double_column(returns, column);
    if (raw_values.size() < 4) {
        return nan;
    }

    const double sharpe =
        sharpe_ratio(returns, risk_free, daily_annualization, column);
    if (std::isnan(sharpe)) {
        return nan;
    }

    const biased_moments moments = nan_only_biased_moments(raw_values);
    const double skew = std::isnan(moments.skew) ? 0.0 : moments.skew;
    const double kurt =
        std::isnan(moments.kurtosis) ? 0.0 : moments.kurtosis;

    const double n = static_cast<double>(raw_values.size());
    const double dampening = std::min(1.0, n / 50.0);
    const double skew_adjustment = (skew / 6.0) * sharpe * dampening;
    const double kurt_adjustment =
        (kurt / 24.0) * (sharpe * sharpe) * dampening;
    double adjustment = 1.0 + skew_adjustment - kurt_adjustment;

    if (raw_values.size() < 20) {
        adjustment = std::max(0.9, std::min(1.1, adjustment));
    } else {
        adjustment = std::max(0.8, std::min(1.3, adjustment));
    }

    return sharpe * adjustment;
}

double sortino_ratio(const dataframe& returns,
                     double required_return,
                     double annualization,
                     std::string_view column) {
    const auto original_values = require_double_column(returns, column);
    if (original_values.size() < 2) {
        return nan;
    }
    const auto values = valid_values(original_values);
    if (values.empty()) {
        return nan;
    }

    std::vector<double> excess;
    excess.reserve(values.size());
    double squared_downside = 0.0;
    for (const double value : values) {
        const double excess_value = value - required_return;
        excess.push_back(excess_value);
        if (excess_value < 0.0) {
            squared_downside += excess_value * excess_value;
        }
    }
    const double annualized_mean = mean(excess) * annualization;
    const double annualized_downside = std::sqrt(
        squared_downside / static_cast<double>(excess.size())) * std::sqrt(annualization);
    return annualized_mean / annualized_downside;
}

double calmar_ratio(const dataframe& returns,
                    double risk_free,
                    double annualization,
                    std::string_view column) {
    const double maximum_drawdown = max_drawdown(returns, column);
    if (!(maximum_drawdown < 0.0)) {
        return nan;
    }
    return finite_quotient(annual_return(returns, annualization, column) - risk_free,
                           std::fabs(maximum_drawdown));
}

double mar_ratio(const dataframe& returns,
                 double annualization,
                 std::string_view column) {
    if (column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    const auto raw_values = require_double_column(returns, column);
    if (raw_values.size() < 2) {
        return nan;
    }

    const double maximum_drawdown = max_drawdown(returns, column);
    if (maximum_drawdown >= 0.0) {
        return nan;
    }

    const auto values = valid_values(raw_values);
    if (values.empty()) {
        return nan;
    }

    const double result =
        mean(values) * annualization / std::fabs(maximum_drawdown);
    if (std::isinf(result)) {
        return nan;
    }
    return result;
}

double common_sense_ratio(const dataframe& returns, std::string_view column) {
    if (column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    if (require_double_column(returns, column).size() < 2) {
        return nan;
    }

    const double return_tail_ratio = tail_ratio(returns, column);
    const double return_win_rate = win_rate(returns, column);
    if (std::isnan(return_tail_ratio) || std::isnan(return_win_rate) ||
        return_win_rate == 0.0) {
        return nan;
    }
    if (return_win_rate == 1.0) {
        return std::numeric_limits<double>::infinity();
    }
    return return_tail_ratio * return_win_rate / (1.0 - return_win_rate);
}

double kappa_three_ratio(const dataframe& returns,
                         double mar,
                         std::string_view column) {
    if (column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    const auto raw_values = require_double_column(returns, column);
    if (raw_values.size() < 2) {
        return nan;
    }
    const auto values = valid_values(raw_values);
    if (values.size() < 2) {
        return nan;
    }
    if (std::isnan(mar)) {
        return nan;
    }

    double lpm3_total = 0.0;
    for (const double value : values) {
        const double deviation = mar - value;
        if (std::isnan(deviation)) {
            return nan;
        }
        const double downside = std::max(0.0, deviation);
        lpm3_total += downside * downside * downside;
    }
    const double lpm3 = lpm3_total / static_cast<double>(values.size());
    const double mu = mean(values);
    if (lpm3 < 1.0e-30) {
        return mu > mar ? std::numeric_limits<double>::infinity() : nan;
    }
    return (mu - mar) / std::pow(lpm3, 1.0 / 3.0);
}

double excess_sharpe(const dataframe& returns,
                     const dataframe& benchmark,
                     std::string_view returns_column_name,
                     std::string_view benchmark_column_name) {
    const auto pairs = align_finite_pairs(returns, benchmark, returns_column_name,
                                          benchmark_column_name);
    const auto active = active_returns(pairs);
    const double tracking_error = sample_standard_deviation(active);
    if (!(tracking_error >= minimum_tracking_error)) {
        return nan;
    }
    return finite_quotient(mean(active), tracking_error);
}

double information_ratio(const dataframe& returns,
                         const dataframe& benchmark,
                         double annualization,
                         std::string_view returns_column_name,
                         std::string_view benchmark_column_name) {
    const auto pairs = align_finite_pairs(returns, benchmark, returns_column_name,
                                          benchmark_column_name);
    const auto active = active_returns(pairs);
    if (active.size() < 2 || !std::isfinite(annualization) || annualization <= 0.0) {
        return nan;
    }

    const double sample_deviation = sample_standard_deviation(active);
    const double annualized_tracking_error =
        sample_deviation * std::sqrt(annualization);
    if (!(annualized_tracking_error > 0.0) ||
        !std::isfinite(annualized_tracking_error)) {
        return nan;
    }
    return finite_quotient(mean(active) * annualization, annualized_tracking_error);
}

double omega_ratio(const dataframe& returns,
                   double risk_free,
                   double required_return,
                   double annualization,
                   std::string_view column) {
    const auto values = detail::finite_values(require_double_column(returns, column));
    if (values.size() < 2 || !std::isfinite(risk_free) ||
        !std::isfinite(required_return) || !std::isfinite(annualization) ||
        annualization <= 0.0) {
        return nan;
    }

    double return_threshold = required_return;
    if (annualization != 1.0) {
        if (required_return <= -1.0) {
            return nan;
        }
        return_threshold = std::pow(1.0 + required_return, 1.0 / annualization) - 1.0;
    }
    const double threshold = risk_free + return_threshold;
    if (!std::isfinite(return_threshold) || !std::isfinite(threshold)) {
        return nan;
    }

    double gains = 0.0;
    double losses = 0.0;
    for (const double value : values) {
        const double excess = value - threshold;
        if (excess > 0.0) {
            gains += excess;
        } else if (excess < 0.0) {
            losses -= excess;
        }
    }
    if (!(losses > 0.0) || !std::isfinite(gains) || !std::isfinite(losses)) {
        return nan;
    }
    return finite_quotient(gains, losses);
}

double stability_of_timeseries(const dataframe& returns, std::string_view column) {
    const auto values = detail::finite_values(require_double_column(returns, column));
    if (values.size() < 2) {
        return nan;
    }

    std::vector<double> cumulative_log_returns;
    cumulative_log_returns.reserve(values.size());
    double cumulative = 0.0;
    for (const double value : values) {
        const double log_return = std::log1p(value);
        if (!std::isfinite(log_return)) {
            return nan;
        }
        cumulative += log_return;
        if (!std::isfinite(cumulative)) {
            return nan;
        }
        cumulative_log_returns.push_back(cumulative);
    }

    const double count = static_cast<double>(cumulative_log_returns.size());
    const double mean_time = (count - 1.0) / 2.0;
    const double mean_response = mean(cumulative_log_returns);
    double time_sum_of_squares = 0.0;
    double response_sum_of_squares = 0.0;
    double cross_sum = 0.0;
    for (std::size_t position = 0; position < cumulative_log_returns.size(); ++position) {
        const double time_residual = static_cast<double>(position) - mean_time;
        const double response_residual = cumulative_log_returns[position] - mean_response;
        time_sum_of_squares += time_residual * time_residual;
        response_sum_of_squares += response_residual * response_residual;
        cross_sum += time_residual * response_residual;
    }
    if (!(time_sum_of_squares > 0.0) || !(response_sum_of_squares > 0.0) ||
        !std::isfinite(cross_sum)) {
        return nan;
    }
    const double result = cross_sum * cross_sum /
                          (time_sum_of_squares * response_sum_of_squares);
    return std::isfinite(result) ? result : nan;
}

double capture(const dataframe& returns,
               const dataframe& benchmark,
               double annualization,
               std::string_view returns_column_name,
               std::string_view benchmark_column_name) {
    const auto pairs = align_finite_pairs(returns, benchmark, returns_column_name,
                                          benchmark_column_name);
    return capture_from_pairs(pairs, annualization, returns_column_name,
                              benchmark_column_name);
}

double up_capture(const dataframe& returns,
                  const dataframe& benchmark,
                  double annualization,
                  std::string_view returns_column_name,
                  std::string_view benchmark_column_name) {
    const auto pairs = align_finite_pairs(returns, benchmark, returns_column_name,
                                          benchmark_column_name);
    return capture_from_pairs(select_benchmark_sign(pairs, true), annualization,
                              returns_column_name, benchmark_column_name);
}

double down_capture(const dataframe& returns,
                    const dataframe& benchmark,
                    double annualization,
                    std::string_view returns_column_name,
                    std::string_view benchmark_column_name) {
    const auto pairs = align_finite_pairs(returns, benchmark, returns_column_name,
                                          benchmark_column_name);
    return capture_from_pairs(select_benchmark_sign(pairs, false), annualization,
                              returns_column_name, benchmark_column_name);
}

double up_down_capture(const dataframe& returns,
                       const dataframe& benchmark,
                       double annualization,
                       std::string_view returns_column_name,
                       std::string_view benchmark_column_name) {
    const auto pairs = align_finite_pairs(returns, benchmark, returns_column_name,
                                          benchmark_column_name);
    const double up = capture_from_pairs(select_benchmark_sign(pairs, true), annualization,
                                         returns_column_name, benchmark_column_name);
    const double down = capture_from_pairs(select_benchmark_sign(pairs, false), annualization,
                                           returns_column_name, benchmark_column_name);
    if (!std::isfinite(up) || !std::isfinite(down) || down == 0.0) {
        return nan;
    }
    return finite_quotient(up, down);
}

}  // namespace fincore
