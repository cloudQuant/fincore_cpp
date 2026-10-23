#include <fincore/factor/data.hpp>

#include <fincore/detail/numeric.hpp>
#include <fincore/factor/exceptions.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace fincore {
namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();

using key = std::pair<timestamp, std::string>;

void validate_factor_series(const factor_series& factor) {
    if (factor.dates.size() != factor.assets.size() ||
        factor.dates.size() != factor.values.size()) {
        throw factor_data_error{
            "factor dates, assets and values must have equal lengths"};
    }
    std::set<key> seen;
    for (std::size_t index = 0; index < factor.dates.size(); ++index) {
        if (!seen.emplace(factor.dates[index], factor.assets[index]).second) {
            throw factor_data_error{"factor index must be unique"};
        }
    }
}

struct price_table {
    std::vector<timestamp> dates;
    std::map<std::string, std::vector<double>> columns;
};

price_table load_prices(const dataframe& prices,
                        const std::set<std::string>& assets) {
    const auto& raw_index = prices.get_index();
    std::vector<std::size_t> order(raw_index.size());
    std::iota(order.begin(), order.end(), 0U);
    std::stable_sort(order.begin(), order.end(),
                     [&raw_index](const std::size_t left,
                                  const std::size_t right) {
                         return raw_index[left] < raw_index[right];
                     });
    price_table table;
    table.dates.reserve(raw_index.size());
    for (const std::size_t position : order) {
        if (!table.dates.empty() &&
            table.dates.back() == raw_index[position]) {
            throw factor_data_error{"prices index must be unique"};
        }
        table.dates.push_back(raw_index[position]);
    }
    for (const auto& info : prices.get_columns_info<double>()) {
        const std::string name{std::get<0>(info).c_str()};
        const auto& values = prices.get_column<double>(name.c_str());
        std::vector<double> sorted;
        sorted.reserve(order.size());
        for (const std::size_t position : order) {
            sorted.push_back(values[position]);
        }
        table.columns.emplace(name, std::move(sorted));
    }
    for (const auto& asset : assets) {
        if (table.columns.find(asset) == table.columns.end()) {
            throw factor_data_error{
                "prices do not contain factor asset '" + asset + "'"};
        }
    }
    return table;
}

std::size_t price_position(const price_table& prices,
                           const timestamp date) {
    const auto found = std::lower_bound(prices.dates.begin(),
                                        prices.dates.end(), date);
    if (found == prices.dates.end() || *found != date) {
        return prices.dates.size();
    }
    return static_cast<std::size_t>(found - prices.dates.begin());
}

std::vector<double> sorted_finite(std::vector<double> values) {
    values.erase(std::remove_if(values.begin(), values.end(),
                                [](const double value) {
                                    return !std::isfinite(value);
                                }),
                 values.end());
    std::sort(values.begin(), values.end());
    return values;
}

std::vector<double> quantile_edges(std::vector<double> values,
                                   const int quantiles) {
    if (quantiles < 1) {
        throw factor_data_error{"quantiles must be at least one"};
    }
    auto sorted = sorted_finite(std::move(values));
    if (sorted.empty()) {
        throw factor_data_error{"cannot quantize an empty group"};
    }
    std::vector<double> edges;
    edges.reserve(static_cast<std::size_t>(quantiles) + 1U);
    for (int index = 0; index <= quantiles; ++index) {
        edges.push_back(detail::linear_percentile(
            sorted, static_cast<double>(index) /
                        static_cast<double>(quantiles)));
    }
    for (std::size_t index = 1; index < edges.size(); ++index) {
        if (!(edges[index] > edges[index - 1U])) {
            throw factor_data_error{"Bin edges must be unique"};
        }
    }
    return edges;
}

std::vector<double> cut_edges(std::vector<double> values,
                              const int bins) {
    if (bins < 1) {
        throw factor_data_error{"bins must be at least one"};
    }
    auto sorted = sorted_finite(std::move(values));
    if (sorted.empty()) {
        throw factor_data_error{"cannot cut an empty group"};
    }
    const double minimum = sorted.front();
    const double maximum = sorted.back();
    std::vector<double> edges(static_cast<std::size_t>(bins) + 1U, 0.0);
    for (int index = 0; index <= bins; ++index) {
        edges[static_cast<std::size_t>(index)] =
            minimum + (maximum - minimum) *
                          static_cast<double>(index) /
                          static_cast<double>(bins);
    }
    const double adjustment = (maximum - minimum) * 0.001;
    if (adjustment > 0.0) {
        edges.front() -= adjustment;
        edges.back() += adjustment;
    }
    for (std::size_t index = 1; index < edges.size(); ++index) {
        if (!(edges[index] > edges[index - 1U])) {
            throw factor_data_error{"Bin edges must be unique"};
        }
    }
    return edges;
}

int assign_bin(const double value, const std::vector<double>& edges) {
    if (value <= edges.front()) {
        return 0;
    }
    const auto upper = std::lower_bound(edges.begin() + 1, edges.end(),
                                        value);
    if (upper == edges.end()) {
        return static_cast<int>(edges.size()) - 2;
    }
    return std::max(0, static_cast<int>(upper - edges.begin()) - 1);
}

std::vector<std::optional<int>> quantize_bucket(
    const std::vector<std::size_t>& rows, const factor_series& factor,
    const quantization_config& config) {
    std::vector<std::optional<int>> labels(rows.size(), std::nullopt);
    const int bucket_count =
        config.use_bins
            ? (config.bins.empty()
                   ? config.quantiles
                   : static_cast<int>(config.bins.size()) - 1)
            : config.quantiles;
    try {
        if (config.zero_aware) {
            const int split = bucket_count / 2;
            if (split < 1) {
                throw factor_data_error{
                    "zero_aware requires at least two quantiles or bins"};
            }
            std::vector<std::size_t> negative;
            std::vector<std::size_t> positive;
            for (std::size_t position = 0; position < rows.size();
                 ++position) {
                if (factor.values[rows[position]] >= 0.0) {
                    positive.push_back(position);
                } else {
                    negative.push_back(position);
                }
            }
            if (negative.empty() || positive.empty()) {
                throw factor_data_error{"Bin edges must be unique"};
            }
            const auto edges_for = [&](const std::vector<std::size_t>& subset) {
                std::vector<double> values;
                values.reserve(subset.size());
                for (const std::size_t position : subset) {
                    values.push_back(factor.values[rows[position]]);
                }
                if (config.use_bins && !config.bins.empty()) {
                    return config.bins;
                }
                return config.use_bins
                           ? cut_edges(std::move(values), split)
                           : quantile_edges(std::move(values), split);
            };
            const auto negative_edges = edges_for(negative);
            const auto positive_edges = edges_for(positive);
            for (const std::size_t position : negative) {
                labels[position] =
                    assign_bin(factor.values[rows[position]],
                               negative_edges) +
                    1;
            }
            for (const std::size_t position : positive) {
                labels[position] =
                    assign_bin(factor.values[rows[position]],
                               positive_edges) +
                    1 + split;
            }
            return labels;
        }
        std::vector<double> values;
        values.reserve(rows.size());
        for (const std::size_t row : rows) {
            values.push_back(factor.values[row]);
        }
        std::vector<double> edges;
        if (config.use_bins && !config.bins.empty()) {
            edges = config.bins;
            for (std::size_t index = 1; index < edges.size(); ++index) {
                if (!(edges[index] > edges[index - 1U])) {
                    throw factor_data_error{
                        "bins must increase monotonically"};
                }
            }
        } else if (config.use_bins) {
            edges = cut_edges(std::move(values), bucket_count);
        } else {
            edges = quantile_edges(std::move(values), bucket_count);
        }
        for (std::size_t position = 0; position < rows.size();
             ++position) {
            labels[position] =
                assign_bin(factor.values[rows[position]], edges) + 1;
        }
        return labels;
    } catch (const factor_data_error&) {
        if (!config.no_raise) {
            throw;
        }
        std::fill(labels.begin(), labels.end(), std::nullopt);
        return labels;
    }
}

std::vector<std::optional<int>> quantize_series(
    const factor_series& factor,
    const std::vector<std::optional<std::string>>& groups,
    const quantization_config& config,
    const std::vector<bool>* include) {
    std::vector<std::optional<int>> labels(factor.dates.size(),
                                          std::nullopt);
    std::map<std::pair<timestamp, std::string>, std::vector<std::size_t>>
        buckets;
    for (std::size_t row = 0; row < factor.dates.size(); ++row) {
        if (include != nullptr && !(*include)[row]) {
            continue;
        }
        if (!std::isfinite(factor.values[row])) {
            continue;
        }
        std::string group;
        if (config.by_group) {
            if (!groups[row].has_value()) {
                throw factor_data_error{
                    "factor_data must contain a group when by_group=True"};
            }
            group = *groups[row];
        }
        buckets[{factor.dates[row], group}].push_back(row);
    }
    for (const auto& entry : buckets) {
        const auto bucket_labels =
            quantize_bucket(entry.second, factor, config);
        for (std::size_t position = 0; position < entry.second.size();
             ++position) {
            labels[entry.second[position]] = bucket_labels[position];
        }
    }
    return labels;
}

std::vector<std::optional<std::string>> resolve_groups(
    const factor_series& factor, const factor_grouping& grouping) {
    std::vector<std::optional<std::string>> groups(factor.dates.size(),
                                                   std::nullopt);
    if (grouping.group_by_asset.empty()) {
        return groups;
    }
    for (std::size_t row = 0; row < factor.dates.size(); ++row) {
        if (!std::isfinite(factor.values[row])) {
            continue;
        }
        const auto found =
            grouping.group_by_asset.find(factor.assets[row]);
        if (found == grouping.group_by_asset.end()) {
            throw factor_data_error{
                "Assets ['" + factor.assets[row] +
                "'] not in group mapping"};
        }
        std::string group = found->second;
        const auto label = grouping.group_labels.find(group);
        if (label != grouping.group_labels.end()) {
            group = label->second;
        }
        groups[row] = group;
    }
    return groups;
}

factor_loss_report make_loss_report(const std::size_t input_count,
                                    const std::size_t finite_count,
                                    const std::size_t forward_count,
                                    const std::size_t binning_count) {
    if (input_count == 0U) {
        throw factor_data_error{
            "factor must contain at least one observation"};
    }
    factor_loss_report report;
    report.input_count = input_count;
    report.finite_factor_count = finite_count;
    report.forward_returns_count = forward_count;
    report.binning_count = binning_count;
    const double total = static_cast<double>(input_count);
    report.factor_input_loss =
        static_cast<double>(input_count - finite_count) / total;
    report.forward_returns_loss =
        static_cast<double>(finite_count - forward_count) / total;
    report.binning_loss =
        static_cast<double>(forward_count - binning_count) / total;
    report.total_loss = report.factor_input_loss +
                        report.forward_returns_loss +
                        report.binning_loss;
    return report;
}

void require_unique_labels(const forward_return_table& forward) {
    std::set<std::string> labels;
    for (const auto& label : forward.labels) {
        if (!labels.insert(label).second) {
            throw factor_data_error{
                "forward return labels must be unique"};
        }
    }
}

void validate_forward_table(const forward_return_table& forward,
                            const factor_series& factor) {
    if (forward.dates.size() != forward.assets.size()) {
        throw factor_data_error{
            "forward-return dates and assets must have equal lengths"};
    }
    for (const auto& values : forward.values) {
        if (values.size() != forward.dates.size()) {
            throw factor_data_error{
                "forward-return values must match the row count"};
        }
    }
    if (forward.values.size() != forward.labels.size()) {
        throw factor_data_error{
            "forward-return labels and values must have equal lengths"};
    }
    std::set<key> seen;
    for (std::size_t row = 0; row < forward.dates.size(); ++row) {
        if (!seen.emplace(forward.dates[row], forward.assets[row]).second) {
            throw factor_data_error{
                "forward-return index must be unique"};
        }
    }
    static_cast<void>(factor);
}

prepared_factor_data prepare_impl(
    const factor_series& factor, const forward_return_table& forward,
    const prepare_factor_config& config) {
    if (!(config.max_loss >= 0.0 && config.max_loss <= 1.0)) {
        throw factor_data_error{
            "max_loss must be a number between 0 and 1"};
    }
    validate_factor_series(factor);
    validate_forward_table(forward, factor);
    require_unique_labels(forward);

    std::map<key, std::size_t> forward_rows;
    for (std::size_t row = 0; row < forward.dates.size(); ++row) {
        forward_rows.emplace(key{forward.dates[row], forward.assets[row]},
                             row);
    }
    const auto groups = resolve_groups(factor, config.grouping);
    const bool has_groups = !config.grouping.group_by_asset.empty();
    std::vector<bool> finite(factor.dates.size(), false);
    std::vector<bool> merged(factor.dates.size(), false);
    std::size_t finite_count = 0U;
    std::size_t forward_count = 0U;
    for (std::size_t row = 0; row < factor.dates.size(); ++row) {
        if (!std::isfinite(factor.values[row])) {
            continue;
        }
        finite[row] = true;
        ++finite_count;
        bool available = true;
        if (has_groups && !groups[row].has_value()) {
            available = false;
        }
        for (std::size_t label = 0; available && label < forward.labels.size();
             ++label) {
            const auto found =
                forward_rows.find(key{factor.dates[row],
                                      factor.assets[row]});
            const double value =
                found == forward_rows.end()
                    ? nan
                    : forward.values[label][found->second];
            if (!std::isfinite(value)) {
                available = false;
            }
        }
        if (available) {
            merged[row] = true;
            ++forward_count;
        }
    }
    quantization_config quantize_config;
    quantize_config.quantiles = config.quantiles;
    quantize_config.use_bins = config.use_bins;
    quantize_config.bins = config.bins;
    quantize_config.by_group = config.binning_by_group;
    quantize_config.no_raise = config.max_loss != 0.0;
    quantize_config.zero_aware = config.zero_aware;
    const auto quantiles =
        quantize_series(factor, groups, quantize_config, &merged);
    prepared_factor_data prepared;
    prepared.forward_labels = forward.labels;
    for (std::size_t row = 0; row < factor.dates.size(); ++row) {
        if (!merged[row] || !quantiles[row].has_value()) {
            continue;
        }
        factor_row record;
        record.date = factor.dates[row];
        record.asset = factor.assets[row];
        record.factor = factor.values[row];
        record.quantile = *quantiles[row];
        record.has_group = groups[row].has_value();
        record.group = groups[row].value_or("");
        record.forward_returns.reserve(forward.labels.size());
        for (std::size_t label = 0; label < forward.labels.size(); ++label) {
            const auto found =
                forward_rows.find(key{factor.dates[row],
                                      factor.assets[row]});
            record.forward_returns.push_back(
                found == forward_rows.end()
                    ? nan
                    : forward.values[label][found->second]);
        }
        prepared.rows.push_back(std::move(record));
    }
    prepared.loss_report =
        make_loss_report(factor.dates.size(), finite_count, forward_count,
                         prepared.rows.size());
    if (prepared.loss_report.total_loss > config.max_loss) {
        throw factor_loss_exceeded{
            "max_loss exceeded for factor preparation",
            prepared.loss_report};
    }
    return prepared;
}

}  // namespace

forward_return_table compute_forward_returns(
    const factor_series& factor, const dataframe& prices,
    const std::vector<int>& periods,
    const std::optional<double> filter_zscore,
    const bool cumulative_returns) {
    validate_factor_series(factor);
    if (periods.empty()) {
        throw factor_data_error{"periods must contain positive integers"};
    }
    std::set<std::string> assets(factor.assets.begin(),
                                 factor.assets.end());
    const auto price_data = load_prices(prices, assets);
    std::size_t shared = 0U;
    for (const timestamp date : factor.dates) {
        if (price_position(price_data, date) < price_data.dates.size()) {
            ++shared;
        }
    }
    if (shared == 0U) {
        throw factor_data_error{
            "Factor and prices indices don't match"};
    }
    if (filter_zscore.has_value() &&
        (!std::isfinite(*filter_zscore))) {
        throw factor_data_error{
            "filter_zscore must be a finite number or None"};
    }

    forward_return_table table;
    table.dates = factor.dates;
    table.assets = factor.assets;
    table.values.assign(periods.size(),
                        std::vector<double>(factor.dates.size(), nan));
    for (std::size_t label = 0; label < periods.size(); ++label) {
        const int period = periods[label];
        if (period <= 0) {
            throw factor_data_error{"periods must contain positive integers"};
        }
        table.labels.push_back(std::to_string(period) + "D");
        for (std::size_t row = 0; row < factor.dates.size(); ++row) {
            const std::size_t position =
                price_position(price_data, factor.dates[row]);
            if (position >= price_data.dates.size()) {
                continue;
            }
            const auto column =
                price_data.columns.find(factor.assets[row]);
            if (column == price_data.columns.end()) {
                continue;
            }
            const auto& values = column->second;
            const std::size_t future =
                position + static_cast<std::size_t>(period);
            if (future >= values.size()) {
                continue;
            }
            const std::size_t base =
                cumulative_returns
                    ? position
                    : future - 1U;
            if (values[base] == 0.0) {
                table.values[label][row] = nan;
            } else {
                table.values[label][row] =
                    values[future] / values[base] - 1.0;
            }
        }
        if (filter_zscore.has_value()) {
            for (const auto& asset : assets) {
                std::vector<double> finite_values;
                for (std::size_t row = 0; row < factor.dates.size();
                     ++row) {
                    if (factor.assets[row] == asset &&
                        std::isfinite(table.values[label][row])) {
                        finite_values.push_back(table.values[label][row]);
                    }
                }
                if (finite_values.size() < 2U) {
                    continue;
                }
                const double mean =
                    std::accumulate(finite_values.begin(),
                                    finite_values.end(), 0.0) /
                    static_cast<double>(finite_values.size());
                double variance = 0.0;
                for (const double value : finite_values) {
                    variance += (value - mean) * (value - mean);
                }
                variance /= static_cast<double>(finite_values.size() - 1U);
                const double deviation = std::sqrt(variance);
                for (std::size_t row = 0; row < factor.dates.size();
                     ++row) {
                    if (factor.assets[row] != asset ||
                        !std::isfinite(table.values[label][row])) {
                        continue;
                    }
                    if (std::abs(table.values[label][row] - mean) >
                        *filter_zscore * deviation) {
                        table.values[label][row] = nan;
                    }
                }
            }
        }
    }
    return table;
}

std::vector<std::optional<int>> quantize_factor(
    const factor_series& factor, const quantization_config& config,
    const factor_grouping& grouping) {
    validate_factor_series(factor);
    if (config.use_bins) {
        if (config.quantiles < 1 && config.bins.empty()) {
            throw factor_data_error{
                "Either quantiles or bins should be provided"};
        }
    } else if (config.quantiles < 1) {
        throw factor_data_error{
            "Either quantiles or bins should be provided"};
    }
    if (config.by_group && grouping.group_by_asset.empty()) {
        throw factor_data_error{
            "factor_data must contain a 'group' column when by_group=True"};
    }
    const auto groups = resolve_groups(factor, grouping);
    return quantize_series(factor, groups, config, nullptr);
}

prepared_factor_data prepare_factor_data(
    const factor_series& factor, const dataframe& prices,
    const prepare_factor_config& config) {
    const auto forward = compute_forward_returns(
        factor, prices, config.periods, config.filter_zscore,
        config.cumulative_returns);
    return prepare_impl(factor, forward, config);
}

prepared_factor_data prepare_factor_data_from_forward_returns(
    const factor_series& factor,
    const forward_return_table& forward_returns,
    const prepare_factor_config& config) {
    return prepare_impl(factor, forward_returns, config);
}

multi_horizon_prepared_factor_data prepare_factor_data_by_horizon(
    const factor_series& factor, const dataframe& prices,
    const prepare_factor_config& config) {
    if (!(config.max_loss >= 0.0 && config.max_loss <= 1.0)) {
        throw factor_data_error{
            "max_loss must be a number between 0 and 1"};
    }
    if (config.filter_zscore.has_value()) {
        throw factor_data_error{
            "per-horizon factor preparation does not allow full-sample "
            "filter_zscore"};
    }
    validate_factor_series(factor);
    const auto forward = compute_forward_returns(
        factor, prices, config.periods, std::nullopt,
        config.cumulative_returns);
    require_unique_labels(forward);
    const auto groups = resolve_groups(factor, config.grouping);
    const bool has_groups = !config.grouping.group_by_asset.empty();
    std::vector<bool> finite(factor.dates.size(), false);
    std::size_t finite_count = 0U;
    for (std::size_t row = 0; row < factor.dates.size(); ++row) {
        if (std::isfinite(factor.values[row])) {
            finite[row] = true;
            ++finite_count;
        }
    }
    if (finite_count == 0U) {
        throw factor_data_error{
            "factor must contain at least one finite observation"};
    }
    quantization_config quantize_config;
    quantize_config.quantiles = config.quantiles;
    quantize_config.use_bins = config.use_bins;
    quantize_config.bins = config.bins;
    quantize_config.by_group = config.binning_by_group;
    quantize_config.no_raise = config.max_loss != 0.0;
    quantize_config.zero_aware = config.zero_aware;
    const auto quantiles =
        quantize_series(factor, groups, quantize_config, &finite);

    multi_horizon_prepared_factor_data result;
    for (std::size_t label = 0; label < forward.labels.size(); ++label) {
        prepared_factor_data prepared;
        prepared.forward_labels = {forward.labels[label]};
        std::size_t forward_count = 0U;
        for (std::size_t row = 0; row < factor.dates.size(); ++row) {
            if (!finite[row] || !std::isfinite(forward.values[label][row])) {
                continue;
            }
            ++forward_count;
            if (!quantiles[row].has_value()) {
                continue;
            }
            if (has_groups && !groups[row].has_value()) {
                continue;
            }
            factor_row record;
            record.date = factor.dates[row];
            record.asset = factor.assets[row];
            record.factor = factor.values[row];
            record.quantile = *quantiles[row];
            record.has_group = groups[row].has_value();
            record.group = groups[row].value_or("");
            record.forward_returns = {forward.values[label][row]};
            prepared.rows.push_back(std::move(record));
        }
        prepared.loss_report =
            make_loss_report(factor.dates.size(), finite_count,
                             forward_count, prepared.rows.size());
        if (prepared.loss_report.total_loss > config.max_loss) {
            throw factor_loss_exceeded{
                "max_loss exceeded for horizon " + forward.labels[label],
                prepared.loss_report};
        }
        result.by_horizon.emplace_back(forward.labels[label],
                                       std::move(prepared));
    }
    return result;
}

}  // namespace fincore
