#include <fincore/metrics/ratios.hpp>

#include <fincore/detail/numeric.hpp>
#include <fincore/metrics/risk.hpp>
#include <fincore/metrics/stats.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
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

double conditional_sharpe_ratio(const dataframe& returns,
                                double cutoff,
                                double risk_free,
                                double annualization,
                                std::string_view column) {
    if (column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    const auto raw_values = require_double_column(returns, column);
    if (raw_values.size() < 2) {
        return nan;
    }
    if (!(cutoff > 0.0 && cutoff < 1.0)) {
        return nan;
    }

    std::vector<double> sorted_values(raw_values.begin(), raw_values.end());
    for (const double value : sorted_values) {
        if (std::isnan(value)) {
            return nan;
        }
    }
    std::sort(sorted_values.begin(), sorted_values.end());

    const double virtual_index =
        static_cast<double>(sorted_values.size() - 1) * cutoff;
    const auto lower =
        static_cast<std::size_t>(std::floor(virtual_index));
    const auto upper =
        static_cast<std::size_t>(std::ceil(virtual_index));
    double cutoff_value = sorted_values[lower];
    if (lower != upper) {
        const double difference =
            sorted_values[upper] - sorted_values[lower];
        const double fraction =
            virtual_index - static_cast<double>(lower);
        cutoff_value = fraction < 0.5
                           ? sorted_values[lower] + difference * fraction
                           : sorted_values[upper] -
                                 difference * (1.0 - fraction);
    }

    std::vector<double> tail;
    tail.reserve(raw_values.size());
    for (const double value : raw_values) {
        if (value <= cutoff_value) {
            tail.push_back(value);
        }
    }
    if (tail.size() < 2) {
        return nan;
    }

    const double tail_mean = mean(tail);
    double squared_total = 0.0;
    for (const double value : tail) {
        const double deviation = value - tail_mean;
        squared_total += deviation * deviation;
    }
    const double tail_std = std::sqrt(
        squared_total / static_cast<double>(tail.size() - 1));
    const double excess_mean = tail_mean - risk_free;
    if (tail_std == 0.0 || !std::isfinite(excess_mean) ||
        !std::isfinite(tail_std)) {
        return nan;
    }
    return excess_mean / tail_std * std::sqrt(annualization);
}

double normal_cdf(double value) {
    return 0.5 * std::erfc(-value / std::sqrt(2.0));
}

double normal_ppf(double probability) {
    constexpr double low = 0.02425;
    constexpr double high = 1.0 - low;
    double result = 0.0;
    if (probability < low) {
        const double q = std::sqrt(-2.0 * std::log(probability));
        result = (((((-7.784894002430293e-03 * q - 3.223964580411365e-01) * q -
                      2.400758277161838e+00) * q - 2.549732539343734e+00) * q +
                    4.374664141464968e+00) * q + 2.938163982698783e+00) /
                 ((((7.784695709041462e-03 * q + 3.224671290700398e-01) * q +
                    2.445134137142996e+00) * q + 3.754408661907416e+00) * q + 1.0);
    } else if (probability <= high) {
        const double q = probability - 0.5;
        const double r = q * q;
        result = (((((-3.969683028665376e+01 * r + 2.209460984245205e+02) * r -
                      2.759285104469687e+02) * r + 1.383577518672690e+02) * r -
                    3.066479806614716e+01) * r + 2.506628277459239e+00) * q /
                 (((((-5.447609879822406e+01 * r + 1.615858368580409e+02) * r -
                      1.556989798598866e+02) * r + 6.680131188771972e+01) * r -
                    1.328068155288572e+01) * r + 1.0);
    } else {
        const double q = std::sqrt(-2.0 * std::log(1.0 - probability));
        result = -(((((-7.784894002430293e-03 * q - 3.223964580411365e-01) * q -
                       2.400758277161838e+00) * q - 2.549732539343734e+00) * q +
                     4.374664141464968e+00) * q + 2.938163982698783e+00) /
                  ((((7.784695709041462e-03 * q + 3.224671290700398e-01) * q +
                     2.445134137142996e+00) * q + 3.754408661907416e+00) * q + 1.0);
    }

    const double error = normal_cdf(result) - probability;
    const double correction =
        error * std::sqrt(2.0 * std::numbers::pi) * std::exp(result * result / 2.0);
    return result - correction / (1.0 + result * correction / 2.0);
}

double sample_skewness(const std::vector<double>& values) {
    const double count = static_cast<double>(values.size());
    if (values.size() < 3) {
        return 0.0;
    }
    const double average = mean(values);
    double squared_total = 0.0;
    for (const double value : values) {
        const double deviation = value - average;
        squared_total += deviation * deviation;
    }
    const double deviation =
        std::sqrt(squared_total / (count - 1.0));
    if (deviation == 0.0) {
        return 0.0;
    }
    double cubed_total = 0.0;
    for (const double value : values) {
        cubed_total += std::pow((value - average) / deviation, 3.0);
    }
    return count / ((count - 1.0) * (count - 2.0)) * cubed_total;
}

double sample_excess_kurtosis(const std::vector<double>& values) {
    const double count = static_cast<double>(values.size());
    if (values.size() < 4) {
        return 0.0;
    }
    const double average = mean(values);
    double squared_total = 0.0;
    for (const double value : values) {
        const double deviation = value - average;
        squared_total += deviation * deviation;
    }
    const double deviation =
        std::sqrt(squared_total / (count - 1.0));
    if (deviation == 0.0) {
        return 0.0;
    }
    double fourth_total = 0.0;
    for (const double value : values) {
        fourth_total += std::pow((value - average) / deviation, 4.0);
    }
    const double kurtosis =
        count * (count + 1.0) / ((count - 1.0) * (count - 2.0) * (count - 3.0)) *
        fourth_total;
    return kurtosis -
           3.0 * (count - 1.0) * (count - 1.0) / ((count - 2.0) * (count - 3.0));
}

double deflated_sharpe_ratio(const dataframe& returns,
                             double risk_free,
                             int num_trials,
                             std::string_view column) {
    if (column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    const auto raw_values = require_double_column(returns, column);
    const auto clean_values = valid_values(raw_values);
    if (clean_values.size() < 3) {
        return nan;
    }

    std::vector<double> excess;
    excess.reserve(clean_values.size());
    for (const double value : clean_values) {
        excess.push_back(value - risk_free);
    }

    const double count = static_cast<double>(excess.size());
    const double excess_mean = mean(excess);
    double squared_total = 0.0;
    for (const double value : excess) {
        const double deviation = value - excess_mean;
        squared_total += deviation * deviation;
    }
    const double excess_deviation =
        std::sqrt(squared_total / (count - 1.0));
    const double sr_hat =
        excess_deviation > 1e-15 ? excess_mean / excess_deviation : 0.0;

    const double gamma3 = sample_skewness(excess);
    const double gamma4 = sample_excess_kurtosis(excess) + 3.0;
    const double sr_variance =
        1.0 - gamma3 * sr_hat + (gamma4 - 1.0) / 4.0 * sr_hat * sr_hat;

    const int trials = std::max(num_trials, 1);
    double sr_star = 0.0;
    if (trials > 1) {
        constexpr double euler = 0.5772156649015329;
        const double trial_count = static_cast<double>(trials);
        const double z_inverse_n =
            normal_ppf(1.0 - 1.0 / trial_count);
        const double z_inverse_ne =
            normal_ppf(1.0 - 1.0 / (trial_count * std::numbers::e));
        sr_star = std::sqrt(std::max(sr_variance, 0.0)) *
                  ((1.0 - euler) * z_inverse_n + euler * z_inverse_ne);
    }

    if (sr_variance <= 0.0) {
        return sr_hat > sr_star ? 1.0 : 0.0;
    }

    const double z = (sr_hat - sr_star) * std::sqrt(count - 1.0) /
                     std::sqrt(sr_variance);
    return normal_cdf(z);
}

double population_standard_deviation(const std::vector<double>& values) {
    if (values.empty()) {
        return nan;
    }
    const double average = mean(values);
    double total = 0.0;
    for (const double value : values) {
        const double deviation = value - average;
        total += deviation * deviation;
    }
    return std::sqrt(total / static_cast<double>(values.size()));
}

bool all_zero_drawdowns(const std::vector<double>& drawdowns) {
    for (const double value : drawdowns) {
        if (value != 0.0) {
            return false;
        }
    }
    return true;
}

double burke_ratio(const dataframe& returns,
                   double risk_free,
                   double annualization,
                   std::string_view column) {
    if (column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    const auto raw_values = require_double_column(returns, column);
    if (raw_values.size() < 2) {
        return nan;
    }

    const auto drawdowns = get_all_drawdowns(returns, column);
    double burke_risk = 0.0;
    if (drawdowns.empty() || all_zero_drawdowns(drawdowns)) {
        const auto clean_values = valid_values(raw_values);
        std::vector<double> downside;
        for (const double value : clean_values) {
            if (value < 0.0) {
                downside.push_back(value);
            }
        }
        burke_risk =
            downside.empty()
                ? std::max(population_standard_deviation(clean_values), 1e-10)
                : population_standard_deviation(downside);
    } else {
        double squared_total = 0.0;
        for (const double drawdown : drawdowns) {
            squared_total += drawdown * drawdown;
        }
        burke_risk = std::sqrt(squared_total);
    }

    const double annual_return_value =
        annual_return(returns, annualization, column);
    if (burke_risk == 0.0 || burke_risk < 1e-10) {
        return annual_return_value - risk_free > 0.0
                   ? std::numeric_limits<double>::infinity()
                   : nan;
    }
    return (annual_return_value - risk_free) / burke_risk;
}

double sterling_ratio(const dataframe& returns,
                      double risk_free,
                      double annualization,
                      std::string_view column) {
    if (column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    const auto raw_values = require_double_column(returns, column);
    if (raw_values.size() < 2) {
        return nan;
    }

    const auto drawdowns = get_all_drawdowns(returns, column);
    double average_drawdown = 0.0;
    if (drawdowns.empty() || all_zero_drawdowns(drawdowns)) {
        const auto clean_values = valid_values(raw_values);
        std::vector<double> downside;
        for (const double value : clean_values) {
            if (value < 0.0) {
                downside.push_back(value);
            }
        }
        average_drawdown =
            downside.empty()
                ? std::max(
                      std::fabs(population_standard_deviation(clean_values)),
                      1e-10)
                : std::fabs(mean(downside));
    } else {
        average_drawdown = std::fabs(mean(drawdowns));
    }

    const double annual_return_value =
        annual_return(returns, annualization, column);
    if (average_drawdown == 0.0 || average_drawdown < 1e-10) {
        return annual_return_value - risk_free > 0.0
                   ? std::numeric_limits<double>::infinity()
                   : nan;
    }
    return (annual_return_value - risk_free) / average_drawdown;
}

double cal_treynor_ratio(const dataframe& returns,
                         const dataframe& benchmark,
                         double risk_free,
                         double annualization,
                         std::string_view returns_column_name,
                         std::string_view benchmark_column_name) {
    if (returns_column_name.find('\0') != std::string_view::npos ||
        benchmark_column_name.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    const auto aligned = align_on_common_index(returns, returns_column_name,
                                               benchmark,
                                               benchmark_column_name);
    if (require_double_column(aligned.first, returns_column_name).size() < 2) {
        return nan;
    }

    const double annual_return_value =
        annual_return(aligned.first, annualization, returns_column_name);
    const double beta_value =
        beta(aligned.first, aligned.second, risk_free, returns_column_name,
             benchmark_column_name);
    if (beta_value == 0.0 || beta_value < 0.0 || std::isnan(beta_value)) {
        return nan;
    }
    return (annual_return_value - risk_free) / beta_value;
}

double m_squared(const dataframe& returns,
                 const dataframe& benchmark,
                 double risk_free,
                 double annualization,
                 std::string_view returns_column_name,
                 std::string_view benchmark_column_name) {
    if (returns_column_name.find('\0') != std::string_view::npos ||
        benchmark_column_name.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    const auto aligned = align_on_common_index(returns, returns_column_name,
                                               benchmark,
                                               benchmark_column_name);
    if (require_double_column(aligned.first, returns_column_name).size() < 2) {
        return nan;
    }

    const double annual_return_value =
        annual_return(aligned.first, annualization, returns_column_name);
    const double portfolio_volatility =
        annual_volatility(aligned.first, annualization, returns_column_name);
    const double benchmark_volatility =
        annual_volatility(aligned.second, annualization,
                          benchmark_column_name);
    if (portfolio_volatility == 0.0 || portfolio_volatility < 0.0 ||
        std::isnan(portfolio_volatility)) {
        return nan;
    }
    return (annual_return_value - risk_free) *
               (benchmark_volatility / portfolio_volatility) +
           risk_free;
}

double capture_return_impl(const dataframe& returns,
                           const dataframe& benchmark,
                           double annualization,
                           std::string_view returns_column_name,
                           std::string_view benchmark_column_name,
                           bool upward) {
    if (returns_column_name.find('\0') != std::string_view::npos ||
        benchmark_column_name.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    const auto aligned = align_on_common_index(returns, returns_column_name,
                                               benchmark,
                                               benchmark_column_name);
    const auto values =
        require_double_column(aligned.first, returns_column_name);
    const auto factors =
        require_double_column(aligned.second, benchmark_column_name);

    std::vector<double> selected;
    selected.reserve(values.size());
    for (std::size_t position = 0; position < values.size(); ++position) {
        const bool include =
            upward ? factors[position] > 0.0 : factors[position] < 0.0;
        if (include) {
            selected.push_back(values[position]);
        }
    }
    if (selected.empty()) {
        return nan;
    }

    double wealth = 1.0;
    for (const double value : selected) {
        wealth *= 1.0 + (std::isnan(value) ? 0.0 : value);
    }
    if (wealth <= 0.0) {
        return -1.0;
    }
    return std::pow(wealth,
                    annualization / static_cast<double>(selected.size())) -
           1.0;
}

double up_capture_return(const dataframe& returns,
                         const dataframe& benchmark,
                         double annualization,
                         std::string_view returns_column_name,
                         std::string_view benchmark_column_name) {
    return capture_return_impl(returns, benchmark, annualization,
                               returns_column_name, benchmark_column_name,
                               true);
}

double down_capture_return(const dataframe& returns,
                           const dataframe& benchmark,
                           double annualization,
                           std::string_view returns_column_name,
                           std::string_view benchmark_column_name) {
    return capture_return_impl(returns, benchmark, annualization,
                               returns_column_name, benchmark_column_name,
                               false);
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
