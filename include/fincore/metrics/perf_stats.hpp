#pragma once

#include <fincore/metrics/alpha_beta.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fincore {

std::map<std::string, double> perf_stats(
    const dataframe& returns,
    frequency period = frequency::daily,
    std::string_view returns_column_name = returns_column);

std::map<std::string, double> perf_stats(
    const dataframe& returns,
    const dataframe& benchmark,
    frequency period = frequency::daily,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);

struct distribution_stats {
    double mean;
    double median;
    double std;
    double p5;
    double p25;
    double p75;
    double p95;
    double iqr;
};

distribution_stats calc_distribution_stats(std::span<const double> values);

struct bootstrap_metric_stats {
    std::string name;
    std::vector<double> samples;
    distribution_stats summary;
};

std::vector<bootstrap_metric_stats> perf_stats_bootstrap(
    const dataframe& returns,
    std::size_t n_samples = 1000,
    std::uint64_t seed = 0,
    std::string_view column = returns_column);

}  // namespace fincore
