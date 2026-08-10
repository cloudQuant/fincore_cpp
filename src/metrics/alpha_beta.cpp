#include <fincore/metrics/alpha_beta.hpp>

#include <cmath>
#include <limits>
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

}  // namespace fincore
