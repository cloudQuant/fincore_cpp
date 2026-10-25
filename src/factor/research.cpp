#include <fincore/factor/research.hpp>
#include "research_detailed_internal.hpp"

#include "research_export_internal.hpp"

#include <fincore/detail/factor_compute.hpp>
#include <fincore/detail/json.hpp>
#include <fincore/factor/exceptions.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fincore {
namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();

std::vector<double> ranks(std::vector<double> values) {
    std::vector<std::size_t> order(values.size());
    for (std::size_t index = 0; index < values.size(); ++index) {
        order[index] = index;
    }
    std::stable_sort(order.begin(), order.end(),
                     [&values](const std::size_t left,
                               const std::size_t right) {
                         return values[left] < values[right];
                     });
    std::vector<double> result(values.size(), nan);
    std::size_t position = 0U;
    while (position < values.size()) {
        std::size_t end = position + 1U;
        while (end < values.size() &&
               values[order[end]] == values[order[position]]) {
            ++end;
        }
        const double rank =
            0.5 * (static_cast<double>(position + 1U) +
                   static_cast<double>(end));
        for (std::size_t index = position; index < end; ++index) {
            result[order[index]] = rank;
        }
        position = end;
    }
    return result;
}

double mean_of(const std::vector<double>& values) {
    double total = 0.0;
    std::size_t count = 0U;
    for (const double value : values) {
        if (std::isfinite(value)) {
            total += value;
            ++count;
        }
    }
    return count == 0U ? nan : total / static_cast<double>(count);
}

double std_of(const std::vector<double>& values) {
    const double mean = mean_of(values);
    if (!std::isfinite(mean)) {
        return nan;
    }
    double total = 0.0;
    std::size_t count = 0U;
    for (const double value : values) {
        if (std::isfinite(value)) {
            total += (value - mean) * (value - mean);
            ++count;
        }
    }
    return count < 2U ? nan
                      : std::sqrt(total / static_cast<double>(count - 1U));
}

void preprocess(factor_series& factors, const std::string& method) {
    if (method == "none") {
        return;
    }
    if (method != "cross_section_rank" &&
        method != "cross_section_zscore") {
        throw factor_data_error{
            "preprocess must be cross_section_rank, cross_section_zscore or "
            "none"};
    }
    std::map<timestamp, std::vector<std::size_t>> by_date;
    for (std::size_t row = 0; row < factors.dates.size(); ++row) {
        if (std::isfinite(factors.values[row])) {
            by_date[factors.dates[row]].push_back(row);
        }
    }
    for (const auto& entry : by_date) {
        std::vector<double> values;
        values.reserve(entry.second.size());
        for (const std::size_t row : entry.second) {
            values.push_back(factors.values[row]);
        }
        if (method == "cross_section_rank") {
            const auto ranked = ranks(values);
            for (std::size_t index = 0; index < entry.second.size(); ++index) {
                factors.values[entry.second[index]] =
                    ranked[index] / static_cast<double>(values.size());
            }
        } else {
            const double mean = mean_of(values);
            const double deviation = std_of(values);
            for (std::size_t index = 0; index < entry.second.size(); ++index) {
                factors.values[entry.second[index]] =
                    deviation > 0.0
                        ? (values[index] - mean) / deviation
                        : 0.0;
            }
        }
    }
}

std::string join_names(const std::vector<std::string>& names) {
    std::string result;
    for (std::size_t index = 0; index < names.size(); ++index) {
        if (index != 0U) {
            result += ", ";
        }
        result += names[index];
    }
    return result;
}

struct id_cell_hash {
    std::size_t operator()(
        const std::pair<timestamp, std::size_t>& key) const noexcept {
        const std::size_t left = std::hash<timestamp>{}(key.first);
        const std::size_t right = std::hash<std::size_t>{}(key.second);
        return left ^ (right + 0x9e3779b9U + (left << 6U) + (left >> 2U));
    }
};

void require_targets_present(const std::unordered_set<std::string>& observed,
                             const std::vector<std::string>& targets,
                             const std::string& message) {
    std::vector<std::string> missing;
    for (const auto& target : targets) {
        if (!observed.contains(target)) {
            missing.push_back(target);
        }
    }
    if (!missing.empty()) {
        throw factor_data_error{message + ": " + join_names(missing)};
    }
}

std::vector<std::size_t> select_panel_source_rows_impl(
    const factor_series& factors, const research_spec& spec) {
    if (!(spec.max_loss >= 0.0 && spec.max_loss <= 1.0)) {
        throw factor_data_error{
            "max_loss must be a number between 0 and 1"};
    }
    if (spec.quantiles < 2) {
        throw factor_data_error{"quantiles must be at least two"};
    }
    if (spec.periods.empty()) {
        throw factor_data_error{"periods must contain positive integers"};
    }
    for (const int period : spec.periods) {
        if (period <= 0) {
            throw factor_data_error{
                "periods must contain positive integers"};
        }
    }
    for (const int lag : spec.stability_lags) {
        if (lag <= 0) {
            throw factor_data_error{
                "stability_lags must contain positive integers"};
        }
    }
    for (const auto& [name, bounds] : spec.segments) {
        if (name.empty() || bounds.first.empty() || bounds.second.empty()) {
            throw factor_data_error{
                "segments must have non-empty names and bounds"};
        }
    }
    bool input_keys_sorted = true;
    for (std::size_t row = 1U; row < factors.dates.size(); ++row) {
        const std::size_t previous = row - 1U;
        if (factors.dates[row] < factors.dates[previous] ||
            (factors.dates[row] == factors.dates[previous] &&
             factors.assets[row] < factors.assets[previous])) {
            input_keys_sorted = false;
            break;
        }
    }
    std::unordered_map<std::string, std::size_t> asset_ids;
    asset_ids.reserve(factors.assets.size());
    std::unordered_set<std::pair<timestamp, std::size_t>, id_cell_hash> seen;
    if (!input_keys_sorted) {
        seen.reserve(factors.dates.size());
    }
    std::vector<unsigned char> full_seen;
    std::vector<unsigned char> window_seen;
    std::vector<std::size_t> window_indices;
    window_indices.reserve(factors.dates.size());
    bool duplicate = false;
    for (std::size_t row = 0; row < factors.dates.size(); ++row) {
        const auto [entry, inserted] =
            asset_ids.try_emplace(factors.assets[row], asset_ids.size());
        const std::size_t id = entry->second;
        if (inserted) {
            full_seen.push_back(0U);
            window_seen.push_back(0U);
        }
        full_seen[id] = 1U;
        if (input_keys_sorted) {
            if (row != 0U &&
                factors.dates[row] == factors.dates[row - 1U] &&
                factors.assets[row] == factors.assets[row - 1U]) {
                duplicate = true;
            }
        } else if (!seen.emplace(factors.dates[row], id).second) {
            duplicate = true;
        }
        if (factors.dates[row] >= spec.start &&
            factors.dates[row] <= spec.end) {
            window_indices.push_back(row);
            window_seen[id] = 1U;
        }
    }
    if (duplicate) {
        throw factor_data_error{
            "factor panel contains duplicate (date, asset) keys"};
    }
    const auto missing_targets =
        [&](const std::vector<unsigned char>& observed) {
            std::vector<std::string> missing;
            for (const auto& target : spec.targets) {
                const auto found = asset_ids.find(target);
                if (found == asset_ids.end() || observed[found->second] == 0U) {
                    missing.push_back(target);
                }
            }
            return missing;
        };
    if (!spec.targets.empty()) {
        const auto missing = missing_targets(full_seen);
        if (!missing.empty()) {
            throw factor_data_error{
                "factor panel is missing requested target assets: " +
                join_names(missing)};
        }
    }
    if (window_indices.empty()) {
        throw factor_data_error{
            "factors contain no cells inside the requested window"};
    }
    if (!spec.targets.empty()) {
        const auto missing = missing_targets(window_seen);
        if (!missing.empty()) {
            throw factor_data_error{
                "research window is missing requested target assets: " +
                join_names(missing)};
        }
    }
    std::vector<unsigned char> selected(asset_ids.size(), 0U);
    for (const auto& target : spec.targets) {
        const auto found = asset_ids.find(target);
        if (found != asset_ids.end()) {
            selected[found->second] = 1U;
        }
    }
    if (!spec.targets.empty()) {
        const auto selected_end = std::remove_if(
            window_indices.begin(), window_indices.end(),
            [&](const std::size_t row) {
                const auto found = asset_ids.find(factors.assets[row]);
                return found == asset_ids.end() ||
                       selected[found->second] == 0U;
            });
        window_indices.erase(selected_end, window_indices.end());
    }
    return window_indices;
}

}  // namespace

research_capabilities research_capabilities_report() {
    return {};
}

factor_series research_panel(const factor_series& factors,
                              const research_spec& spec) {
    detail::note_panel_preprocessing();
    const auto selected_rows =
        select_panel_source_rows_impl(factors, spec);
    factor_series panel;
    panel.dates.reserve(selected_rows.size());
    panel.assets.reserve(selected_rows.size());
    panel.values.reserve(selected_rows.size());
    for (const std::size_t row : selected_rows) {
        panel.dates.push_back(factors.dates[row]);
        panel.assets.push_back(factors.assets[row]);
        panel.values.push_back(factors.values[row]);
    }
    for (auto& value : panel.values) {
        value = std::isfinite(value) ? value : nan;
    }
    for (std::size_t row = 0; row < panel.dates.size(); ++row) {
        const auto direction =
            spec.direction_overrides.find(panel.assets[row]);
        if (direction != spec.direction_overrides.end() &&
            std::isfinite(panel.values[row])) {
            panel.values[row] *= static_cast<double>(direction->second);
        }
    }
    for (const auto& [asset, group] : spec.groups) {
        if (asset.empty() || group.empty()) {
            throw factor_data_error{
                "groups must have non-empty asset and group names"};
        }
    }
    if (!spec.groups.empty()) {
        for (const auto& asset : panel.assets) {
            if (!spec.groups.contains(asset)) {
                throw factor_data_error{
                    "groups do not cover every evaluated asset"};
            }
        }
    }
    preprocess(panel, spec.preprocess);

    return panel;
}

namespace detail {

factor_series research_panel_for_benchmark(
    const factor_series& factors, const research_spec& spec) {
    return research_panel(factors, spec);
}

}  // namespace detail

std::vector<research_signal_row> build_signals(
    const factor_series& factors, const factor_series& panel,
    const research_spec& spec) {
    std::unordered_map<timestamp, std::size_t> dates;
    std::unordered_map<std::string, std::size_t> assets;
    dates.reserve(factors.dates.size());
    assets.reserve(factors.assets.size());
    for (std::size_t row = 0; row < factors.dates.size(); ++row) {
        dates.try_emplace(factors.dates[row], dates.size());
        assets.try_emplace(factors.assets[row], assets.size());
    }
    for (std::size_t row = 0; row < panel.dates.size(); ++row) {
        dates.try_emplace(panel.dates[row], dates.size());
        assets.try_emplace(panel.assets[row], assets.size());
    }
    const auto width = assets.size();
    std::vector<double> raw(dates.size() * width, nan);
    std::vector<unsigned char> has_raw(dates.size() * width, 0U);
    for (std::size_t row = 0; row < factors.dates.size(); ++row) {
        const auto date = dates.find(factors.dates[row]);
        const auto asset = assets.find(factors.assets[row]);
        if (date == dates.end() || asset == assets.end()) {
            continue;
        }
        const auto index = date->second * width + asset->second;
        raw[index] = factors.values[row];
        has_raw[index] = 1U;
    }
    std::vector<research_signal_row> signals;
    signals.reserve(panel.dates.size());
    for (std::size_t row = 0; row < panel.dates.size(); ++row) {
        const auto direction =
            spec.direction_overrides.find(panel.assets[row]);
        const int sign = direction != spec.direction_overrides.end()
                             ? direction->second
                             : 1;
        const auto date = dates.find(panel.dates[row]);
        const auto asset = assets.find(panel.assets[row]);
        const auto index = date->second * width + asset->second;
        const double value = has_raw[index] != 0U
                                 ? raw[index] * static_cast<double>(sign)
                                 : panel.values[row];
        const auto group = spec.groups.find(panel.assets[row]);
        signals.push_back(
            {panel.dates[row], panel.assets[row], "factor", sign, value,
             panel.values[row], spec.preprocess,
             group != spec.groups.end() ? group->second : "default"});
    }
    return signals;
}

factor_series apply_universe(const factor_series& panel,
                             const dataframe& universe) {
    if (!universe.get_columns_info<double>().empty()) {
        throw factor_data_error{
            "universe must contain only boolean columns"};
    }
    const auto columns = universe.get_columns_info<bool>();
    if (columns.empty()) {
        throw factor_data_error{
            "universe must contain at least one boolean asset column"};
    }
    if (std::adjacent_find(universe.get_index().begin(),
                           universe.get_index().end()) !=
        universe.get_index().end()) {
        throw factor_data_error{"universe index must be unique"};
    }
    std::map<timestamp, std::size_t> rows;
    for (std::size_t row = 0; row < universe.get_index().size(); ++row) {
        rows[universe.get_index()[row]] = row;
    }
    std::map<std::string, const std::vector<bool>*> flags;
    for (const auto& info : columns) {
        const std::string name{std::get<0>(info).c_str()};
        flags[name] = &universe.get_column<bool>(name.c_str());
    }
    factor_series filtered;
    for (std::size_t row = 0; row < panel.dates.size(); ++row) {
        const auto universe_row = rows.find(panel.dates[row]);
        if (universe_row == rows.end()) {
            continue;
        }
        const auto column = flags.find(panel.assets[row]);
        if (column == flags.end() || !(*column->second)[universe_row->second]) {
            continue;
        }
        filtered.dates.push_back(panel.dates[row]);
        filtered.assets.push_back(panel.assets[row]);
        filtered.values.push_back(panel.values[row]);
    }
    if (filtered.dates.empty()) {
        throw factor_data_error{
            "universe removed every factor cell"};
    }
    return filtered;
}

research_result assemble_research(
    const multi_horizon_prepared_factor_data& prepared,
    const research_spec& spec);

static research_result evaluate_with_table(
    const factor_series& panel,
    const forward_return_table& precomputed_returns,
    const research_spec& spec, const std::string& label,
    bool positional_period_selection);

namespace {

constexpr timestamp nanoseconds_per_day = 86'400'000'000'000LL;

timestamp utc_day(const timestamp value) {
    timestamp day = value / nanoseconds_per_day;
    if (value % nanoseconds_per_day < 0) {
        --day;
    }
    return day;
}

void append_duration_component(std::string& result,
                               const std::int64_t value,
                               const char* suffix) {
    if (value > 0) {
        result += std::to_string(value);
        result += suffix;
    }
}

std::string timedelta_label(const std::int64_t days,
                            timestamp remainder) {
    constexpr timestamp nanoseconds_per_hour = 3'600'000'000'000LL;
    constexpr timestamp nanoseconds_per_minute = 60'000'000'000LL;
    constexpr timestamp nanoseconds_per_second = 1'000'000'000LL;
    constexpr timestamp nanoseconds_per_millisecond = 1'000'000LL;
    constexpr timestamp nanoseconds_per_microsecond = 1'000LL;

    const auto hours = remainder / nanoseconds_per_hour;
    remainder %= nanoseconds_per_hour;
    const auto minutes = remainder / nanoseconds_per_minute;
    remainder %= nanoseconds_per_minute;
    const auto seconds = remainder / nanoseconds_per_second;
    remainder %= nanoseconds_per_second;
    const auto milliseconds = remainder / nanoseconds_per_millisecond;
    remainder %= nanoseconds_per_millisecond;
    const auto microseconds = remainder / nanoseconds_per_microsecond;
    remainder %= nanoseconds_per_microsecond;

    std::string result;
    append_duration_component(result, days, "D");
    append_duration_component(result, hours, "h");
    append_duration_component(result, minutes, "m");
    append_duration_component(result, seconds, "s");
    append_duration_component(result, milliseconds, "ms");
    append_duration_component(result, microseconds, "us");
    append_duration_component(result, remainder, "ns");
    return result;
}

timestamp utc_day_remainder(const timestamp value) {
    timestamp remainder = value % nanoseconds_per_day;
    if (remainder < 0) {
        remainder += nanoseconds_per_day;
    }
    return remainder;
}

std::string canonical_period_label(
    const dataframe& prices,
    const std::vector<timestamp>& study_dates,
    const int period) {
    const auto& index = prices.get_index();
    std::set<timestamp> observed_days;
    for (const timestamp date : index) {
        observed_days.insert(utc_day(date));
    }
    for (const timestamp date : study_dates) {
        observed_days.insert(utc_day(date));
    }

    const auto period_size = static_cast<std::size_t>(period);
    for (const timestamp date : study_dates) {
        const auto found = std::find(index.begin(), index.end(), date);
        if (found == index.end()) {
            continue;
        }
        const auto row = static_cast<std::size_t>(found - index.begin());
        if (period_size >= index.size() - row) {
            continue;
        }

        const timestamp start = index[row];
        const timestamp end = index[row + period_size];
        if (end <= start) {
            continue;
        }

        const timestamp start_day = utc_day(start);
        const timestamp end_day = utc_day(end);
        const auto first_session = observed_days.lower_bound(start_day);
        const auto last_session = observed_days.lower_bound(end_day);
        const auto trading_days = static_cast<std::int64_t>(
            std::distance(first_session, last_session));

        // The inferred custom calendar treats each observed weekday as active
        // and every missing date within the grid span as a holiday. Thus its
        // sessions between these endpoints are the distinct observed dates
        // in the half-open [start, end) range.
        timestamp wall_clock_remainder =
            utc_day_remainder(end) - utc_day_remainder(start);
        if (wall_clock_remainder < 0) {
            wall_clock_remainder += nanoseconds_per_day;
        }
        return timedelta_label(trading_days, wall_clock_remainder);
    }
    return std::to_string(period) + "D";
}

std::vector<timestamp> research_study_dates(const dataframe& prices,
                                            const research_spec& spec) {
    std::vector<timestamp> dates;
    for (const timestamp date : prices.get_index()) {
        if (date >= spec.start && date <= spec.end) {
            dates.push_back(date);
        }
    }
    return dates;
}

std::vector<std::string> canonical_period_labels(
    const dataframe& prices,
    const research_spec& spec,
    const std::vector<timestamp>& study_dates) {
    std::vector<std::string> labels;
    labels.reserve(spec.periods.size());
    std::set<std::string> unique_labels;
    for (const int period : spec.periods) {
        const auto label = canonical_period_label(prices, study_dates, period);
        if (!unique_labels.insert(label).second) {
            throw factor_data_error{
                "requested periods map to duplicate forward-return labels"};
        }
        labels.push_back(label);
    }
    return labels;
}

forward_return_table next_open_table(const dataframe& prices,
                                     const research_spec& spec,
                                     const std::vector<std::string>& labels) {
    std::vector<std::string> assets;
    for (const auto& info : prices.get_columns_info<double>()) {
        if (std::get<2>(info) == typeid(double)) {
            assets.emplace_back(std::get<0>(info).c_str());
        }
    }
    if (assets.empty() || prices.get_index().size() < 2U) {
        throw factor_data_error{
            "next-open labels require prices with assets and at least two rows"};
    }
    if (!std::is_sorted(prices.get_index().begin(), prices.get_index().end()) ||
        std::adjacent_find(prices.get_index().begin(),
                           prices.get_index().end()) !=
            prices.get_index().end()) {
        throw factor_data_error{
            "next-open price index must be sorted and duplicate-free"};
    }
    forward_return_table table;
    table.dates = prices.get_index();
    table.assets = assets;
    table.labels = labels;
    std::vector<const std::vector<double>*> columns;
    columns.reserve(assets.size());
    for (const auto& asset : assets) {
        columns.push_back(&prices.get_column<double>(asset.c_str()));
    }
    for (const int period : spec.periods) {
        std::vector<double> values(table.dates.size() * assets.size(), nan);
        for (std::size_t row = 0; row < table.dates.size(); ++row) {
            const std::size_t numerator_row = row +
                                              static_cast<std::size_t>(period) +
                                              1U;
            if (numerator_row >= table.dates.size()) {
                continue;
            }
            for (std::size_t asset = 0; asset < assets.size(); ++asset) {
                const auto& column = *columns[asset];
                const double numerator = column[numerator_row];
                const double denominator = column[row + 1U];
                const double value = numerator / denominator - 1.0;
                values[row * assets.size() + asset] =
                    std::isfinite(value) ? value : nan;
            }
        }
        table.values.push_back(std::move(values));
    }
    return table;
}

void append_detailed_next_open_horizons(
    const factor_series& panel, const dataframe& prices,
    const research_spec& spec, const std::vector<std::string>& periods,
    research_detailed_detail::label_input& result) {
    const auto& price_dates = prices.get_index();
    const auto price_columns = prices.get_columns_info<double>();
    std::size_t double_column_count = 0U;
    for (const auto& info : price_columns) {
        if (std::get<2>(info) == typeid(double)) {
            ++double_column_count;
        }
    }
    if (double_column_count == 0U || price_dates.size() < 2U) {
        throw factor_data_error{
            "next-open labels require prices with assets and at least two rows"};
    }
    if (!std::is_sorted(price_dates.begin(), price_dates.end()) ||
        std::adjacent_find(price_dates.begin(), price_dates.end()) !=
            price_dates.end()) {
        throw factor_data_error{
            "next-open price index must be sorted and duplicate-free"};
    }

    // Prefer a direct non-owning lookup over all double columns when there
    // are more panel rows than available columns. The alternative keeps only
    // selected assets in the map, which bounds temporary storage for wide
    // price tables with small panels.
    std::unordered_map<std::string_view, const std::vector<double>*>
        price_columns_by_asset;
    if (panel.dates.size() > double_column_count) {
        // The string views point into names owned by price_columns, which
        // stays alive and unchanged until this function returns. Use c_str()
        // for DataFrame's existing lookup semantics; assigning in info order
        // preserves the previous last-match behavior.
        price_columns_by_asset.reserve(double_column_count);
        for (const auto& info : price_columns) {
            if (std::get<2>(info) != typeid(double)) {
                continue;
            }
            const auto& name = std::get<0>(info);
            price_columns_by_asset.insert_or_assign(
                std::string_view{name.c_str()},
                &prices.get_column<double>(name.c_str()));
        }
    } else {
        // These keys refer to strings owned by panel, which remains unchanged
        // for this call. Column-name lookup keeps the same C-string semantics
        // as DataFrame::get_column.
        price_columns_by_asset.reserve(
            std::min(panel.assets.size(), price_columns.size()));
        for (const auto& asset : panel.assets) {
            price_columns_by_asset.try_emplace(std::string_view{asset},
                                               nullptr);
        }
        for (const auto& info : price_columns) {
            if (std::get<2>(info) != typeid(double)) {
                continue;
            }
            const auto& name = std::get<0>(info);
            const auto found = price_columns_by_asset.find(
                std::string_view{name.c_str()});
            if (found != price_columns_by_asset.end()) {
                found->second = &prices.get_column<double>(name.c_str());
            }
        }
    }

    // The input panel need not be sorted. Map grid dates once, then resolve
    // each panel row's date and selected price column before its horizon loop.
    std::unordered_map<timestamp, std::size_t> date_positions;
    date_positions.reserve(price_dates.size());
    for (std::size_t row = 0; row < price_dates.size(); ++row) {
        date_positions.emplace(price_dates[row], row);
    }

    const std::size_t first_output_horizon = result.horizons.size();
    for (std::size_t horizon = 0; horizon < periods.size(); ++horizon) {
        result.horizons.push_back(
            {periods[horizon], std::vector<double>(panel.dates.size(), nan)});
    }

    bool has_cached_date = false;
    timestamp cached_date = 0;
    auto cached_date_position = date_positions.end();
    for (std::size_t row = 0; row < panel.dates.size(); ++row) {
        if (!has_cached_date || panel.dates[row] != cached_date) {
            cached_date = panel.dates[row];
            cached_date_position = date_positions.find(cached_date);
            has_cached_date = true;
        }
        const auto asset = price_columns_by_asset.find(
            std::string_view{panel.assets[row]});
        if (cached_date_position == date_positions.end() ||
            asset == price_columns_by_asset.end() || asset->second == nullptr) {
            continue;
        }
        const std::size_t price_row = cached_date_position->second;
        const auto& column = *asset->second;
        for (std::size_t horizon = 0; horizon < periods.size(); ++horizon) {
            const auto period =
                static_cast<std::size_t>(spec.periods[horizon]);
            const std::size_t numerator_row = price_row + period + 1U;
            if (numerator_row >= price_dates.size()) {
                continue;
            }
            const double value =
                column[numerator_row] / column[price_row + 1U] - 1.0;
            result.horizons[first_output_horizon + horizon].label_values[row] =
                std::isfinite(value) ? value : nan;
        }
    }
}

}  // namespace

std::vector<std::size_t>
research_detailed_detail::select_panel_source_rows(
    const factor_series& factors, const research_spec& spec) {
    detail::note_panel_preprocessing();
    return select_panel_source_rows_impl(factors, spec);
}

research_detailed_detail::label_input
research_detailed_detail::build_detailed_label_input(
    const factor_series& panel, const dataframe& prices,
    const research_spec& spec) {
    if (spec.label != "close_to_close" &&
        spec.label != "next_open_to_open") {
        throw factor_data_error{"unsupported research label: " + spec.label};
    }
    const auto periods = canonical_period_labels(
        prices, spec, research_study_dates(prices, spec));
    research_detailed_detail::label_input result;
    result.periods = periods;
    result.horizons.reserve(periods.size());
    if (spec.label == "close_to_close") {
        auto forward = compute_forward_returns(panel, prices, spec.periods,
                                               std::nullopt, true);
        if (forward.values.size() != periods.size()) {
            throw factor_data_error{
                "price-derived labels must match the requested periods"};
        }
        for (std::size_t horizon = 0; horizon < periods.size(); ++horizon) {
            auto values = std::move(forward.values[horizon]);
            for (double& value : values) {
                if (!std::isfinite(value)) {
                    value = nan;
                }
            }
            result.horizons.push_back(
                {periods[horizon], std::move(values)});
        }
        return result;
    }

    append_detailed_next_open_horizons(panel, prices, spec, periods, result);
    return result;
}

namespace {

struct evaluated_research {
    research_result result;
    factor_series panel;
};

evaluated_research evaluate_factor_research_with_panel(
    const factor_series& factors, const dataframe& prices,
    const research_spec& spec) {
    if (spec.label == "precomputed") {
        throw factor_data_error{
            "precomputed labels require the precomputed evaluator"};
    }
    if (spec.label != "close_to_close" &&
        spec.label != "next_open_to_open") {
        throw factor_data_error{"unsupported research label: " + spec.label};
    }
    factor_series panel = research_panel(factors, spec);
    const auto labels = canonical_period_labels(
        prices, spec, research_study_dates(prices, spec));
    if (spec.label == "next_open_to_open") {
        auto result = evaluate_with_table(
            panel, next_open_table(prices, spec, labels), spec,
            "next_open_to_open", true);
        result.signals = build_signals(factors, panel, spec);
        return {std::move(result), std::move(panel)};
    }

    prepare_factor_config config;
    config.periods = spec.periods;
    config.quantiles = spec.quantiles;
    config.max_loss = spec.max_loss;
    auto prepared =
        prepare_factor_data_by_horizon(panel, prices, config);
    if (prepared.by_horizon.size() != spec.periods.size()) {
        throw factor_data_error{
            "prepared horizons must match the requested periods"};
    }
    for (std::size_t index = 0; index < spec.periods.size(); ++index) {
        const std::string& label = labels[index];
        prepared.by_horizon[index].first = label;
        prepared.by_horizon[index].second.forward_labels = {label};
    }
    auto result = assemble_research(prepared, spec);
    result.signals = build_signals(factors, panel, spec);
    return {std::move(result), std::move(panel)};
}

}  // namespace

research_result evaluate_factor_research(const factor_series& factors,
                                         const dataframe& prices,
                                         const research_spec& spec) {
    return std::move(
        evaluate_factor_research_with_panel(factors, prices, spec).result);
}

research_result assemble_research(
    const multi_horizon_prepared_factor_data& prepared,
    const research_spec& spec) {
    research_result result;
    result.manifest = {
        {"api_version", research_api_version},
        {"schema_version", std::to_string(research_result_schema_version)},
        {"snapshot_id", spec.snapshot_id},
        {"price_basis", spec.price_basis},
        {"calendar_id", spec.calendar_id},
        {"frequency", spec.frequency},
        {"label", spec.label},
        {"preprocess", spec.preprocess},
        {"quantiles", std::to_string(spec.quantiles)},
        {"periods", std::to_string(spec.periods.size())},
        {"groups", std::to_string(spec.groups.size())}};
    for (const auto& [name, bounds] : spec.segments) {
        result.manifest["segment." + name] =
            bounds.first + ".." + bounds.second;
    }
    result.spread.assign(spec.periods.size(), nan);
    result.coverage.assign(spec.periods.size(), 0U);
    if (prepared.by_horizon.empty()) {
        result.status = "UNEVALUABLE";
        result.diagnostics.push_back("no prepared horizons");
        return result;
    }

    // Combine per-horizon ICs into one date x horizon table and summaries.
    std::vector<factor_ic_table> ics(prepared.by_horizon.size());
    for (std::size_t index = 0; index < prepared.by_horizon.size(); ++index) {
        ics[index] = factor_information_coefficient(
            prepared.by_horizon[index].second);
    }
    std::set<timestamp> dates_set;
    for (const auto& ic : ics) {
        for (const auto date : ic.dates) {
            dates_set.insert(date);
        }
    }
    result.information_coefficient.dates.assign(dates_set.begin(),
                                                dates_set.end());
    for (const auto& entry : prepared.by_horizon) {
        result.information_coefficient.labels.push_back(entry.first);
    }
    const std::size_t labels =
        result.information_coefficient.labels.size();
    const std::size_t dates =
        result.information_coefficient.dates.size();
    result.information_coefficient.values.assign(dates * labels, nan);
    result.information_coefficient_summary.labels =
        result.information_coefficient.labels;
    result.information_coefficient_summary.means.assign(labels, nan);
    result.quantile_returns.labels = result.information_coefficient.labels;
    std::set<int> quantiles;
    for (const auto& entry : prepared.by_horizon) {
        for (const auto& row : entry.second.rows) {
            quantiles.insert(row.quantile);
        }
    }
    result.quantile_returns.quantiles.assign(quantiles.begin(),
                                             quantiles.end());
    result.quantile_returns.means.assign(
        result.quantile_returns.quantiles.size() * labels, nan);
    result.quantile_returns.standard_errors.assign(
        result.quantile_returns.quantiles.size() * labels, nan);
    std::vector<quantile_return_table> quantile_tables(
        prepared.by_horizon.size());
    for (std::size_t index = 0; index < prepared.by_horizon.size(); ++index) {
        quantile_tables[index] = mean_return_by_quantile(
            prepared.by_horizon[index].second, false, false, false, false);
    }
    std::map<timestamp, std::size_t> date_position;
    for (std::size_t index = 0; index < dates; ++index) {
        date_position[result.information_coefficient.dates[index]] = index;
    }
    for (std::size_t label = 0; label < labels; ++label) {
        const auto& prepared_horizon = prepared.by_horizon[label].second;
        const auto& ic = ics[label];
        for (std::size_t index = 0; index < ic.dates.size(); ++index) {
            result.information_coefficient.values
                [date_position[ic.dates[index]] * labels + label] =
                ic.values[index];
        }
        result.coverage[label] = prepared_horizon.rows.size();
        const auto summary =
            detail::summarize_information_coefficient(ic);
        if (!summary.means.empty()) {
            result.information_coefficient_summary.means[label] =
                summary.means.front();
        }
        // Research tables report raw quantile returns; demeaning is an
        // analysis-view option and would make the table incomparable with the
        // Python research contract.
        const auto& quantile_table = quantile_tables[label];
        for (std::size_t quantile = 0;
             quantile < result.quantile_returns.quantiles.size();
             ++quantile) {
            const auto found = std::find(
                quantile_table.quantiles.begin(),
                quantile_table.quantiles.end(),
                result.quantile_returns.quantiles[quantile]);
            if (found == quantile_table.quantiles.end()) {
                continue;
            }
            const std::size_t source =
                static_cast<std::size_t>(found -
                                         quantile_table.quantiles.begin());
            if (!quantile_table.means.empty()) {
                result.quantile_returns.means
                    [quantile * labels + label] =
                    quantile_table.means[source];
                result.quantile_returns.standard_errors
                    [quantile * labels + label] =
                    quantile_table.standard_errors[source];
            }
        }
        if (result.quantile_returns.quantiles.size() >= 2U &&
            !quantile_table.means.empty()) {
            const double top =
                quantile_table.means[result.quantile_returns.quantiles.size() -
                                     1U];
            const double bottom = quantile_table.means[0];
            result.spread[label] = top - bottom;
        }
        if (label == 0U) {
            result.stability =
                detail::factor_rank_autocorrelation_many(
                    prepared_horizon, spec.stability_lags);
            result.turnover = detail::quantile_turnover_many(
                prepared_horizon, result.quantile_returns.quantiles, 1);
        }
    }
    result.stability_lags = spec.stability_lags;
    for (const auto& [label, horizon] : prepared.by_horizon) {
        for (const auto& row : horizon.rows) {
            if (!row.forward_returns.empty()) {
                result.labels.push_back(
                    {row.date, row.asset, label, row.forward_returns.front()});
            }
        }
    }
    std::size_t label_row_count = 0U;
    for (const auto& entry : prepared.by_horizon) {
        label_row_count += entry.second.rows.size();
    }
    result.labels.reserve(label_row_count);
    result.status = result.coverage.empty() ? "UNEVALUABLE" : "OK";
    for (const std::size_t count : result.coverage) {
        if (count == 0U) {
            result.status = "PARTIAL";
            result.diagnostics.push_back("a horizon has no prepared rows");
        }
    }
    return result;
}



namespace {

void append_csv_escape(std::string& output, const std::string& text) {
    if (text.find_first_of(",\"\n\r") == std::string::npos) {
        output += text;
        return;
    }
    output.push_back('"');
    for (const char character : text) {
        if (character == '"') {
            output.push_back('"');
        }
        output.push_back(character);
    }
    output.push_back('"');
}

std::string csv_number(const double value) {
    if (!std::isfinite(value)) {
        return {};
    }
    std::string result;
    detail::append_python_float(result, value, "research export");
    return result;
}

std::string json_escape(const std::string& text) {
    std::string escaped;
    for (const char character : text) {
        switch (character) {
            case '"': escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped.push_back(character); break;
        }
    }
    return escaped;
}

void append_row_text(std::string& line,
                     const std::vector<std::string>& cells) {
    std::size_t reserve = cells.size();
    for (const auto& cell : cells) {
        reserve += cell.size() + 3U;
    }
    line.reserve(line.size() + reserve);
    for (std::size_t index = 0; index < cells.size(); ++index) {
        if (index > 0U) {
            line.push_back(',');
        }
        append_csv_escape(line, cells[index]);
    }
    line.push_back('\n');
}

std::string row_text(const std::vector<std::string>& cells) {
    std::string line;
    append_row_text(line, cells);
    return line;
}

constexpr std::size_t export_buffer_bytes = 1U << 20;

void flush_export_buffer(std::ofstream& stream, std::string& buffer) {
    if (buffer.empty()) {
        return;
    }
    stream.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    if (!stream) {
        throw factor_data_error{"could not write research export file"};
    }
    buffer.clear();
}

void finish_output_stream(std::ofstream& stream,
                          const char* write_error_message) {
    stream.flush();
    if (!stream) {
        throw factor_data_error{write_error_message};
    }
    stream.close();
    if (!stream) {
        throw factor_data_error{write_error_message};
    }
}

void validate_matrix_size(const char* name, const std::size_t rows,
                          const std::size_t columns,
                          const std::size_t actual_size) {
    if (columns != 0U &&
        rows > std::numeric_limits<std::size_t>::max() / columns) {
        throw factor_data_error{std::string{"research export "} + name +
                               " dimensions overflow"};
    }
    if (actual_size != rows * columns) {
        throw factor_data_error{std::string{"research export "} + name +
                               " dimensions do not match"};
    }
}

void validate_export_shapes(const research_result& result) {
    const auto label_count = result.information_coefficient.labels.size();
    validate_matrix_size("information coefficient",
                         result.information_coefficient.dates.size(),
                         label_count,
                         result.information_coefficient.values.size());

    const auto& quantile_table = result.quantile_returns;
    if (quantile_table.labels != result.information_coefficient.labels) {
        throw factor_data_error{
            "research export quantile labels do not match IC labels"};
    }
    const auto quantile_count = quantile_table.quantiles.size();
    validate_matrix_size("quantile means", quantile_count, label_count,
                         quantile_table.means.size());
    validate_matrix_size("quantile standard errors", quantile_count,
                         label_count, quantile_table.standard_errors.size());

    const auto validate_series = [](const char* name,
                                    const std::vector<factor_series_result>&
                                        series_collection) {
        for (const auto& series : series_collection) {
            if (series.dates.size() != series.values.size()) {
                throw factor_data_error{std::string{"research export "} +
                                       name +
                                       " dates and values dimensions do not match"};
            }
        }
    };
    validate_series("turnover", result.turnover);
    validate_series("stability", result.stability);
}

void validate_export_manifest(const research_result& result) {
    for (const std::string_view reserved_key : {"status", "exports"}) {
        if (result.manifest.contains(std::string{reserved_key})) {
            throw factor_data_error{
                "research export manifest contains reserved key: " +
                std::string{reserved_key}};
        }
    }
}

class created_directory_guard {
public:
    explicit created_directory_guard(std::filesystem::path path)
        : path_(std::move(path)) {}

    created_directory_guard(const created_directory_guard&) = delete;
    created_directory_guard& operator=(const created_directory_guard&) = delete;

    ~created_directory_guard() {
        if (active_) {
            std::error_code ignored;
            std::filesystem::remove_all(path_, ignored);
        }
    }

    void arm() noexcept { active_ = true; }
    void release() noexcept { active_ = false; }

private:
    std::filesystem::path path_;
    bool active_{false};
};

void write_table_rows_impl(const std::filesystem::path& path,
                           const std::vector<std::string>& header,
                           const std::vector<std::vector<std::string>>& rows) {
    std::ofstream stream(path, std::ios::binary);
    if (!stream) {
        throw factor_data_error{"could not create research export file"};
    }
    std::string buffer;
    buffer.reserve(export_buffer_bytes + 65536U);
    append_row_text(buffer, header);
    for (const auto& row : rows) {
        append_row_text(buffer, row);
        if (buffer.size() >= export_buffer_bytes) {
            flush_export_buffer(stream, buffer);
        }
    }
    flush_export_buffer(stream, buffer);
    finish_output_stream(stream, "could not write research export file");
}

using research_export_detail::row_emitter;
using research_export_detail::table_generator;

void write_table(const std::filesystem::path& path,
                 const std::vector<std::string>& header,
                 const table_generator& generate) {
    std::ofstream stream(path, std::ios::binary);
    if (!stream) {
        throw factor_data_error{"could not create research export file"};
    }
    std::string buffer;
    buffer.reserve(export_buffer_bytes + 65536U);
    append_row_text(buffer, header);
    generate([&stream, &buffer](const std::vector<std::string>& row) {
        append_row_text(buffer, row);
        if (buffer.size() >= export_buffer_bytes) {
            flush_export_buffer(stream, buffer);
        }
    });
    flush_export_buffer(stream, buffer);
    finish_output_stream(stream, "could not write research export file");
}

}  // namespace

namespace research_export_detail {

void write_table_rows(
    const std::filesystem::path& path,
    const std::vector<std::string>& header,
    const std::vector<std::vector<std::string>>& rows) {
    write_table_rows_impl(path, header, rows);
}

}  // namespace research_export_detail

research_result evaluate_factor_research(const factor_series& factors,
                                         const dataframe& prices,
                                         const research_spec& spec,
                                         const dataframe& universe) {
    if (spec.label == "precomputed") {
        throw factor_data_error{
            "precomputed labels require the precomputed evaluator"};
    }
    if (spec.label != "close_to_close" &&
        spec.label != "next_open_to_open") {
        throw factor_data_error{"unsupported research label: " + spec.label};
    }
    const factor_series panel =
        apply_universe(research_panel(factors, spec), universe);
    const auto labels = canonical_period_labels(
        prices, spec, research_study_dates(prices, spec));

    if (spec.label == "next_open_to_open") {
        auto result = evaluate_with_table(
            panel, next_open_table(prices, spec, labels), spec,
            "next_open_to_open", true);
        result.signals = build_signals(factors, panel, spec);
        result.manifest["universe"] = "applied";
        return result;
    }

    prepare_factor_config config;
    config.periods = spec.periods;
    config.quantiles = spec.quantiles;
    config.max_loss = spec.max_loss;
    auto prepared =
        prepare_factor_data_by_horizon(panel, prices, config);
    if (prepared.by_horizon.size() != labels.size()) {
        throw factor_data_error{
            "prepared horizons must match the requested periods"};
    }
    for (std::size_t index = 0; index < labels.size(); ++index) {
        prepared.by_horizon[index].first = labels[index];
        prepared.by_horizon[index].second.forward_labels = {labels[index]};
    }
    auto result = assemble_research(prepared, spec);
    result.signals = build_signals(factors, panel, spec);
    result.manifest["universe"] = "applied";
    return result;
}

static research_result evaluate_with_table(
    const factor_series& panel,
    const forward_return_table& precomputed_returns,
    const research_spec& spec, const std::string& label,
    const bool positional_period_selection) {
    const std::size_t asset_count = precomputed_returns.assets.size();
    const std::size_t date_count = precomputed_returns.dates.size();
    const std::size_t label_count = precomputed_returns.labels.size();
    if (date_count == 0U || asset_count == 0U || label_count == 0U) {
        throw factor_data_error{
            "precomputed returns must contain dates, assets and labels"};
    }
    if (precomputed_returns.values.size() != label_count) {
        throw factor_data_error{
            "precomputed returns must be label-major"};
    }
    for (const auto& values : precomputed_returns.values) {
        if (values.size() != date_count * asset_count) {
            throw factor_data_error{
                "precomputed return rows must match dates x assets"};
        }
    }
    std::unordered_map<timestamp, std::size_t> date_index;
    date_index.reserve(date_count);
    for (std::size_t date = 0; date < date_count; ++date) {
        date_index.try_emplace(precomputed_returns.dates[date], date);
    }
    std::unordered_map<std::string, std::size_t> asset_index;
    asset_index.reserve(asset_count);
    for (std::size_t asset = 0; asset < asset_count; ++asset) {
        asset_index.try_emplace(precomputed_returns.assets[asset], asset);
    }
    std::vector<unsigned char> present(date_count * asset_count, 0U);
    for (std::size_t date = 0; date < date_count; ++date) {
        const auto date_position =
            date_index.find(precomputed_returns.dates[date]);
        for (std::size_t asset = 0; asset < asset_count; ++asset) {
            const auto asset_position =
                asset_index.find(precomputed_returns.assets[asset]);
            const auto index =
                date_position->second * asset_count + asset_position->second;
            if (present[index] != 0U) {
                throw factor_data_error{
                    "precomputed returns keys must be unique"};
            }
            present[index] = 1U;
        }
    }
    constexpr std::size_t missing_index =
        std::numeric_limits<std::size_t>::max();
    std::vector<std::size_t> panel_index(panel.dates.size(), missing_index);
    for (std::size_t row = 0; row < panel.dates.size(); ++row) {
        const auto date = date_index.find(panel.dates[row]);
        const auto asset = asset_index.find(panel.assets[row]);
        if (date == date_index.end() || asset == asset_index.end()) {
            continue;
        }
        const auto index = date->second * asset_count + asset->second;
        if (present[index] != 0U) {
            panel_index[row] = index;
        }
    }
    std::vector<std::size_t> label_indices;
    for (std::size_t period_index = 0; period_index < spec.periods.size();
         ++period_index) {
        if (positional_period_selection) {
            if (period_index >= precomputed_returns.labels.size()) {
                throw factor_data_error{
                    "forward-return table is missing requested periods"};
            }
            label_indices.push_back(period_index);
            continue;
        }
        const std::string label =
            std::to_string(spec.periods[period_index]) + "D";
        const auto found = std::find(precomputed_returns.labels.begin(),
                                     precomputed_returns.labels.end(),
                                     label);
        if (found != precomputed_returns.labels.end()) {
            label_indices.push_back(static_cast<std::size_t>(
                found - precomputed_returns.labels.begin()));
        } else if (precomputed_returns.labels.size() ==
                   spec.periods.size()) {
            label_indices.push_back(period_index);
        } else {
            throw factor_data_error{
                "precomputed returns are missing period " + label};
        }
    }
    quantization_config quantile_config;
    quantile_config.quantiles = spec.quantiles;
    quantile_config.no_raise = spec.max_loss != 0.0;
    multi_horizon_prepared_factor_data prepared;
    for (const std::size_t label_index : label_indices) {
        const std::string& label = precomputed_returns.labels[label_index];
        prepared_factor_data horizon;
        horizon.forward_labels = {label};
        // Quantile buckets are assigned on the cross-section that has a
        // forward return for this horizon, matching the Python research
        // contract. Binning the whole panel would shift edges on dates with
        // missing labels.
        factor_series labeled_panel;
        labeled_panel.dates.reserve(panel.dates.size());
        labeled_panel.assets.reserve(panel.dates.size());
        labeled_panel.values.reserve(panel.dates.size());
        std::vector<std::size_t> labeled_rows;
        labeled_rows.reserve(panel.dates.size());
        for (std::size_t row = 0; row < panel.dates.size(); ++row) {
            const auto index = panel_index[row];
            if (index == missing_index) {
                continue;
            }
            const double value =
                precomputed_returns.values[label_index][index];
            if (!std::isfinite(value)) {
                continue;
            }
            labeled_panel.dates.push_back(panel.dates[row]);
            labeled_panel.assets.push_back(panel.assets[row]);
            labeled_panel.values.push_back(panel.values[row]);
            labeled_rows.push_back(row);
        }
        const auto labeled_quantiles =
            quantize_factor(labeled_panel, quantile_config);
        horizon.rows.reserve(labeled_rows.size());
        for (std::size_t index = 0; index < labeled_rows.size(); ++index) {
            if (!labeled_quantiles[index].has_value()) {
                continue;
            }
            const auto row = labeled_rows[index];
            factor_row factor;
            factor.date = panel.dates[row];
            factor.asset = panel.assets[row];
            factor.factor = panel.values[row];
            factor.quantile = *labeled_quantiles[index];
            factor.has_group = false;
            factor.forward_returns = {
                precomputed_returns.values[label_index][panel_index[row]]};
            horizon.rows.push_back(std::move(factor));
        }
        if (horizon.rows.empty()) {
            throw factor_data_error{
                "precomputed returns produced no prepared rows for " + label};
        }
        prepared.by_horizon.push_back(
            {label, std::move(horizon)});
    }
    auto result = assemble_research(prepared, spec);
    result.manifest["label"] = label;
    return result;
}

research_result evaluate_factor_research(
    const factor_series& factors,
    const forward_return_table& precomputed_returns,
    const research_spec& spec) {
    if (spec.label == "next_open_to_open") {
        throw factor_data_error{
            "next-open labels require the price-based evaluator"};
    }
    const factor_series panel = research_panel(factors, spec);
    auto result = evaluate_with_table(panel, precomputed_returns, spec,
                                      "precomputed", false);
    result.signals = build_signals(factors, panel, spec);
    return result;
}


namespace {

void validate_combinations(
    const research_spec& spec,
    const std::set<std::string>& factor_names) {
    for (const auto& [name, weights] : spec.combinations) {
        if (name.empty() || factor_names.contains(name)) {
            throw factor_data_error{
                "combination names must be non-empty and distinct from factors"};
        }
        bool used = false;
        for (const auto& [factor, weight] : weights) {
            if (!factor_names.contains(factor)) {
                throw factor_data_error{
                    "combination references an unknown factor"};
            }
            if (!std::isfinite(weight)) {
                throw factor_data_error{
                    "combination weights must be finite"};
            }
            if (weight != 0.0) {
                used = true;
            }
        }
        if (!used) {
            throw factor_data_error{
                "combination has no non-zero weights"};
        }
    }
}

factor_series blend_combination(
    const std::string& name,
    const std::map<std::string, double>& weights,
    const std::map<std::string, factor_series>& processed) {
    std::map<std::pair<timestamp, std::string>, double> totals;
    bool first = true;
    for (const auto& [factor, weight] : weights) {
        if (weight == 0.0) {
            continue;
        }
        const auto& panel = processed.at(factor);
        std::map<std::pair<timestamp, std::string>, double> values;
        for (std::size_t row = 0; row < panel.dates.size(); ++row) {
            values[{panel.dates[row], panel.assets[row]}] =
                panel.values[row];
        }
        if (first) {
            for (const auto& [key, value] : values) {
                totals[key] = weight * value;
            }
            first = false;
        } else {
            for (auto entry = totals.begin(); entry != totals.end();) {
                const auto found = values.find(entry->first);
                if (found == values.end()) {
                    entry = totals.erase(entry);
                } else {
                    entry->second += weight * found->second;
                    ++entry;
                }
            }
        }
    }
    factor_series blended;
    for (const auto& [key, value] : totals) {
        blended.dates.push_back(key.first);
        blended.assets.push_back(key.second);
        blended.values.push_back(value);
    }
    if (blended.dates.empty()) {
        throw factor_data_error{
            "combination " + name + " has no shared factor cells"};
    }
    return blended;
}

}  // namespace

research_panel_result evaluate_factor_research_panel(
    const std::span<const named_factor_panel> factors,
    const dataframe& prices, const research_spec& spec) {
    if (factors.empty()) {
        throw factor_data_error{
            "factor panels must contain at least one factor"};
    }
    std::set<std::string> names;
    for (const auto& factor : factors) {
        if (factor.name.empty() || !names.insert(factor.name).second) {
            throw factor_data_error{
                "factor names must be non-empty and unique"};
        }
    }
    validate_combinations(spec, names);
    std::set<std::string> evaluated_names = names;
    for (const auto& [name, weights] : spec.combinations) {
        static_cast<void>(weights);
        evaluated_names.insert(name);
    }
    for (const auto& [name, pair] : spec.comparisons) {
        static_cast<void>(name);
        if (!evaluated_names.contains(pair.first) ||
            !evaluated_names.contains(pair.second)) {
            throw factor_data_error{
                "comparison references an unknown factor"};
        }
    }
    research_panel_result result;
    std::size_t ok_count = 0U;
    std::size_t unevaluable_count = 0U;
    std::map<std::string, factor_series> processed;
    for (const auto& factor : factors) {
        auto evaluated = evaluate_factor_research_with_panel(
            factor.values, prices, spec);
        named_research_result named;
        named.name = factor.name;
        named.result = std::move(evaluated.result);
        if (named.result.status == "OK") {
            ++ok_count;
        } else if (named.result.status == "UNEVALUABLE") {
            ++unevaluable_count;
        }
        for (auto& signal : named.result.signals) {
            signal.factor = factor.name;
            result.signals.push_back(signal);
        }
        processed[factor.name] = std::move(evaluated.panel);
        result.factors.push_back(std::move(named));
    }
    std::vector<research_factor_panel> panels;
    panels.reserve(factors.size() + spec.combinations.size());
    for (const auto& factor : factors) {
        panels.push_back({factor.name, processed.at(factor.name)});
    }
    for (const auto& [name, weights] : spec.combinations) {
        factor_series blended =
            blend_combination(name, weights, processed);
        research_spec combination_spec = spec;
        combination_spec.preprocess = "none";
        combination_spec.direction_overrides.clear();
        combination_spec.combinations.clear();
        named_research_result named;
        named.name = name;
        named.result = evaluate_factor_research(blended, prices,
                                                combination_spec);
        for (auto& signal : named.result.signals) {
            signal.factor = name;
            result.signals.push_back(signal);
        }
        if (named.result.status == "OK") {
            ++ok_count;
        } else if (named.result.status == "UNEVALUABLE") {
            ++unevaluable_count;
        }
        result.factors.push_back(named);
        panels.push_back({name, blended});
    }
    if (ok_count == result.factors.size()) {
        result.status = "OK";
    } else if (unevaluable_count == result.factors.size()) {
        result.status = "UNEVALUABLE";
    } else {
        result.status = "PARTIAL";
    }
    result.correlations =
        research_factor_correlations(panels, spec.min_assets);
    if (!spec.comparisons.empty()) {
        result.comparisons = research_factor_comparisons(
            result.factors, spec.comparisons);
    }
    result.manifest = {
        {"api_version", research_api_version},
        {"schema_version", std::to_string(research_result_schema_version)},
        {"factor_count", std::to_string(result.factors.size())},
        {"combination_count", std::to_string(spec.combinations.size())},
        {"correlation_rows", std::to_string(result.correlations.size())},
        {"comparison_rows", std::to_string(result.comparisons.size())}};
    return result;
}

research_export_result research_export_detail::
    export_factor_research_with_writer(
    const research_result& result, const std::string& directory,
    const bool charts,
    const research_export_detail::table_writer& writer) {
    if (charts) {
        throw factor_data_error{
            "chart export requires a raster renderer that is not available "
            "natively"};
    }
    if (directory.empty()) {
        throw factor_data_error{"research export directory must not be empty"};
    }
    if (directory.find('\0') != std::string::npos) {
        throw factor_data_error{
            "research export directory must not contain a null byte"};
    }
    validate_export_shapes(result);
    validate_export_manifest(result);
    if (!writer) {
        throw factor_data_error{"research export writer must not be empty"};
    }

    const std::filesystem::path target{directory};
    created_directory_guard cleanup{target};
    std::error_code error;
    const auto parent = target.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, error);
        if (error) {
            throw factor_data_error{
                "could not create research export directory"};
        }
    }
    error.clear();
    const bool created = std::filesystem::create_directory(target, error);
    if (created) {
        cleanup.arm();
    }
    if (error) {
        throw factor_data_error{"could not create research export directory"};
    }
    if (!created) {
        throw factor_data_error{
            "evaluation export already exists and will not be overwritten"};
    }

    research_export_result exported;
    exported.directory = directory;
    const auto labels = result.information_coefficient.labels;

    std::unordered_map<timestamp, std::string> timestamp_text;
    timestamp_text.reserve(result.information_coefficient.dates.size());
    const auto format_date =
        [&timestamp_text](const timestamp value) -> const std::string& {
        auto [entry, inserted] = timestamp_text.try_emplace(value);
        if (inserted) {
            entry->second = detail::format_timestamp(value);
        }
        return entry->second;
    };

    std::vector<std::string> ic_header{"date"};
    ic_header.insert(ic_header.end(), labels.begin(), labels.end());
    writer(target / "ic.csv", ic_header, [&](const row_emitter& emit) {
        for (std::size_t date = 0;
             date < result.information_coefficient.dates.size(); ++date) {
            std::vector<std::string> row{
                format_date(result.information_coefficient.dates[date])};
            for (std::size_t label = 0; label < labels.size(); ++label) {
                row.push_back(csv_number(
                    result.information_coefficient.values
                        [date * labels.size() + label]));
            }
            emit(row);
        }
    });
    exported.files.push_back("ic.csv");

    writer(target / "ic_summary.csv", {"label", "mean_ic"},
           [&](const row_emitter& emit) {
        for (std::size_t label = 0; label < labels.size(); ++label) {
            emit({labels[label],
                  csv_number(label < result.information_coefficient_summary.means.size()
                                 ? result.information_coefficient_summary.means[label]
                                 : std::nan(""))});
        }
    });
    exported.files.push_back("ic_summary.csv");

    const std::size_t quantiles = result.quantile_returns.quantiles.size();
    writer(target / "quantile_returns.csv",
           {"quantile", "label", "mean", "standard_error"},
           [&](const row_emitter& emit) {
        for (std::size_t quantile = 0; quantile < quantiles; ++quantile) {
            for (std::size_t label = 0; label < labels.size(); ++label) {
                emit({std::to_string(
                          result.quantile_returns.quantiles[quantile]),
                      labels[label],
                      csv_number(result.quantile_returns.means
                                     [quantile * labels.size() + label]),
                      csv_number(result.quantile_returns.standard_errors
                                     [quantile * labels.size() + label])});
            }
        }
    });
    exported.files.push_back("quantile_returns.csv");

    writer(target / "turnover.csv", {"quantile", "date", "value"},
           [&](const row_emitter& emit) {
        for (std::size_t index = 0; index < result.turnover.size(); ++index) {
            const std::string quantile =
                index < quantiles
                    ? std::to_string(result.quantile_returns.quantiles[index])
                    : std::to_string(index + 1U);
            const auto& series = result.turnover[index];
            for (std::size_t date = 0; date < series.dates.size(); ++date) {
                emit({quantile, format_date(series.dates[date]),
                      csv_number(series.values[date])});
            }
        }
    });
    exported.files.push_back("turnover.csv");

    writer(target / "stability.csv", {"lag", "date", "value"},
           [&](const row_emitter& emit) {
        for (std::size_t index = 0; index < result.stability.size(); ++index) {
            const std::string lag =
                index < result.stability_lags.size()
                    ? std::to_string(result.stability_lags[index])
                    : std::to_string(index + 1U);
            const auto& series = result.stability[index];
            for (std::size_t date = 0; date < series.dates.size(); ++date) {
                emit({lag, format_date(series.dates[date]),
                      csv_number(series.values[date])});
            }
        }
    });
    exported.files.push_back("stability.csv");

    writer(target / "spreads.csv", {"label", "spread"},
           [&](const row_emitter& emit) {
        for (std::size_t label = 0; label < labels.size(); ++label) {
            emit({labels[label],
                  csv_number(label < result.spread.size()
                                 ? result.spread[label]
                                 : std::nan(""))});
        }
    });
    exported.files.push_back("spreads.csv");

    writer(target / "coverage.csv", {"label", "count"},
           [&](const row_emitter& emit) {
        for (std::size_t label = 0; label < labels.size(); ++label) {
            emit({labels[label],
                  label < result.coverage.size()
                      ? std::to_string(result.coverage[label])
                      : std::string{}});
        }
    });
    exported.files.push_back("coverage.csv");

    writer(target / "labels.csv", {"date", "asset", "period", "value"},
           [&](const row_emitter& emit) {
        for (const auto& row : result.labels) {
            emit({format_date(row.date), row.asset, row.period,
                  csv_number(row.value)});
        }
    });
    exported.files.push_back("labels.csv");

    writer(target / "signals.csv",
           {"date", "asset", "factor", "direction", "value",
            "processed_value", "preprocess", "group"},
           [&](const row_emitter& emit) {
        for (const auto& row : result.signals) {
            emit({format_date(row.date), row.asset, row.factor,
                  std::to_string(row.direction), csv_number(row.value),
                  csv_number(row.processed_value), row.preprocess, row.group});
        }
    });
    exported.files.push_back("signals.csv");

    writer(target / "diagnostics.csv", {"index", "message"},
           [&](const row_emitter& emit) {
        for (std::size_t index = 0; index < result.diagnostics.size();
             ++index) {
            emit({std::to_string(index + 1U), result.diagnostics[index]});
        }
    });
    exported.files.push_back("diagnostics.csv");

    std::ostringstream manifest;
    manifest << "{\n  \"status\": \"" << json_escape(result.status)
             << "\",\n  \"exports\": [";
    for (std::size_t index = 0; index < exported.files.size(); ++index) {
        if (index > 0U) {
            manifest << ", ";
        }
        manifest << '\"' << json_escape(exported.files[index]) << '\"';
    }
    manifest << "]";
    for (const auto& [key, value] : result.manifest) {
        manifest << ",\n  \"" << json_escape(key) << "\": \""
                 << json_escape(value) << '\"';
    }
    manifest << "\n}\n";
    {
        std::ofstream stream(target / "manifest.json", std::ios::binary);
        if (!stream) {
            throw factor_data_error{"could not create research manifest"};
        }
        stream << manifest.str();
        finish_output_stream(stream, "could not write research manifest");
    }
    exported.files.push_back("manifest.json");
    cleanup.release();
    return exported;
}

research_export_result export_factor_research(
    const research_result& result, const std::string& directory,
    const bool charts) {
    return research_export_detail::export_factor_research_with_writer(
        result, directory, charts, write_table);
}


std::vector<research_correlation_row> research_factor_correlations(
    const std::span<const research_factor_panel> factors,
    const int min_assets) {
    if (min_assets < 1) {
        throw factor_data_error{
            "min_assets must be a positive integer"};
    }
    if (factors.empty()) {
        return {};
    }
    std::set<std::string> names;
    std::vector<std::map<timestamp, std::map<std::string, double>>> panels;
    panels.reserve(factors.size());
    for (const auto& factor : factors) {
        if (factor.name.empty() || !names.insert(factor.name).second) {
            throw factor_data_error{
                "factor names must be non-empty and unique"};
        }
        std::map<timestamp, std::map<std::string, double>> by_date;
        for (std::size_t row = 0; row < factor.values.dates.size(); ++row) {
            if (!std::isfinite(factor.values.values[row])) {
                continue;
            }
            by_date[factor.values.dates[row]][factor.values.assets[row]] =
                factor.values.values[row];
        }
        panels.push_back(std::move(by_date));
    }
    std::vector<research_correlation_row> result;
    for (std::size_t left = 0; left < factors.size(); ++left) {
        for (std::size_t right = left + 1U; right < factors.size(); ++right) {
            std::vector<double> correlations;
            const auto& left_panel = panels[left];
            const auto& right_panel = panels[right];
            for (const auto& [date, left_values] : left_panel) {
                const auto found = right_panel.find(date);
                if (found == right_panel.end()) {
                    continue;
                }
                const auto& right_values = found->second;
                std::vector<double> x_values;
                std::vector<double> y_values;
                for (const auto& [asset, value] : left_values) {
                    const auto other = right_values.find(asset);
                    if (other == right_values.end()) {
                        continue;
                    }
                    x_values.push_back(value);
                    y_values.push_back(other->second);
                }
                if (x_values.size() <
                    static_cast<std::size_t>(min_assets)) {
                    continue;
                }
                const double x_mean =
                    std::accumulate(x_values.begin(), x_values.end(), 0.0) /
                    static_cast<double>(x_values.size());
                const double y_mean =
                    std::accumulate(y_values.begin(), y_values.end(), 0.0) /
                    static_cast<double>(y_values.size());
                double xx = 0.0;
                double yy = 0.0;
                double xy = 0.0;
                for (std::size_t index = 0; index < x_values.size();
                     ++index) {
                    const double dx = x_values[index] - x_mean;
                    const double dy = y_values[index] - y_mean;
                    xx += dx * dx;
                    yy += dy * dy;
                    xy += dx * dy;
                }
                if (xx <= 0.0 || yy <= 0.0) {
                    continue;
                }
                const double value = xy / std::sqrt(xx * yy);
                if (std::isfinite(value)) {
                    correlations.push_back(value);
                }
            }
            research_correlation_row row;
            row.left = factors[left].name;
            row.right = factors[right].name;
            row.n_dates = correlations.size();
            if (!correlations.empty()) {
                const double mean =
                    std::accumulate(correlations.begin(),
                                    correlations.end(), 0.0) /
                    static_cast<double>(correlations.size());
                row.mean_correlation = mean;
                if (correlations.size() > 1U) {
                    double variance = 0.0;
                    for (const double value : correlations) {
                        variance += (value - mean) * (value - mean);
                    }
                    row.std_correlation =
                        std::sqrt(variance /
                                  static_cast<double>(correlations.size() -
                                                      1U));
                } else {
                    row.std_correlation = nan;
                }
            } else {
                row.mean_correlation = nan;
                row.std_correlation = nan;
            }
            result.push_back(std::move(row));
        }
    }
    return result;
}

namespace {

struct ic_statistics {
    double mean = nan;
    double icir = nan;
    std::size_t n_dates = 0U;
};

ic_statistics label_statistics(const research_result& result,
                               const std::string& label) {
    ic_statistics statistics;
    const auto found =
        std::find(result.information_coefficient.labels.begin(),
                  result.information_coefficient.labels.end(), label);
    if (found == result.information_coefficient.labels.end()) {
        return statistics;
    }
    const std::size_t column = static_cast<std::size_t>(
        found - result.information_coefficient.labels.begin());
    const std::size_t labels = result.information_coefficient.labels.size();
    std::vector<double> values;
    for (std::size_t date = 0;
         date < result.information_coefficient.dates.size(); ++date) {
        const double value =
            result.information_coefficient.values[date * labels + column];
        if (std::isfinite(value)) {
            values.push_back(value);
        }
    }
    statistics.n_dates = values.size();
    if (values.empty()) {
        return statistics;
    }
    statistics.mean =
        std::accumulate(values.begin(), values.end(), 0.0) /
        static_cast<double>(values.size());
    if (values.size() > 1U) {
        double variance = 0.0;
        for (const double value : values) {
            variance += (value - statistics.mean) *
                        (value - statistics.mean);
        }
        const double deviation = std::sqrt(
            variance / static_cast<double>(values.size() - 1U));
        if (deviation > 1e-15) {
            statistics.icir = statistics.mean / deviation;
        }
    }
    return statistics;
}

}  // namespace

std::vector<research_comparison_row> research_factor_comparisons(
    const std::span<const named_research_result> results,
    const std::map<std::string, std::pair<std::string, std::string>>&
        comparisons) {
    std::map<std::string, const research_result*> by_name;
    for (const auto& entry : results) {
        if (entry.name.empty() ||
            !by_name.emplace(entry.name, &entry.result).second) {
            throw factor_data_error{
                "research result names must be non-empty and unique"};
        }
    }
    std::vector<research_comparison_row> rows;
    for (const auto& [name, pair] : comparisons) {
        const auto left = by_name.find(pair.first);
        const auto right = by_name.find(pair.second);
        if (left == by_name.end() || right == by_name.end()) {
            throw factor_data_error{
                "comparison references an unknown factor"};
        }
        std::vector<std::string> labels =
            left->second->information_coefficient.labels;
        for (const auto& label : right->second->information_coefficient.labels) {
            if (std::find(labels.begin(), labels.end(), label) ==
                labels.end()) {
                labels.push_back(label);
            }
        }
        for (const auto& label : labels) {
            const auto left_stats = label_statistics(*left->second, label);
            const auto right_stats = label_statistics(*right->second, label);
            research_comparison_row row;
            row.comparison = name;
            row.left = pair.first;
            row.right = pair.second;
            row.period = label;
            row.mean_ic_left = left_stats.mean;
            row.mean_ic_right = right_stats.mean;
            row.icir_left = left_stats.icir;
            row.icir_right = right_stats.icir;
            row.n_dates_left = left_stats.n_dates;
            row.n_dates_right = right_stats.n_dates;
            row.mean_ic = std::isfinite(left_stats.mean) &&
                                  std::isfinite(right_stats.mean)
                              ? left_stats.mean - right_stats.mean
                              : nan;
            row.icir = std::isfinite(left_stats.icir) &&
                               std::isfinite(right_stats.icir)
                           ? left_stats.icir - right_stats.icir
                           : nan;
            rows.push_back(std::move(row));
        }
    }
    return rows;
}

}  // namespace fincore
