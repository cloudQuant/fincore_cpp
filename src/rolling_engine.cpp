#include <fincore/rolling_engine.hpp>

#include <fincore/metrics/alpha_beta.hpp>
#include <fincore/metrics/drawdown.hpp>
#include <fincore/metrics/ratios.hpp>
#include <fincore/metrics/returns.hpp>

#include <cmath>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace fincore {
namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();
constexpr double minimum_variance = 1.0e-30;

std::string_view metric_name(rolling_metric metric) {
    switch (metric) {
        case rolling_metric::mean_return:
            return "mean_return";
        case rolling_metric::volatility:
            return "volatility";
        case rolling_metric::sharpe:
            return "sharpe";
        case rolling_metric::max_drawdown:
            return "max_drawdown";
        case rolling_metric::beta:
            return "beta";
    }
    throw input_error{"unknown rolling metric"};
}

double annual_volatility_window(const std::span<const double> values,
                                const double annualization) {
    // Match annual_volatility's NaN-only filter: infinities remain samples.
    std::size_t count = 0U;
    for (const double value : values) {
        if (!std::isnan(value)) {
            ++count;
        }
    }
    if (count < 2U) {
        return nan;
    }

    double mean = 0.0;
    for (const double value : values) {
        if (!std::isnan(value)) {
            mean += value;
        }
    }
    mean /= static_cast<double>(count);

    double squared_deviations = 0.0;
    for (const double value : values) {
        if (!std::isnan(value)) {
            const double deviation = value - mean;
            squared_deviations += deviation * deviation;
        }
    }
    const double sample_variance =
        squared_deviations / static_cast<double>(count - 1U);
    return std::sqrt(sample_variance) * std::sqrt(annualization);
}

double sharpe_window(const std::span<const double> values,
                     const double annualization) {
    // Keep the scalar's NaN-only filtering and explicit zero-risk-free shift.
    std::size_t count = 0U;
    for (const double value : values) {
        if (!std::isnan(value)) {
            ++count;
        }
    }
    if (count < 2U) {
        return nan;
    }

    double total_excess = 0.0;
    for (const double value : values) {
        if (!std::isnan(value)) {
            total_excess += value - 0.0;
        }
    }
    const double average = total_excess / static_cast<double>(count);
    double squared_deviations = 0.0;
    for (const double value : values) {
        if (!std::isnan(value)) {
            const double excess = value - 0.0;
            const double deviation = excess - average;
            squared_deviations += deviation * deviation;
        }
    }
    const double sample_standard_deviation =
        std::sqrt(squared_deviations / static_cast<double>(count - 1U));
    return average * std::sqrt(annualization) /
           sample_standard_deviation;
}

double max_drawdown_window(const std::span<const double> values) {
    double wealth = 100.0;
    double peak = wealth;
    double result = nan;
    for (const double value : values) {
        wealth *= 1.0 + (std::isnan(value) ? 0.0 : value);
        peak = std::fmax(peak, wealth);
        const double drawdown = (wealth - peak) / peak;
        if (!std::isnan(drawdown) &&
            (std::isnan(result) || drawdown < result)) {
            result = drawdown;
        }
    }
    return result;
}

// These are the aligned windows produced once by compute(). The full
// alignment stores the first right-hand value for each timestamp in every
// duplicate left row, so beta's scalar re-alignment of a window is lockstep.
double beta_window(const std::span<const double> returns,
                   const std::span<const double> benchmark) {
    std::size_t count = 0U;
    double benchmark_total = 0.0;
    for (std::size_t position = 0U; position < returns.size(); ++position) {
        if (std::isnan(returns[position]) || std::isnan(benchmark[position])) {
            continue;
        }
        benchmark_total += benchmark[position] - 0.0;
        ++count;
    }
    if (count < 2U) {
        return nan;
    }

    const double benchmark_mean =
        benchmark_total / static_cast<double>(count);
    double covariance = 0.0;
    double benchmark_variance = 0.0;
    for (std::size_t position = 0U; position < returns.size(); ++position) {
        if (std::isnan(returns[position]) || std::isnan(benchmark[position])) {
            continue;
        }
        const double return_value = returns[position] - 0.0;
        const double benchmark_value = benchmark[position] - 0.0;
        const double benchmark_residual = benchmark_value - benchmark_mean;
        covariance += benchmark_residual * return_value;
        benchmark_variance += benchmark_residual * benchmark_residual;
    }
    covariance /= static_cast<double>(count);
    benchmark_variance /= static_cast<double>(count);
    if (!(benchmark_variance >= minimum_variance)) {
        return nan;
    }
    const double result = covariance / benchmark_variance;
    return std::isfinite(result) ? result : nan;
}

}  // namespace

rolling_engine::rolling_engine(dataframe returns, std::optional<dataframe> benchmark,
                               std::size_t window, double annualization)
    : returns_{std::move(returns)},
      benchmark_{std::move(benchmark)},
      window_{window},
      annualization_{annualization} {
    if (window_ < 2U) {
        throw input_error{"rolling window must be at least two observations"};
    }
    require_double_column(returns_, returns_column);
    if (benchmark_) {
        require_double_column(*benchmark_, benchmark_column);
    }
}

std::map<std::string, dataframe> rolling_engine::compute(
    std::span<const rolling_metric> metrics) const {
    std::map<std::string, dataframe> result;

    for (const auto metric : metrics) {
        const std::string name{metric_name(metric)};
        if (metric == rolling_metric::beta && !benchmark_) {
            throw input_error{"benchmark is required to compute rolling beta"};
        }

        const dataframe* metric_returns = &returns_;
        const dataframe* metric_benchmark = nullptr;
        std::optional<std::pair<dataframe, dataframe>> aligned_frames;
        if (metric == rolling_metric::beta) {
            aligned_frames.emplace(align_on_common_index(
                returns_, returns_column, *benchmark_, benchmark_column));
            metric_returns = &aligned_frames->first;
            metric_benchmark = &aligned_frames->second;
        }

        const auto returns_values =
            require_double_column(*metric_returns, returns_column);
        const auto& returns_index = metric_returns->get_index();
        std::span<const double> benchmark_values;
        if (metric_benchmark != nullptr) {
            benchmark_values =
                require_double_column(*metric_benchmark, benchmark_column);
        }
        const std::size_t complete_windows = returns_values.size() < window_
                                                 ? 0U
                                                 : returns_values.size() - window_ + 1U;

        std::vector<timestamp> output_index;
        std::vector<double> output_values;
        output_index.reserve(complete_windows);
        output_values.reserve(complete_windows);
        for (std::size_t begin = 0; begin < complete_windows; ++begin) {
            const auto returns_window = returns_values.subspan(begin, window_);
            double value = 0.0;
            switch (metric) {
                case rolling_metric::mean_return: {
                    double total = 0.0;
                    for (const double entry : returns_window) {
                        total += entry;
                    }
                    value =
                        total / static_cast<double>(window_) * annualization_;
                    break;
                }
                case rolling_metric::volatility:
                    value = annual_volatility_window(returns_window,
                                                     annualization_);
                    break;
                case rolling_metric::sharpe:
                    value = sharpe_window(returns_window, annualization_);
                    break;
                case rolling_metric::max_drawdown:
                    value = max_drawdown_window(returns_window);
                    break;
                case rolling_metric::beta:
                    value = beta_window(
                        returns_window,
                        benchmark_values.subspan(begin, window_));
                    break;
            }
            output_index.push_back(returns_index[begin + window_ - 1U]);
            output_values.push_back(value);
        }
        result.emplace(name, make_series(std::move(output_index), returns_column,
                                         std::move(output_values)));
    }
    return result;
}

}  // namespace fincore
