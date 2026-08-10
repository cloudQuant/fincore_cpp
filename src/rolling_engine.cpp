#include <fincore/rolling_engine.hpp>

#include <fincore/metrics/alpha_beta.hpp>
#include <fincore/metrics/drawdown.hpp>
#include <fincore/metrics/ratios.hpp>
#include <fincore/metrics/returns.hpp>

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace fincore {
namespace {

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

dataframe slice_frame(const dataframe& frame, std::string_view column,
                      std::size_t begin, std::size_t window) {
    const auto values = require_double_column(frame, column);
    const auto& index = frame.get_index();
    std::vector<timestamp> sliced_index(index.begin() + static_cast<std::ptrdiff_t>(begin),
                                        index.begin() + static_cast<std::ptrdiff_t>(begin + window));
    std::vector<double> sliced_values(values.begin() + static_cast<std::ptrdiff_t>(begin),
                                      values.begin() + static_cast<std::ptrdiff_t>(begin + window));
    return make_series(std::move(sliced_index), column, std::move(sliced_values));
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

        const auto returns_values = require_double_column(*metric_returns, returns_column);
        const auto& returns_index = metric_returns->get_index();
        const std::size_t complete_windows = returns_values.size() < window_
                                                 ? 0U
                                                 : returns_values.size() - window_ + 1U;

        std::vector<timestamp> output_index;
        std::vector<double> output_values;
        output_index.reserve(complete_windows);
        output_values.reserve(complete_windows);
        for (std::size_t begin = 0; begin < complete_windows; ++begin) {
            const auto returns_window =
                slice_frame(*metric_returns, returns_column, begin, window_);
            double value = 0.0;
            switch (metric) {
                case rolling_metric::mean_return: {
                    const auto values = require_double_column(returns_window, returns_column);
                    double total = 0.0;
                    for (const double entry : values) {
                        total += entry;
                    }
                    value = total / static_cast<double>(window_) * annualization_;
                    break;
                }
                case rolling_metric::volatility:
                    value = annual_volatility(returns_window, annualization_);
                    break;
                case rolling_metric::sharpe:
                    value = sharpe_ratio(returns_window, 0.0, annualization_);
                    break;
                case rolling_metric::max_drawdown:
                    value = max_drawdown(returns_window);
                    break;
                case rolling_metric::beta:
                    value = beta(returns_window,
                                 slice_frame(*metric_benchmark, benchmark_column, begin, window_));
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
