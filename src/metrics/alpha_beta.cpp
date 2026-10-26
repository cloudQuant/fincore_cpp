#include <fincore/metrics/alpha_beta.hpp>

#include <fincore/calendar.hpp>

#include <cmath>
#include <limits>
#include <map>
#include <utility>
#include <vector>

namespace fincore {
namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();
constexpr double minimum_variance = 1.0e-30;

struct paired_returns {
    std::vector<double> returns;
    std::vector<double> benchmark;
};

struct finite_aligned_pairs {
    std::vector<timestamp> index;
    std::vector<double> returns;
    std::vector<double> benchmark;
};

struct yearly_paired_returns {
    std::vector<timestamp> index;
    std::vector<double> returns;
    std::vector<double> benchmark;
};

enum class benchmark_selection { all, up, down };

// Zero belongs to the down side here, which is the opposite of the strict sign
// split the capture and up/down ratio helpers use; ExtendedAlphaBeta.
// IncludesZeroBenchmarkInDownSelection pins it, and it is what lets the three
// selections cover every finite pair without leaving a gap between them.
bool selects_benchmark(double benchmark_value, benchmark_selection selection) {
    switch (selection) {
    case benchmark_selection::all:
        return true;
    case benchmark_selection::up:
        return benchmark_value > 0.0;
    case benchmark_selection::down:
        return benchmark_value <= 0.0;
    }
    return false;
}

finite_aligned_pairs aligned_finite_selected_pairs(
    const dataframe& returns,
    const dataframe& benchmark,
    std::string_view returns_column_name,
    std::string_view benchmark_column_name,
    benchmark_selection selection) {
    auto [aligned_returns, aligned_benchmark] = align_on_common_index(
        returns, returns_column_name, benchmark, benchmark_column_name);
    const auto return_values =
        require_double_column(aligned_returns, returns_column_name);
    const auto benchmark_values =
        require_double_column(aligned_benchmark, benchmark_column_name);
    const auto& index = aligned_returns.get_index();

    finite_aligned_pairs result;
    result.index.reserve(index.size());
    result.returns.reserve(return_values.size());
    result.benchmark.reserve(benchmark_values.size());
    for (std::size_t position = 0; position < return_values.size(); ++position) {
        const double return_value = return_values[position];
        const double benchmark_value = benchmark_values[position];
        if (!std::isfinite(return_value) || !std::isfinite(benchmark_value) ||
            !selects_benchmark(benchmark_value, selection)) {
            continue;
        }
        result.index.push_back(index[position]);
        result.returns.push_back(return_value);
        result.benchmark.push_back(benchmark_value);
    }
    return result;
}

std::pair<double, double> nan_pair() {
    return {nan, nan};
}

// Two different filters live in this file on purpose: the pair above drops
// non-finite values because it feeds a conditional selection, while this one
// drops only NaN so an infinity stays a sample and makes the regression result
// non-finite rather than silently excluded.
paired_returns aligned_valid_pairs(const dataframe& returns,
                                   const dataframe& benchmark,
                                   std::string_view returns_column_name,
                                   std::string_view benchmark_column_name,
                                   double risk_free) {
    auto [aligned_returns, aligned_benchmark] = align_on_common_index(
        returns, returns_column_name, benchmark, benchmark_column_name);
    const auto return_values =
        require_double_column(aligned_returns, returns_column_name);
    const auto benchmark_values =
        require_double_column(aligned_benchmark, benchmark_column_name);

    paired_returns result;
    result.returns.reserve(return_values.size());
    result.benchmark.reserve(benchmark_values.size());
    for (std::size_t position = 0; position < return_values.size(); ++position) {
        if (!std::isnan(return_values[position]) && !std::isnan(benchmark_values[position])) {
            result.returns.push_back(return_values[position] - risk_free);
            result.benchmark.push_back(benchmark_values[position] - risk_free);
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

double beta_from_pairs(const paired_returns& pairs) {
    if (pairs.returns.size() < 2) {
        return nan;
    }

    const double benchmark_mean = mean(pairs.benchmark);
    double covariance = 0.0;
    double benchmark_variance = 0.0;
    for (std::size_t position = 0; position < pairs.returns.size(); ++position) {
        const double benchmark_residual = pairs.benchmark[position] - benchmark_mean;
        covariance += benchmark_residual * pairs.returns[position];
        benchmark_variance += benchmark_residual * benchmark_residual;
    }
    covariance /= static_cast<double>(pairs.returns.size());
    benchmark_variance /= static_cast<double>(pairs.returns.size());
    if (!(benchmark_variance >= minimum_variance)) {
        return nan;
    }
    const double result = covariance / benchmark_variance;
    return std::isfinite(result) ? result : nan;
}

std::map<int, yearly_paired_returns> group_aligned_by_utc_year(
    const dataframe& returns,
    const dataframe& benchmark,
    std::string_view returns_column_name,
    std::string_view benchmark_column_name) {
    const auto return_values =
        require_double_column(returns, returns_column_name);
    const auto benchmark_values =
        require_double_column(benchmark, benchmark_column_name);
    const auto& index = returns.get_index();

    std::map<int, yearly_paired_returns> buckets;
    for (std::size_t position = 0; position < index.size(); ++position) {
        auto& bucket = buckets[utc_year(index[position])];
        bucket.index.push_back(index[position]);
        bucket.returns.push_back(return_values[position]);
        bucket.benchmark.push_back(benchmark_values[position]);
    }
    return buckets;
}

template <typename Metric>
dataframe aggregate_aligned_by_utc_year(
    const dataframe& aligned_returns,
    const dataframe& aligned_benchmark,
    std::string_view returns_column_name,
    std::string_view benchmark_column_name,
    std::string_view output_column_name,
    Metric metric) {
    auto buckets = group_aligned_by_utc_year(
        aligned_returns, aligned_benchmark, returns_column_name,
        benchmark_column_name);
    std::vector<timestamp> result_index;
    std::vector<double> result_values;
    result_index.reserve(buckets.size());
    result_values.reserve(buckets.size());
    for (auto& [year, bucket] : buckets) {
        result_index.push_back(utc_timestamp(year));
        auto benchmark_index = bucket.index;
        const auto group_returns =
            make_series(std::move(bucket.index), returns_column_name,
                        std::move(bucket.returns));
        const auto group_benchmark =
            make_series(std::move(benchmark_index), benchmark_column_name,
                        std::move(bucket.benchmark));
        result_values.push_back(metric(group_returns, group_benchmark));
    }
    return make_series(std::move(result_index), output_column_name,
                       std::move(result_values));
}

std::pair<double, double> selected_alpha_beta(
    const dataframe& returns,
    const dataframe& benchmark,
    double risk_free,
    double annualization,
    std::string_view returns_column_name,
    std::string_view benchmark_column_name,
    benchmark_selection selection) {
    const auto pairs = aligned_finite_selected_pairs(
        returns, benchmark, returns_column_name, benchmark_column_name, selection);
    if (pairs.index.size() < 2 || !std::isfinite(risk_free) ||
        !std::isfinite(annualization) || annualization <= 0.0) {
        return nan_pair();
    }

    const auto paired_returns =
        make_series(pairs.index, returns_column_name, pairs.returns);
    const auto paired_benchmark =
        make_series(pairs.index, benchmark_column_name, pairs.benchmark);
    const double alpha_value = alpha(paired_returns, paired_benchmark, risk_free,
                                     annualization, returns_column_name,
                                     benchmark_column_name);
    const double beta_value = beta(paired_returns, paired_benchmark, risk_free,
                                   returns_column_name, benchmark_column_name);
    if (!std::isfinite(alpha_value) || !std::isfinite(beta_value)) {
        return nan_pair();
    }
    return {alpha_value, beta_value};
}

}  // namespace

double beta(const dataframe& returns,
            const dataframe& benchmark,
            double risk_free,
            std::string_view returns_column_name,
            std::string_view benchmark_column_name) {
    return beta_from_pairs(aligned_valid_pairs(returns, benchmark, returns_column_name,
                                               benchmark_column_name, risk_free));
}

double alpha(const dataframe& returns,
             const dataframe& benchmark,
             double risk_free,
             double annualization,
             std::string_view returns_column_name,
             std::string_view benchmark_column_name) {
    const auto pairs = aligned_valid_pairs(returns, benchmark, returns_column_name,
                                           benchmark_column_name, risk_free);
    const double beta_value = beta_from_pairs(pairs);
    if (!std::isfinite(beta_value)) {
        return nan;
    }

    double total_alpha = 0.0;
    for (std::size_t position = 0; position < pairs.returns.size(); ++position) {
        total_alpha += pairs.returns[position] - beta_value * pairs.benchmark[position];
    }
    const double result = std::pow(1.0 + total_alpha / static_cast<double>(pairs.returns.size()),
                                   annualization) - 1.0;
    return std::isfinite(result) ? result : nan;
}

dataframe annual_alpha(const dataframe& returns,
                       const dataframe& benchmark,
                       double risk_free,
                       frequency period,
                       std::optional<double> annualization,
                       std::string_view returns_column_name,
                       std::string_view benchmark_column_name) {
    const auto [aligned_returns, aligned_benchmark] = align_on_common_index(
        returns, returns_column_name, benchmark, benchmark_column_name);
    const double factor = annualization_factor(period, annualization);
    return aggregate_aligned_by_utc_year(
        aligned_returns, aligned_benchmark, returns_column_name,
        benchmark_column_name, "alpha",
        [risk_free, factor, returns_column_name,
         benchmark_column_name](const dataframe& group_returns,
                                const dataframe& group_benchmark) {
            return alpha(group_returns, group_benchmark, risk_free, factor,
                         returns_column_name, benchmark_column_name);
        });
}

dataframe annual_beta(const dataframe& returns,
                      const dataframe& benchmark,
                      double risk_free,
                      frequency period,
                      std::optional<double> annualization,
                      std::string_view returns_column_name,
                      std::string_view benchmark_column_name) {
    const auto [aligned_returns, aligned_benchmark] = align_on_common_index(
        returns, returns_column_name, benchmark, benchmark_column_name);
    static_cast<void>(period);
    static_cast<void>(annualization);
    return aggregate_aligned_by_utc_year(
        aligned_returns, aligned_benchmark, returns_column_name,
        benchmark_column_name, "beta",
        [risk_free, returns_column_name,
         benchmark_column_name](const dataframe& group_returns,
                                const dataframe& group_benchmark) {
            return beta(group_returns, group_benchmark, risk_free,
                        returns_column_name, benchmark_column_name);
        });
}

std::pair<double, double> alpha_beta(
    const dataframe& returns,
    const dataframe& benchmark,
    double risk_free,
    double annualization,
    std::string_view returns_column_name,
    std::string_view benchmark_column_name) {
    return selected_alpha_beta(returns, benchmark, risk_free, annualization,
                               returns_column_name, benchmark_column_name,
                               benchmark_selection::all);
}

std::pair<double, double> up_alpha_beta(
    const dataframe& returns,
    const dataframe& benchmark,
    double risk_free,
    double annualization,
    std::string_view returns_column_name,
    std::string_view benchmark_column_name) {
    return selected_alpha_beta(returns, benchmark, risk_free, annualization,
                               returns_column_name, benchmark_column_name,
                               benchmark_selection::up);
}

std::pair<double, double> down_alpha_beta(
    const dataframe& returns,
    const dataframe& benchmark,
    double risk_free,
    double annualization,
    std::string_view returns_column_name,
    std::string_view benchmark_column_name) {
    return selected_alpha_beta(returns, benchmark, risk_free, annualization,
                               returns_column_name, benchmark_column_name,
                               benchmark_selection::down);
}

double alpha_percentile_rank(
    const dataframe& strategy_returns,
    std::span<const dataframe> peer_returns,
    const dataframe& benchmark,
    double risk_free,
    double annualization,
    std::string_view returns_column_name,
    std::string_view benchmark_column_name) {
    if (returns_column_name.find('\0') != std::string_view::npos ||
        benchmark_column_name.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    if (require_double_column(strategy_returns, returns_column_name).size() <
        3U) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const double strategy_alpha =
        alpha(strategy_returns, benchmark, risk_free, annualization,
              returns_column_name, benchmark_column_name);
    if (std::isnan(strategy_alpha)) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    std::vector<double> peer_alphas;
    for (const dataframe& peer : peer_returns) {
        if (require_double_column(peer, returns_column_name).size() < 3U) {
            continue;
        }
        const double peer_alpha = alpha(peer, benchmark, risk_free,
                                        annualization, returns_column_name,
                                        benchmark_column_name);
        if (!std::isnan(peer_alpha)) {
            peer_alphas.push_back(peer_alpha);
        }
    }
    if (peer_alphas.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    std::size_t below = 0;
    for (const double peer_alpha : peer_alphas) {
        if (peer_alpha < strategy_alpha) {
            ++below;
        }
    }
    return static_cast<double>(below) /
           static_cast<double>(peer_alphas.size());
}

}  // namespace fincore
