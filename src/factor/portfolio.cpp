#include <fincore/factor/portfolio.hpp>

#include <fincore/factor/exceptions.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
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
            component = component * 10 + (label[index] - '0');
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
    std::map<timestamp, std::vector<factor_weight_row>> trades;
    std::set<std::string> assets;
    for (const auto& row : *weights) {
        trades[row.date].push_back(row);
        assets.insert(row.asset);
    }
    struct active_trade {
        timestamp expires;
        std::vector<factor_weight_row> weights;
    };
    std::vector<active_trade> active;
    std::set<timestamp> all_dates;
    for (const auto& entry : trades) {
        all_dates.insert(entry.first);
        all_dates.insert(add_business_days(entry.first, days));
    }
    std::vector<factor_weight_row> positions;
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
        std::map<std::string, double> totals;
        for (const auto& entry : active) {
            for (const auto& row : entry.weights) {
                totals[row.asset] += row.weight;
            }
        }
        double gross = 0.0;
        for (const auto& entry : totals) {
            gross += std::abs(entry.second);
        }
        for (const auto& asset : assets) {
            const double value =
                gross > 0.0 ? totals[asset] / gross : 0.0;
            positions.push_back({date, asset, value});
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
    return cumulative_from_table(table, period_column(filtered, period_label));
}

factor_portfolio_inputs build_factor_portfolio_inputs(
    const prepared_factor_data& factor_data,
    const std::string_view period_label, const bool long_short,
    const bool group_neutral, const bool equal_weight,
    const std::vector<int>& quantiles,
    const std::vector<std::string>& groups) {
    factor_portfolio_inputs inputs;
    auto filtered = filter_rows(factor_data, quantiles, groups, period_label);
    const auto table = factor_returns(filtered, long_short, group_neutral,
                                      equal_weight);
    const std::size_t label = period_column(filtered, period_label);
    inputs.returns = daily_returns_from_cumulative(
        cumulative_from_table(table, label));

    const auto weights = factor_weights(filtered, long_short, group_neutral,
                                        equal_weight);
    inputs.positions = daily_positions(factor_positions(
        filtered, period_label, long_short, group_neutral, equal_weight, {},
        {}, weights));

    const auto benchmark_label =
        std::find(factor_data.forward_labels.begin(),
                  factor_data.forward_labels.end(), period_label);
    if (benchmark_label != factor_data.forward_labels.end()) {
        prepared_factor_data benchmark_data = factor_data;
        for (auto& row : benchmark_data.rows) {
            row.factor = std::abs(row.factor);
        }
        const auto benchmark_table =
            factor_returns(benchmark_data, false, false, true);
        const std::size_t benchmark_column =
            period_column(benchmark_data, period_label);
        inputs.benchmark_returns = daily_returns_from_cumulative(
            cumulative_from_table(benchmark_table, benchmark_column));
    }
    return inputs;
}

}  // namespace fincore
