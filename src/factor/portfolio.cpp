#include <fincore/factor/portfolio.hpp>

#include <fincore/detail/factor_compute.hpp>
#include <fincore/factor/exceptions.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fincore {
namespace {

constexpr timestamp day_ns = 86'400'000'000'000LL;
constexpr std::string_view cash_asset = "cash";

prepared_factor_data filter_rows(const prepared_factor_data& data,
                                 const std::vector<int>& quantiles,
                                 const std::vector<std::string>& groups,
                                 const std::string_view period_label) {
    bool has_period = false;
    for (const auto& label : data.forward_labels) {
        has_period = has_period || label == period_label;
    }
    if (!has_period) {
        throw factor_data_error{
            "Period '" + std::string(period_label) + "' not found"};
    }
    prepared_factor_data filtered;
    filtered.forward_labels = data.forward_labels;
    for (const auto& row : data.rows) {
        if (!quantiles.empty() &&
            std::find(quantiles.begin(), quantiles.end(), row.quantile) ==
                quantiles.end()) {
            continue;
        }
        if (!groups.empty() &&
            (!row.has_group ||
             std::find(groups.begin(), groups.end(), row.group) ==
                 groups.end())) {
            continue;
        }
        filtered.rows.push_back(row);
    }
    if (filtered.rows.empty()) {
        throw factor_data_error{"factor portfolio selection is empty"};
    }
    return filtered;
}

int period_days(const std::string_view label) {
    int total = 0;
    std::size_t index = 0;
    while (index < label.size()) {
        const std::size_t begin = index;
        int component = 0;
        while (index < label.size() && label[index] >= '0' &&
               label[index] <= '9') {
            const int digit = label[index] - '0';
            if (component > (std::numeric_limits<int>::max() - digit) / 10) {
                throw factor_data_error{
                    "period label must fit in a positive day count"};
            }
            component = component * 10 + digit;
            ++index;
        }
        if (index == begin || index == label.size() || label[index] != 'D') {
            throw factor_data_error{
                "period label must contain only positive day components"};
        }
        ++index;
        if (component <= 0) {
            throw factor_data_error{
                "period label must contain only positive day components"};
        }
        if (total > std::numeric_limits<int>::max() - component) {
            throw factor_data_error{
                "period label must fit in a positive day count"};
        }
        total += component;
    }
    if (total <= 0) {
        throw factor_data_error{
            "period label must contain only positive day components"};
    }
    return total;
}

std::size_t period_column(const prepared_factor_data& data,
                          const std::string_view label) {
    for (std::size_t index = 0; index < data.forward_labels.size();
         ++index) {
        if (data.forward_labels[index] == label) {
            return index;
        }
    }
    throw factor_data_error{"period label is absent"};
}

timestamp normalize_day(const timestamp value) {
    timestamp day = value / day_ns;
    if (value % day_ns < 0) {
        --day;
    }
    return day * day_ns;
}

bool business_day(const timestamp day) {
    timestamp weekday = (day / day_ns + 4) % 7;
    if (weekday < 0) {
        weekday += 7;
    }
    return weekday != 0 && weekday != 6;
}

// Mirrors pandas' BusinessDay whole-day addition: an off-session start rolls
// forward first, then the remaining business days are counted.
timestamp add_business_days(const timestamp start, const int days) {
    const timestamp normalized = normalize_day(start);
    timestamp day = normalized;
    int remaining = days;
    if (!business_day(day)) {
        while (!business_day(day)) {
            day += day_ns;
        }
        remaining = days > 0 ? days - 1 : 0;
    }
    while (remaining > 0) {
        day += day_ns;
        if (business_day(day)) {
            --remaining;
        }
    }
    return day + (start - normalized);
}

factor_series_result cumulative_from_table(const factor_return_table& table,
                                           const std::size_t label) {
    factor_series_result result;
    result.dates = table.dates;
    result.values.assign(table.dates.size(), 1.0);
    double growth = 1.0;
    for (std::size_t date = 0; date < table.dates.size(); ++date) {
        const double value =
            table.values[date * table.labels.size() + label];
        growth *= 1.0 + (std::isfinite(value) ? value : 0.0);
        result.values[date] = growth;
    }
    return result;
}

// pandas resample("1D").last().ffill(): every calendar day carries the last
// observed cumulative value, and the first daily return is zero-filled.
factor_series_result daily_returns_from_cumulative(
    const factor_series_result& cumulative) {
    factor_series_result result;
    if (cumulative.dates.empty()) {
        return result;
    }
    std::map<timestamp, double> last_by_day;
    for (std::size_t index = 0; index < cumulative.dates.size(); ++index) {
        last_by_day[normalize_day(cumulative.dates[index])] =
            cumulative.values[index];
    }
    const timestamp first_day = last_by_day.begin()->first;
    const timestamp last_day = last_by_day.rbegin()->first;
    double previous = 0.0;
    bool first = true;
    for (timestamp day = first_day; day <= last_day; day += day_ns) {
        const auto entry = last_by_day.find(day);
        const double value = entry != last_by_day.end() ? entry->second
                                                        : previous;
        result.dates.push_back(day);
        result.values.push_back(first ? 0.0 : value / previous - 1.0);
        previous = value;
        first = false;
    }
    return result;
}

std::vector<factor_weight_row> daily_positions(
    const std::vector<factor_weight_row>& raw) {
    std::vector<factor_weight_row> result;
    if (raw.empty()) {
        return result;
    }
    std::set<std::string> assets;
    std::map<timestamp, std::map<std::string, double>> totals_by_day;
    timestamp first_day = normalize_day(raw.front().date);
    timestamp last_day = first_day;
    for (const auto& row : raw) {
        const timestamp day = normalize_day(row.date);
        first_day = std::min(first_day, day);
        last_day = std::max(last_day, day);
        assets.insert(row.asset);
        totals_by_day[day][row.asset] += row.weight;
    }
    std::map<std::string, double> carried;
    for (timestamp day = first_day; day <= last_day; day += day_ns) {
        const auto entry = totals_by_day.find(day);
        if (entry != totals_by_day.end()) {
            carried = entry->second;
        }
        double gross = 0.0;
        for (const auto& asset : assets) {
            gross += std::abs(carried[asset]);
        }
        double asset_total = 0.0;
        for (const auto& asset : assets) {
            const double weight = gross > 0.0 ? carried[asset] / gross : 0.0;
            asset_total += weight;
            result.push_back({day, asset, weight});
        }
        result.push_back({day, std::string{cash_asset}, 1.0 - asset_total});
    }
    return result;
}

}  // namespace

namespace detail {

factor_series_result factor_cumulative_returns_from_table(
    const factor_return_table& returns,
    const std::string_view period_label) {
    const auto period =
        std::find(returns.labels.begin(), returns.labels.end(), period_label);
    if (period == returns.labels.end()) {
        throw factor_data_error{"period label is absent"};
    }
    const std::size_t label =
        static_cast<std::size_t>(period - returns.labels.begin());
    return cumulative_from_table(returns, label);
}

factor_portfolio_inputs build_factor_portfolio_inputs_from_tables(
    const prepared_factor_data& filtered_factor_data,
    const prepared_factor_data& benchmark_source_data,
    const std::string_view period_label, const bool long_short,
    const bool group_neutral, const bool equal_weight,
    const factor_return_table& returns,
    const std::span<const factor_weight_row> weights) {
    if (weights.size() != filtered_factor_data.rows.size()) {
        throw factor_data_error{
            "factor weights must match the prepared row count"};
    }
    factor_portfolio_inputs inputs;
    const std::size_t label = period_column(filtered_factor_data, period_label);
    inputs.returns = daily_returns_from_cumulative(
        cumulative_from_table(returns, label));

    std::vector<factor_weight_row> owned_weights(weights.begin(),
                                                 weights.end());
    inputs.positions = daily_positions(factor_positions(
        filtered_factor_data, period_label, long_short, group_neutral,
        equal_weight, {}, {}, std::move(owned_weights)));

    const auto benchmark_label =
        std::find(benchmark_source_data.forward_labels.begin(),
                  benchmark_source_data.forward_labels.end(), period_label);
    if (benchmark_label != benchmark_source_data.forward_labels.end()) {
        prepared_factor_data benchmark_data = benchmark_source_data;
        for (auto& row : benchmark_data.rows) {
            row.factor = std::abs(row.factor);
        }
        const auto benchmark_weights =
            factor_weights(benchmark_data, false, false, true);
        const auto benchmark_table = factor_returns_from_weights(
            benchmark_data,
            std::span<const factor_weight_row>{benchmark_weights});
        const std::size_t benchmark_column = period_column(
            benchmark_data, period_label);
        inputs.benchmark_returns = daily_returns_from_cumulative(
            cumulative_from_table(benchmark_table, benchmark_column));
    }
    return inputs;
}

factor_portfolio_inputs build_factor_portfolio_inputs_from_outputs(
    const prepared_factor_data& filtered_factor_data,
    const prepared_factor_data& benchmark_source_data,
    const std::string_view period_label,
    const factor_return_table& returns,
    const std::span<const factor_weight_row> positions) {
    note_portfolio_input_build();
    factor_portfolio_inputs inputs;
    const std::size_t label = period_column(filtered_factor_data, period_label);
    inputs.returns = daily_returns_from_cumulative(
        cumulative_from_table(returns, label));
    inputs.positions = daily_positions(
        std::vector<factor_weight_row>(positions.begin(), positions.end()));

    const auto benchmark_label =
        std::find(benchmark_source_data.forward_labels.begin(),
                  benchmark_source_data.forward_labels.end(), period_label);
    if (benchmark_label != benchmark_source_data.forward_labels.end()) {
        prepared_factor_data benchmark_data = benchmark_source_data;
        for (auto& row : benchmark_data.rows) {
            row.factor = std::abs(row.factor);
        }
        const auto benchmark_weights =
            factor_weights(benchmark_data, false, false, true);
        const auto benchmark_table = factor_returns_from_weights(
            benchmark_data,
            std::span<const factor_weight_row>{benchmark_weights});
        const std::size_t benchmark_column = period_column(
            benchmark_data, period_label);
        inputs.benchmark_returns = daily_returns_from_cumulative(
            cumulative_from_table(benchmark_table, benchmark_column));
    }
    return inputs;
}

}  // namespace detail

std::vector<factor_weight_row> factor_positions(
    const prepared_factor_data& factor_data,
    const std::string_view period_label, const bool long_short,
    const bool group_neutral, const bool equal_weight,
    const std::vector<int>& quantiles,
    const std::vector<std::string>& groups,
    std::optional<std::vector<factor_weight_row>> weights) {
    auto filtered = filter_rows(factor_data, quantiles, groups, period_label);
    if (!weights.has_value()) {
        weights = factor_weights(filtered, long_short, group_neutral,
                                 equal_weight);
    }
    const auto days = period_days(period_label);
    detail::note_position_build();
    std::map<timestamp, std::vector<factor_weight_row>> trades;
    std::set<std::string> asset_set;
    for (const auto& row : *weights) {
        trades[row.date].push_back(row);
        asset_set.insert(row.asset);
    }
    const std::vector<std::string> assets(asset_set.begin(),
                                          asset_set.end());
    std::unordered_map<std::string, std::size_t> asset_index;
    asset_index.reserve(assets.size());
    for (std::size_t index = 0; index < assets.size(); ++index) {
        asset_index.emplace(assets[index], index);
    }
    struct active_trade {
        timestamp expires;
        std::vector<factor_weight_row> weights;
    };
    std::vector<active_trade> active;
    std::vector<timestamp> all_dates;
    all_dates.reserve(trades.size() * 2U);
    for (const auto& entry : trades) {
        all_dates.push_back(entry.first);
        all_dates.push_back(add_business_days(entry.first, days));
    }
    std::sort(all_dates.begin(), all_dates.end());
    all_dates.erase(std::unique(all_dates.begin(), all_dates.end()),
                    all_dates.end());

    std::vector<factor_weight_row> positions;
    if (!assets.empty() &&
        all_dates.size() <=
            std::numeric_limits<std::size_t>::max() / assets.size()) {
        positions.reserve(all_dates.size() * assets.size());
    }
    std::vector<double> totals(assets.size());
    for (const timestamp date : all_dates) {
        const auto trade = trades.find(date);
        if (trade != trades.end()) {
            active.push_back(
                {add_business_days(date, days), trade->second});
        }
        active.erase(std::remove_if(active.begin(), active.end(),
                                    [date](const active_trade& entry) {
                                        return entry.expires <= date;
                                    }),
                     active.end());
        std::fill(totals.begin(), totals.end(), 0.0);
        for (const auto& entry : active) {
            for (const auto& row : entry.weights) {
                totals[asset_index.at(row.asset)] += row.weight;
            }
        }
        double gross = 0.0;
        for (const double value : totals) {
            gross += std::abs(value);
        }
        for (std::size_t index = 0; index < assets.size(); ++index) {
            const double value =
                gross > 0.0 ? totals[index] / gross : 0.0;
            positions.push_back({date, assets[index], value});
        }
    }
    return positions;
}

factor_series_result factor_cumulative_returns(
    const prepared_factor_data& factor_data,
    const std::string_view period_label, const bool long_short,
    const bool group_neutral, const bool equal_weight,
    const std::vector<int>& quantiles,
    const std::vector<std::string>& groups) {
    auto filtered = filter_rows(factor_data, quantiles, groups, period_label);
    const auto table = factor_returns(filtered, long_short, group_neutral,
                                      equal_weight);
    return detail::factor_cumulative_returns_from_table(table, period_label);
}

factor_portfolio_inputs build_factor_portfolio_inputs(
    const prepared_factor_data& factor_data,
    const std::string_view period_label, const bool long_short,
    const bool group_neutral, const bool equal_weight,
    const std::vector<int>& quantiles,
    const std::vector<std::string>& groups) {
    auto filtered = filter_rows(factor_data, quantiles, groups, period_label);
    const auto weights = factor_weights(filtered, long_short, group_neutral,
                                        equal_weight);
    const auto table = detail::factor_returns_from_weights(
        filtered, std::span<const factor_weight_row>{weights});
    return detail::build_factor_portfolio_inputs_from_tables(
        filtered, factor_data, period_label, long_short, group_neutral,
        equal_weight, table, std::span<const factor_weight_row>{weights});
}

}  // namespace fincore
