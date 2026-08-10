#include <fincore/metrics/yearly.hpp>

#include <fincore/calendar.hpp>
#include <fincore/metrics/drawdown.hpp>
#include <fincore/metrics/ratios.hpp>

#include <map>
#include <string_view>
#include <utility>
#include <vector>

namespace fincore {
namespace {

struct yearly_bucket {
    std::vector<timestamp> index;
    std::vector<double> values;
};

struct paired_yearly_bucket {
    std::vector<timestamp> index;
    std::vector<double> returns;
    std::vector<double> benchmark;
};

std::map<int, yearly_bucket> group_by_utc_year(const dataframe& frame,
                                               std::string_view column) {
    const auto values = require_double_column(frame, column);
    const auto& index = frame.get_index();
    std::map<int, yearly_bucket> buckets;
    for (std::size_t position = 0; position < index.size(); ++position) {
        auto& bucket = buckets[utc_year(index[position])];
        bucket.index.push_back(index[position]);
        bucket.values.push_back(values[position]);
    }
    return buckets;
}

std::map<int, paired_yearly_bucket> group_aligned_by_utc_year(
    const dataframe& returns,
    const dataframe& benchmark,
    std::string_view returns_column_name,
    std::string_view benchmark_column_name) {
    const auto return_values =
        require_double_column(returns, returns_column_name);
    const auto benchmark_values =
        require_double_column(benchmark, benchmark_column_name);
    const auto& index = returns.get_index();
    std::map<int, paired_yearly_bucket> buckets;
    for (std::size_t position = 0; position < index.size(); ++position) {
        auto& bucket = buckets[utc_year(index[position])];
        bucket.index.push_back(index[position]);
        bucket.returns.push_back(return_values[position]);
        bucket.benchmark.push_back(benchmark_values[position]);
    }
    return buckets;
}

template <typename Metric>
dataframe aggregate_by_year(const dataframe& returns,
                            std::string_view input_column,
                            std::string_view output_column,
                            Metric metric) {
    auto buckets = group_by_utc_year(returns, input_column);
    std::vector<timestamp> result_index;
    std::vector<double> result_values;
    result_index.reserve(buckets.size());
    result_values.reserve(buckets.size());
    for (auto& [year, bucket] : buckets) {
        result_index.push_back(utc_timestamp(year));
        const auto group = make_series(std::move(bucket.index), input_column,
                                       std::move(bucket.values));
        result_values.push_back(metric(group));
    }
    return make_series(std::move(result_index), output_column,
                       std::move(result_values));
}

template <typename Metric>
dataframe aggregate_aligned_by_year(
    const dataframe& aligned_returns,
    const dataframe& aligned_benchmark,
    std::string_view returns_column_name,
    std::string_view benchmark_column_name,
    std::string_view output_column,
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
    return make_series(std::move(result_index), output_column,
                       std::move(result_values));
}

double annual_active_return_from_aligned(
    const dataframe& aligned_returns,
    const dataframe& aligned_benchmark,
    double annualization,
    std::string_view returns_column_name,
    std::string_view benchmark_column_name) {
    return annual_return(aligned_returns, annualization, returns_column_name) -
           annual_return(aligned_benchmark, annualization,
                         benchmark_column_name);
}

}  // namespace

dataframe annual_return_by_year(const dataframe& returns,
                                frequency period,
                                std::optional<double> annualization,
                                std::string_view column) {
    const double factor = annualization_factor(period, annualization);
    return aggregate_by_year(
        returns, column, "annual_return",
        [factor, column](const dataframe& group) {
            return annual_return(group, factor, column);
        });
}

dataframe annual_volatility_by_year(const dataframe& returns,
                                    frequency period,
                                    std::optional<double> annualization,
                                    std::string_view column) {
    const double factor = annualization_factor(period, annualization);
    return aggregate_by_year(
        returns, column, "annual_volatility",
        [factor, column](const dataframe& group) {
            return annual_volatility(group, factor, column);
        });
}

dataframe sharpe_ratio_by_year(const dataframe& returns,
                               double risk_free,
                               frequency period,
                               std::optional<double> annualization,
                               std::string_view column) {
    const double factor = annualization_factor(period, annualization);
    return aggregate_by_year(
        returns, column, "sharpe_ratio",
        [risk_free, factor, column](const dataframe& group) {
            return sharpe_ratio(group, risk_free, factor, column);
        });
}

dataframe max_drawdown_by_year(const dataframe& returns,
                               std::string_view column) {
    return aggregate_by_year(
        returns, column, "max_drawdown",
        [column](const dataframe& group) {
            return max_drawdown(group, column);
        });
}

double annual_active_return(const dataframe& returns,
                            const dataframe& benchmark,
                            frequency period,
                            std::optional<double> annualization,
                            std::string_view returns_column_name,
                            std::string_view benchmark_column_name) {
    const double factor = annualization_factor(period, annualization);
    const auto [aligned_returns, aligned_benchmark] = align_on_common_index(
        returns, returns_column_name, benchmark, benchmark_column_name);
    return annual_active_return_from_aligned(
        aligned_returns, aligned_benchmark, factor, returns_column_name,
        benchmark_column_name);
}

dataframe annual_active_return_by_year(
    const dataframe& returns,
    const dataframe& benchmark,
    frequency period,
    std::optional<double> annualization,
    std::string_view returns_column_name,
    std::string_view benchmark_column_name) {
    const double factor = annualization_factor(period, annualization);
    const auto [aligned_returns, aligned_benchmark] = align_on_common_index(
        returns, returns_column_name, benchmark, benchmark_column_name);
    return aggregate_aligned_by_year(
        aligned_returns, aligned_benchmark, returns_column_name,
        benchmark_column_name, "annual_active_return",
        [factor, returns_column_name,
         benchmark_column_name](const dataframe& group_returns,
                                const dataframe& group_benchmark) {
            return annual_active_return_from_aligned(
                group_returns, group_benchmark, factor, returns_column_name,
                benchmark_column_name);
        });
}

dataframe information_ratio_by_year(
    const dataframe& returns,
    const dataframe& benchmark,
    frequency period,
    std::optional<double> annualization,
    std::string_view returns_column_name,
    std::string_view benchmark_column_name) {
    const double factor = annualization_factor(period, annualization);
    const auto [aligned_returns, aligned_benchmark] = align_on_common_index(
        returns, returns_column_name, benchmark, benchmark_column_name);
    return aggregate_aligned_by_year(
        aligned_returns, aligned_benchmark, returns_column_name,
        benchmark_column_name, "information_ratio",
        [factor, returns_column_name,
         benchmark_column_name](const dataframe& group_returns,
                                const dataframe& group_benchmark) {
            return information_ratio(group_returns, group_benchmark, factor,
                                     returns_column_name,
                                     benchmark_column_name);
        });
}

}  // namespace fincore
