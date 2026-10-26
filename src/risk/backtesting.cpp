#include <fincore/risk/backtesting.hpp>

#include <fincore/detail/digest.hpp>

#include <fincore/dataframe.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <random>
#include <vector>

namespace fincore {
namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();
constexpr std::size_t minimum_observations = 3U;
constexpr double minimum_expected_exceptions = 5.0;

double xlog(const double value) {
    return value > 0.0 ? value * std::log(value) : 0.0;
}

double chi_square_survival_df1(const double statistic) {
    if (!(statistic >= 0.0) || !std::isfinite(statistic)) {
        return 0.0;
    }
    return std::erfc(std::sqrt(statistic / 2.0));
}

struct aligned_series {
    std::vector<timestamp> index;
    std::vector<double> forecast;
    std::vector<double> realized;
};

aligned_series align_finite_pairs(const dataframe& forecast,
                                  const dataframe& realized,
                                  const std::string_view forecast_column,
                                  const std::string_view realized_column) {
    if (forecast_column.find('\0') != std::string_view::npos ||
        realized_column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    const auto aligned = align_on_common_index(forecast, forecast_column,
                                               realized, realized_column);
    const auto forecast_values =
        require_double_column(aligned.first, forecast_column);
    const auto realized_values =
        require_double_column(aligned.second, realized_column);
    aligned_series result;
    for (std::size_t position = 0; position < forecast_values.size();
         ++position) {
        if (std::isnan(forecast_values[position]) ||
            std::isnan(realized_values[position])) {
            continue;
        }
        result.index.push_back(aligned.first.get_index()[position]);
        result.forecast.push_back(forecast_values[position]);
        result.realized.push_back(realized_values[position]);
    }
    if (result.index.empty()) {
        throw input_error{"forecast and realized share no overlapping timestamps"};
    }
    return result;
}

std::string digest_of(const aligned_series& series) {
    std::vector<std::uint8_t> payload;
    payload.reserve(series.index.size() * 24U);
    for (std::size_t position = 0; position < series.index.size();
         ++position) {
        const std::int64_t date = series.index[position];
        const std::uint64_t forecast = [&] {
            std::uint64_t bits = 0;
            std::memcpy(&bits, &series.forecast[position], sizeof(bits));
            return bits;
        }();
        const std::uint64_t realized = [&] {
            std::uint64_t bits = 0;
            std::memcpy(&bits, &series.realized[position], sizeof(bits));
            return bits;
        }();
        for (int shift = 56; shift >= 0; shift -= 8) {
            payload.push_back(static_cast<std::uint8_t>((date >> shift) & 0xffU));
        }
        for (int shift = 56; shift >= 0; shift -= 8) {
            payload.push_back(static_cast<std::uint8_t>((forecast >> shift) & 0xffU));
        }
        for (int shift = 56; shift >= 0; shift -= 8) {
            payload.push_back(static_cast<std::uint8_t>((realized >> shift) & 0xffU));
        }
    }
    return detail::sha256_hex(payload);
}

double linear_quantile(std::vector<double> values, const double q) {
    if (values.empty()) {
        return nan;
    }
    std::sort(values.begin(), values.end());
    const double virtual_index =
        static_cast<double>(values.size() - 1) * q;
    const auto lower = static_cast<std::size_t>(std::floor(virtual_index));
    const auto upper = static_cast<std::size_t>(std::ceil(virtual_index));
    if (lower == upper) {
        return values[lower];
    }
    const double difference = values[upper] - values[lower];
    const double fraction = virtual_index - static_cast<double>(lower);
    return fraction < 0.5 ? values[lower] + difference * fraction
                          : values[upper] - difference * (1.0 - fraction);
}

double binomial_log_pmf(const std::size_t k, const std::size_t n,
                        const double p) {
    // Degenerate probabilities are resolved exactly instead of through
    // log(0); a VaR tail probability can legitimately be 1 - confidence = 0.
    if (p <= 0.0) {
        return k == 0U ? 0.0 : -std::numeric_limits<double>::infinity();
    }
    if (p >= 1.0) {
        return k == n ? 0.0 : -std::numeric_limits<double>::infinity();
    }
    // Kept in log space with lgamma and log1p: the backtest thresholds compare
    // probabilities near 0.9999, where the raw binomial term would underflow.
    return std::lgamma(static_cast<double>(n) + 1.0) -
           std::lgamma(static_cast<double>(k) + 1.0) -
           std::lgamma(static_cast<double>(n - k) + 1.0) +
           static_cast<double>(k) * std::log(p) +
           static_cast<double>(n - k) * std::log1p(-p);
}

double binomial_cdf(const std::size_t k, const std::size_t n,
                    const double p) {
    if (k >= n) {
        return 1.0;
    }
    if (p <= 0.0) {
        return 1.0;
    }
    if (p >= 1.0) {
        return 0.0;
    }
    const auto mode = static_cast<std::size_t>(
        std::floor((static_cast<double>(n) + 1.0) * p));
    // Recurring outward from the mode with the pmf ratio keeps the number of
    // summed terms small; below the mode the mass is accumulated directly,
    // above it the upper tail is summed and subtracted so the result stays
    // inside [0, 1] instead of losing precision to cancellation.
    if (k < mode) {
        double pmf = std::exp(binomial_log_pmf(k, n, p));
        double total = pmf;
        for (std::size_t value = k; value > 0U; --value) {
            pmf *= (static_cast<double>(value) * (1.0 - p)) /
                   (static_cast<double>(n - value + 1U) * p);
            total += pmf;
        }
        return std::min(1.0, total);
    }
    double pmf = std::exp(binomial_log_pmf(k, n, p));
    double tail = 0.0;
    for (std::size_t value = k; value < n; ++value) {
        pmf *= (static_cast<double>(n - value) * p) /
               (static_cast<double>(value + 1U) * (1.0 - p));
        tail += pmf;
    }
    return std::max(0.0, 1.0 - tail);
}

std::size_t binomial_quantile(const double probability, const std::size_t n,
                              const double p) {
    std::size_t low = 0U;
    std::size_t high = n;
    while (low < high) {
        const std::size_t middle = low + (high - low) / 2U;
        if (binomial_cdf(middle, n, p) >= probability) {
            high = middle;
        } else {
            low = middle + 1U;
        }
    }
    return low;
}

}  // namespace

double kupiec_lr(const std::size_t observations,
                 const std::size_t exceptions,
                 const double confidence_level) {
    if (observations == 0U) {
        return 0.0;
    }
    const double p = 1.0 - confidence_level;
    const double n = static_cast<double>(observations);
    const double x = static_cast<double>(exceptions);
    const double term1 = x * std::log(x / (n * p));
    const double term2 =
        (n - x) * std::log((n - x) / (n * (1.0 - p)));
    const double safe_term1 = exceptions == 0U ? 0.0 : term1;
    const double safe_term2 = exceptions == observations ? 0.0 : term2;
    return std::max(0.0, 2.0 * (safe_term1 + safe_term2));
}

double christoffersen_lr(const std::span<const int> exceptions) {
    if (exceptions.size() < 2U) {
        return 0.0;
    }
    double n00 = 0.0;
    double n01 = 0.0;
    double n10 = 0.0;
    double n11 = 0.0;
    for (std::size_t position = 1; position < exceptions.size(); ++position) {
        const int previous = exceptions[position - 1U];
        const int current = exceptions[position];
        if (previous == 0 && current == 0) {
            n00 += 1.0;
        } else if (previous == 0 && current == 1) {
            n01 += 1.0;
        } else if (previous == 1 && current == 0) {
            n10 += 1.0;
        } else {
            n11 += 1.0;
        }
    }
    const double total = n00 + n01 + n10 + n11;
    if (total == 0.0) {
        return 0.0;
    }
    return 2.0 * (xlog(n00) + xlog(n01) + xlog(n10) + xlog(n11) -
                  xlog(n00 + n01) - xlog(n10 + n11) -
                  xlog(n00 + n10) - xlog(n01 + n11) + xlog(total));
}

risk_backtest_result backtest_var(
    const dataframe& forecast, const dataframe& realized,
    const double confidence_level, const double significance,
    const std::string_view forecast_column,
    const std::string_view realized_column) {
    if (!(confidence_level > 0.0 && confidence_level < 1.0)) {
        throw input_error{"confidence_level must be in (0, 1)"};
    }
    const auto series =
        align_finite_pairs(forecast, realized, forecast_column,
                           realized_column);
    std::vector<int> exception_flags;
    exception_flags.reserve(series.index.size());
    std::size_t exceptions = 0U;
    for (std::size_t position = 0; position < series.index.size();
         ++position) {
        const int flag =
            series.realized[position] < series.forecast[position] ? 1 : 0;
        exception_flags.push_back(flag);
        exceptions += static_cast<std::size_t>(flag);
    }
    const std::size_t observations = series.index.size();
    const double p = 1.0 - confidence_level;
    const double kupiec =
        kupiec_lr(observations, exceptions, confidence_level);
    const double kupiec_pvalue = chi_square_survival_df1(kupiec);
    const double christoffersen = christoffersen_lr(exception_flags);
    const double christoffersen_pvalue =
        chi_square_survival_df1(christoffersen);
    const double expected = static_cast<double>(observations) * p;
    risk_backtest_status status = risk_backtest_status::pass;
    if (static_cast<double>(observations) <
            static_cast<double>(minimum_observations) ||
        expected < minimum_expected_exceptions) {
        status = risk_backtest_status::inconclusive;
    } else if (kupiec_pvalue < significance ||
               christoffersen_pvalue < significance) {
        status = risk_backtest_status::fail;
    }
    return {risk_backtest_method::var,
            confidence_level,
            observations,
            exceptions,
            expected,
            observations == 0U
                ? 0.0
                : static_cast<double>(exceptions) /
                      static_cast<double>(observations),
            kupiec,
            kupiec_pvalue,
            christoffersen,
            christoffersen_pvalue,
            digest_of(series),
            {{"significance", significance},
             {"small_sample",
              expected < minimum_expected_exceptions ? 1.0 : 0.0}},
            status};
}

risk_backtest_result backtest_es(
    const dataframe& forecast, const dataframe& realized,
    const double confidence_level, const std::size_t n_bootstrap,
    const std::uint64_t seed, const std::string_view forecast_column,
    const std::string_view realized_column) {
    if (!(confidence_level > 0.0 && confidence_level < 1.0)) {
        throw input_error{"confidence_level must be in (0, 1)"};
    }
    const auto series =
        align_finite_pairs(forecast, realized, forecast_column,
                           realized_column);
    const double threshold =
        linear_quantile(series.realized, 1.0 - confidence_level);
    std::vector<double> tail;
    for (const double value : series.realized) {
        if (value < threshold) {
            tail.push_back(value);
        }
    }
    double realized_es = 0.0;
    if (!tail.empty()) {
        for (const double value : tail) {
            realized_es += value;
        }
        realized_es /= static_cast<double>(tail.size());
    }
    double forecast_es = 0.0;
    for (const double value : series.forecast) {
        forecast_es += value;
    }
    forecast_es /= static_cast<double>(series.forecast.size());

    std::mt19937_64 engine{seed};
    std::uniform_int_distribution<std::size_t> pick(
        0U, tail.empty() ? 0U : tail.size() - 1U);
    std::vector<double> boot_means;
    boot_means.reserve(n_bootstrap);
    for (std::size_t sample_index = 0; sample_index < n_bootstrap;
         ++sample_index) {
        double total = 0.0;
        if (tail.empty()) {
            total = 0.0;
        } else {
            for (std::size_t draw = 0; draw < tail.size(); ++draw) {
                total += tail[pick(engine)];
            }
            total /= static_cast<double>(tail.size());
        }
        boot_means.push_back(total);
    }
    double boot_mean = 0.0;
    for (const double value : boot_means) {
        boot_mean += value;
    }
    boot_mean /= static_cast<double>(boot_means.size());
    double boot_variance = 0.0;
    for (const double value : boot_means) {
        boot_variance += (value - boot_mean) * (value - boot_mean);
    }
    boot_variance /= static_cast<double>(boot_means.size());
    const double boot_std = std::sqrt(boot_variance);
    const double z_score =
        boot_std > 0.0 ? (realized_es - forecast_es) / boot_std : 0.0;
    return {risk_backtest_method::es,
            confidence_level,
            series.index.size(),
            tail.size(),
            static_cast<double>(series.index.size()) *
                (1.0 - confidence_level),
            series.index.empty()
                ? 0.0
                : static_cast<double>(tail.size()) /
                      static_cast<double>(series.index.size()),
            0.0,
            1.0,
            0.0,
            1.0,
            digest_of(series),
            {{"realized_es", realized_es},
             {"forecast_es", forecast_es},
             {"z_score", z_score},
             {"n_bootstrap", static_cast<double>(n_bootstrap)},
             {"seed", static_cast<double>(seed)}},
            risk_backtest_status::experimental};
}

double expected_exception_count(const std::size_t observations,
                                const double confidence_level) {
    return static_cast<double>(observations) * (1.0 - confidence_level);
}

std::string basel_traffic_light(const std::size_t exceptions,
                                const std::size_t observations,
                                const double confidence_level) {
    if (observations == 0U) {
        return "green";
    }
    if (!std::isfinite(confidence_level) ||
        !(confidence_level > 0.0 && confidence_level < 1.0)) {
        throw input_error{"confidence_level must be in (0, 1)"};
    }
    const double p = 1.0 - confidence_level;
    const std::size_t green_max =
        binomial_quantile(0.95, observations, p);
    const std::size_t red_min =
        binomial_quantile(0.9999, observations, p);
    if (exceptions >= red_min) {
        return "red";
    }
    if (exceptions >= green_max) {
        return "yellow";
    }
    return "green";
}

double es_calibration_score(const double forecast_es,
                            const std::span<const double> realized,
                            const double confidence_level) {
    const double threshold = linear_quantile(
        std::vector<double>(realized.begin(), realized.end()),
        1.0 - confidence_level);
    double total = 0.0;
    std::size_t count = 0U;
    for (const double value : realized) {
        if (value <= threshold) {
            total += value;
            ++count;
        }
    }
    const double realized_es =
        count == 0U ? 0.0 : total / static_cast<double>(count);
    if (std::abs(forecast_es) < 1e-15) {
        return nan;
    }
    return (realized_es - forecast_es) / std::abs(forecast_es);
}

std::string_view risk_backtest_status_name(
    const risk_backtest_status status) {
    switch (status) {
        case risk_backtest_status::pass:
            return "pass";
        case risk_backtest_status::fail:
            return "fail";
        case risk_backtest_status::inconclusive:
            return "inconclusive";
        case risk_backtest_status::experimental:
            return "experimental";
    }
    return "inconclusive";
}

}  // namespace fincore
