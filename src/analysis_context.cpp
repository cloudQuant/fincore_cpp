#include <fincore/analysis_context.hpp>

#include <fincore/metrics/alpha_beta.hpp>
#include <fincore/metrics/drawdown.hpp>
#include <fincore/metrics/ratios.hpp>
#include <fincore/metrics/risk.hpp>
#include <fincore/metrics/returns.hpp>

#include <functional>
#include <string>

namespace fincore {

analysis_context::analysis_context(dataframe returns,
                                   std::optional<dataframe> benchmark,
                                   double annualization)
    : returns_{std::move(returns)},
      benchmark_{std::move(benchmark)},
      annualization_{annualization} {
    require_double_column(returns_, returns_column);
    if (benchmark_) {
        require_double_column(*benchmark_, benchmark_column);
    }
}

// Both maps are mutable members reached from const calls, so an analysis_context
// is not safe to touch from two threads at once even though every method looks
// read-only, and the counters record computations rather than requests: a cache
// hit returns early and leaves its count alone.
double analysis_context::cached(std::string_view key) const {
    const std::string key_string{key};
    if (const auto existing = scalar_cache_.find(key_string); existing != scalar_cache_.end()) {
        return existing->second;
    }

    double value = 0.0;
    if (key == "Annual return") {
        value = annual_return(returns_, annualization_);
    } else if (key == "Cumulative returns") {
        value = cumulative_return(returns_);
    } else if (key == "Annual volatility") {
        value = annual_volatility(returns_, annualization_);
    } else if (key == "Sharpe ratio") {
        value = sharpe_ratio(returns_, 0.0, annualization_);
    } else if (key == "Sortino ratio") {
        value = sortino_ratio(returns_, 0.0, annualization_);
    } else if (key == "Calmar ratio") {
        value = calmar_ratio(returns_, 0.0, annualization_);
    } else if (key == "Max drawdown") {
        value = max_drawdown(returns_);
    } else if (key == "Historical value at risk") {
        value = historical_value_at_risk(returns_);
    } else if (key == "Conditional value at risk") {
        value = conditional_value_at_risk(returns_);
    } else if (key == "Alpha" && benchmark_) {
        value = alpha(returns_, *benchmark_, 0.0, annualization_);
    } else if (key == "Beta" && benchmark_) {
        value = beta(returns_, *benchmark_);
    // One message covers both a key that names nothing and a metric this context
    // cannot serve, since alpha and beta are unreachable without a benchmark;
    // summary() leaves those two out of the result for the same reason rather
    // than reporting them as unmeasurable numbers.
    } else {
        throw input_error{"unknown or unavailable analysis summary metric"};
    }

    scalar_cache_.emplace(key_string, value);
    ++computation_counts_[key_string];
    return value;
}

std::map<std::string, double> analysis_context::summary() const {
    static constexpr std::string_view core_keys[]{
        "Annual return", "Cumulative returns", "Annual volatility", "Sharpe ratio",
        "Sortino ratio", "Calmar ratio", "Max drawdown", "Historical value at risk",
        "Conditional value at risk"};

    std::map<std::string, double> result;
    for (const auto key : core_keys) {
        result.emplace(std::string{key}, cached(key));
    }
    if (benchmark_) {
        result.emplace("Alpha", cached("Alpha"));
        result.emplace("Beta", cached("Beta"));
    }
    return result;
}

std::size_t analysis_context::computation_count(std::string_view key) const {
    const auto count = computation_counts_.find(std::string{key});
    return count == computation_counts_.end() ? 0U : count->second;
}

}  // namespace fincore
