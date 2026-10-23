#include <fincore/risk/backtesting.hpp>

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

std::uint32_t rotate_right(const std::uint32_t value, const int bits) {
    return (value >> bits) | (value << (32 - bits));
}

std::string sha256_hex(const std::vector<std::uint8_t>& payload) {
    static constexpr std::array<std::uint32_t, 64> constants{
        0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU,
        0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U,
        0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U,
        0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
        0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U,
        0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U,
        0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
        0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
        0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U,
        0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U, 0x1e376c08U,
        0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU,
        0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
        0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};
    std::array<std::uint32_t, 8> state{0x6a09e667U, 0xbb67ae85U,
                                       0x3c6ef372U, 0xa54ff53aU,
                                       0x510e527fU, 0x9b05688cU,
                                       0x1f83d9abU, 0x5be0cd19U};
    std::vector<std::uint8_t> message(payload);
    const std::uint64_t bit_length =
        static_cast<std::uint64_t>(message.size()) * 8U;
    message.push_back(0x80U);
    while (message.size() % 64U != 56U) {
        message.push_back(0U);
    }
    for (int shift = 56; shift >= 0; shift -= 8) {
        message.push_back(static_cast<std::uint8_t>((bit_length >> shift) & 0xffU));
    }
    for (std::size_t block = 0; block < message.size(); block += 64U) {
        std::array<std::uint32_t, 64> schedule{};
        for (std::size_t word = 0; word < 16U; ++word) {
            schedule[word] =
                (static_cast<std::uint32_t>(message[block + word * 4U]) << 24) |
                (static_cast<std::uint32_t>(message[block + word * 4U + 1U]) << 16) |
                (static_cast<std::uint32_t>(message[block + word * 4U + 2U]) << 8) |
                static_cast<std::uint32_t>(message[block + word * 4U + 3U]);
        }
        for (std::size_t word = 16U; word < 64U; ++word) {
            const std::uint32_t s0 = rotate_right(schedule[word - 15U], 7) ^
                                     rotate_right(schedule[word - 15U], 18) ^
                                     (schedule[word - 15U] >> 3);
            const std::uint32_t s1 = rotate_right(schedule[word - 2U], 17) ^
                                     rotate_right(schedule[word - 2U], 19) ^
                                     (schedule[word - 2U] >> 10);
            schedule[word] = schedule[word - 16U] + s0 +
                             schedule[word - 7U] + s1;
        }
        auto [a, b, c, d, e, f, g, h] = state;
        for (std::size_t word = 0; word < 64U; ++word) {
            const std::uint32_t s1 = rotate_right(e, 6) ^
                                     rotate_right(e, 11) ^
                                     rotate_right(e, 25);
            const std::uint32_t choice = (e & f) ^ (~e & g);
            const std::uint32_t temp1 =
                h + s1 + choice + constants[word] + schedule[word];
            const std::uint32_t s0 = rotate_right(a, 2) ^
                                     rotate_right(a, 13) ^
                                     rotate_right(a, 22);
            const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t temp2 = s0 + majority;
            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }
        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
    }
    static constexpr char hex[] = "0123456789abcdef";
    std::string result;
    result.reserve(64U);
    for (const std::uint32_t word : state) {
        for (int shift = 28; shift >= 0; shift -= 4) {
            result.push_back(hex[(word >> shift) & 0xfU]);
        }
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
    return sha256_hex(payload);
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

}  // namespace fincore
