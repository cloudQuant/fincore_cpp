#pragma once

#include <fincore/dataframe.hpp>

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace fincore {

struct factor_series {
    std::vector<timestamp> dates;
    std::vector<std::string> assets;
    std::vector<double> values;
};

struct forward_return_table {
    std::vector<std::string> labels;
    std::vector<timestamp> dates;
    std::vector<std::string> assets;
    std::vector<std::vector<double>> values;  // labels x rows, NaN missing
};

struct factor_loss_report {
    std::size_t input_count = 0U;
    std::size_t finite_factor_count = 0U;
    std::size_t forward_returns_count = 0U;
    std::size_t binning_count = 0U;
    double factor_input_loss = 0.0;
    double forward_returns_loss = 0.0;
    double binning_loss = 0.0;
    double total_loss = 0.0;
};

struct factor_grouping {
    std::map<std::string, std::string> group_by_asset;
    std::map<std::string, std::string> group_labels;
};

struct quantization_config {
    int quantiles = 5;
    bool use_bins = false;
    std::vector<double> bins;
    bool by_group = false;
    bool no_raise = false;
    bool zero_aware = false;
};

struct prepare_factor_config {
    int quantiles = 5;
    bool use_bins = false;
    std::vector<double> bins;
    std::vector<int> periods{1, 5, 10};
    double max_loss = 0.35;
    bool binning_by_group = false;
    std::optional<double> filter_zscore;
    bool zero_aware = false;
    bool cumulative_returns = true;
    factor_grouping grouping;
};

struct factor_row {
    timestamp date;
    std::string asset;
    double factor;
    int quantile;
    bool has_group;
    std::string group;
    std::vector<double> forward_returns;
};

struct prepared_factor_data {
    std::vector<factor_row> rows;
    std::vector<std::string> forward_labels;
    factor_loss_report loss_report;
};

struct multi_horizon_prepared_factor_data {
    std::vector<std::pair<std::string, prepared_factor_data>> by_horizon;
};

forward_return_table compute_forward_returns(
    const factor_series& factor, const dataframe& prices,
    const std::vector<int>& periods = {1, 5, 10},
    std::optional<double> filter_zscore = std::nullopt,
    bool cumulative_returns = true);

std::vector<std::optional<int>> quantize_factor(
    const factor_series& factor,
    const quantization_config& config = {},
    const factor_grouping& grouping = {});

prepared_factor_data prepare_factor_data(
    const factor_series& factor, const dataframe& prices,
    const prepare_factor_config& config = {});

prepared_factor_data prepare_factor_data_from_forward_returns(
    const factor_series& factor, const forward_return_table& forward_returns,
    const prepare_factor_config& config = {});

multi_horizon_prepared_factor_data prepare_factor_data_by_horizon(
    const factor_series& factor, const dataframe& prices,
    const prepare_factor_config& config = {});

}  // namespace fincore
