#include <fincore/factor/analysis.hpp>

#include <fincore/detail/digest.hpp>
#include <fincore/factor/exceptions.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fincore {
namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();
constexpr std::int64_t day_ns = 86'400'000'000'000LL;

struct civil_date {
    int year;
    unsigned month;
    unsigned day;
};

// Howard Hinnant's civil-from-days algorithm.
civil_date civil_from_days(std::int64_t days) {
    days += 719'468;
    const std::int64_t era = (days >= 0 ? days : days - 146'096) / 146'097;
    const auto doe = static_cast<unsigned>(days - era * 146'097);
    const unsigned yoe =
        (doe - doe / 1'460 + doe / 36'524 - doe / 146'096) / 365;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    const unsigned day = doy - (153 * mp + 2) / 5 + 1;
    const unsigned month = mp < 10 ? mp + 3 : mp - 9;
    return {static_cast<int>(yoe) + static_cast<int>(era) * 400 +
                (month <= 2 ? 1 : 0),
            month, day};
}

bool leap_year(const int year) {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

unsigned days_in_month(const int year, const unsigned month) {
    static constexpr unsigned lengths[] = {31, 28, 31, 30, 31, 30,
                                           31, 31, 30, 31, 30, 31};
    if (month == 2 && leap_year(year)) {
        return 29;
    }
    return lengths[month - 1];
}

std::int64_t days_from_civil(int year, unsigned month, unsigned day) {
    year -= month <= 2 ? 1 : 0;
    const std::int64_t era = (year >= 0 ? year : year - 399) / 400;
    const auto yoe = static_cast<unsigned>(year - era * 400);
    const unsigned doy =
        (153 * (month > 2 ? month - 3 : month + 9) + 2) / 5 + day - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146'097 + static_cast<std::int64_t>(doe) - 719'468;
}

timestamp month_end_of(const timestamp value) {
    const civil_date date =
        civil_from_days(static_cast<std::int64_t>(value / day_ns));
    const unsigned last = days_in_month(date.year, date.month);
    return static_cast<timestamp>(
        days_from_civil(date.year, date.month, last) * day_ns);
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
                "analysis period labels must contain whole day components"};
        }
        ++index;
        if (component <= 0) {
            throw factor_data_error{
                "analysis period labels must contain whole day components"};
        }
        total += component;
    }
    if (total <= 0) {
        throw factor_data_error{
            "analysis period labels must contain whole day components"};
    }
    return total;
}

std::vector<std::string> selected_periods(
    const prepared_factor_data& data, const std::vector<std::string>& periods) {
    if (periods.empty()) {
        return data.forward_labels;
    }
    std::set<std::string> seen;
    for (const auto& period : periods) {
        if (period.empty() || !seen.insert(period).second) {
            throw factor_data_error{
                "periods must be nonempty and duplicate-free"};
        }
        if (std::find(data.forward_labels.begin(), data.forward_labels.end(),
                      period) == data.forward_labels.end()) {
            throw factor_data_error{"unknown forward period: " + period};
        }
    }
    return periods;
}

prepared_factor_data select_periods(const prepared_factor_data& data,
                                    const std::vector<std::string>& periods) {
    prepared_factor_data selected;
    selected.forward_labels = periods;
    selected.loss_report = data.loss_report;
    std::vector<std::size_t> positions;
    positions.reserve(periods.size());
    for (const auto& period : periods) {
        const auto found = std::find(data.forward_labels.begin(),
                                     data.forward_labels.end(), period);
        positions.push_back(
            static_cast<std::size_t>(found - data.forward_labels.begin()));
    }
    selected.rows.reserve(data.rows.size());
    for (const auto& row : data.rows) {
        if (row.forward_returns.size() != data.forward_labels.size()) {
            throw factor_data_error{
                "prepared rows must match the forward label count"};
        }
        factor_row copy = row;
        copy.forward_returns.clear();
        copy.forward_returns.reserve(positions.size());
        for (const std::size_t position : positions) {
            copy.forward_returns.push_back(row.forward_returns[position]);
        }
        selected.rows.push_back(std::move(copy));
    }
    return selected;
}

void validate_config(const factor_analysis_config& config) {
    if (config.turnover_periods.empty()) {
        throw factor_data_error{
            "turnover_periods must contain at least one positive lag"};
    }
    std::set<int> lags;
    for (const int lag : config.turnover_periods) {
        if (lag <= 0 || !lags.insert(lag).second) {
            throw factor_data_error{
                "turnover_periods must be positive and duplicate-free"};
        }
    }
    if (config.time_aggregation.empty()) {
        throw factor_data_error{
            "time_aggregation must contain at least one frequency"};
    }
    bool monthly = false;
    for (const auto& frequency : config.time_aggregation) {
        if (frequency != "M" && frequency != "ME") {
            throw factor_data_error{
                "only monthly time aggregation is implemented natively"};
        }
        if (monthly) {
            throw factor_data_error{
                "time_aggregation must be duplicate-free"};
        }
        monthly = true;
    }
    if (config.portfolio_benchmark_period.empty()) {
        throw factor_data_error{
            "portfolio_benchmark_period must not be empty"};
    }
}

std::vector<factor_quantile_statistics> quantile_statistics(
    const prepared_factor_data& data) {
    std::map<int, std::vector<double>> values;
    for (const auto& row : data.rows) {
        if (std::isfinite(row.factor)) {
            values[row.quantile].push_back(row.factor);
        }
    }
    std::size_t total = 0U;
    for (const auto& entry : values) {
        total += entry.second.size();
    }
    std::vector<factor_quantile_statistics> result;
    for (const auto& [quantile, entries] : values) {
        if (entries.empty()) {
            continue;
        }
        factor_quantile_statistics statistics;
        statistics.quantile = quantile;
        statistics.minimum = entries.front();
        statistics.maximum = entries.front();
        double sum = 0.0;
        for (const double value : entries) {
            statistics.minimum = std::min(statistics.minimum, value);
            statistics.maximum = std::max(statistics.maximum, value);
            sum += value;
        }
        statistics.count = entries.size();
        statistics.mean = sum / static_cast<double>(entries.size());
        if (entries.size() > 1U) {
            double variance = 0.0;
            for (const double value : entries) {
                variance += (value - statistics.mean) *
                            (value - statistics.mean);
            }
            statistics.stddev =
                std::sqrt(variance / static_cast<double>(entries.size() - 1U));
        } else {
            statistics.stddev = nan;
        }
        if (total > 0U) {
            statistics.count_percent =
                static_cast<double>(entries.size()) /
                static_cast<double>(total) * 100.0;
        }
        result.push_back(statistics);
    }
    return result;
}

std::vector<factor_quantile_curve> quantile_cumulative_curves(
    const quantile_date_return_table& table,
    const std::vector<std::string>& periods) {
    std::vector<factor_quantile_curve> result;
    if (periods.empty() || table.dates.empty() || table.quantiles.empty()) {
        return result;
    }
    const auto base_days = period_days(periods.front());
    const std::size_t label_count = table.labels.size();
    for (std::size_t label = 0; label < label_count; ++label) {
        factor_quantile_curve curve;
        curve.period = table.labels[label];
        curve.dates = table.dates;
        curve.quantiles.assign(table.quantiles.rbegin(),
                               table.quantiles.rend());
        const auto span = static_cast<double>(base_days) /
                          static_cast<double>(period_days(curve.period));
        curve.values.assign(table.dates.size() * table.quantiles.size(), 0.0);
        for (std::size_t q = 0; q < table.quantiles.size(); ++q) {
            const std::size_t quantile_index =
                table.quantiles.size() - 1U - q;
            double growth = 1.0;
            for (std::size_t date = 0; date < table.dates.size(); ++date) {
                const double mean =
                    table.means[(date * table.quantiles.size() +
                                 quantile_index) *
                                    label_count +
                                label];
                const double converted =
                    std::isfinite(mean) ? std::pow(1.0 + mean, span) - 1.0
                                        : 0.0;
                growth *= 1.0 + converted;
                curve.values[date * table.quantiles.size() + q] = growth;
            }
        }
        result.push_back(std::move(curve));
    }
    return result;
}

factor_time_information_table monthly_information(
    const factor_ic_table& information, const std::string& frequency) {
    factor_time_information_table table;
    table.frequency = frequency;
    table.labels = information.labels;
    std::map<timestamp, std::vector<double>> sums;
    std::map<timestamp, std::vector<std::size_t>> counts;
    for (std::size_t date = 0; date < information.dates.size(); ++date) {
        const timestamp month = month_end_of(information.dates[date]);
        auto& row = sums[month];
        auto& row_counts = counts[month];
        row.resize(information.labels.size(), 0.0);
        row_counts.resize(information.labels.size(), 0U);
        for (std::size_t label = 0; label < information.labels.size();
             ++label) {
            const double value =
                information.values[date * information.labels.size() + label];
            if (std::isfinite(value)) {
                row[label] += value;
                ++row_counts[label];
            }
        }
    }
    for (const auto& [month, row] : sums) {
        table.dates.push_back(month);
        for (std::size_t label = 0; label < row.size(); ++label) {
            const std::size_t count = counts[month][label];
            table.means.push_back(count > 0U
                                      ? row[label] /
                                            static_cast<double>(count)
                                      : nan);
        }
    }
    return table;
}

std::string fingerprint_of(const prepared_factor_data& data,
                           const factor_analysis_config& config,
                           const std::vector<std::string>& periods,
                           const dataframe* event_returns,
                           const int event_before, const int event_after) {
    std::vector<std::uint8_t> payload;
    const auto append_u64 = [&payload](const std::uint64_t value) {
        for (int shift = 56; shift >= 0; shift -= 8) {
            payload.push_back(
                static_cast<std::uint8_t>((value >> shift) & 0xffU));
        }
    };
    const auto append_double = [&append_u64](const double value) {
        std::uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        append_u64(bits);
    };
    const auto append_text = [&payload](const std::string& text) {
        for (const char character : text) {
            payload.push_back(static_cast<std::uint8_t>(character));
        }
        payload.push_back(0U);
    };
    append_u64(config.long_short ? 1U : 0U);
    append_u64(config.group_neutral ? 1U : 0U);
    append_u64(config.equal_weight ? 1U : 0U);
    append_u64(config.include_portfolio_inputs ? 1U : 0U);
    append_text(config.portfolio_benchmark_period);
    append_u64(periods.size());
    for (const auto& period : periods) {
        append_text(period);
    }
    append_u64(config.turnover_periods.size());
    for (const int lag : config.turnover_periods) {
        append_u64(static_cast<std::uint64_t>(lag));
    }
    append_u64(config.time_aggregation.size());
    for (const auto& frequency : config.time_aggregation) {
        append_text(frequency);
    }
    append_u64(data.rows.size());
    for (const auto& row : data.rows) {
        append_u64(static_cast<std::uint64_t>(row.date));
        append_text(row.asset);
        append_double(row.factor);
        append_u64(static_cast<std::uint64_t>(row.quantile));
        append_text(row.has_group ? row.group : std::string{});
        for (const double value : row.forward_returns) {
            append_double(value);
        }
    }
    append_u64(event_returns != nullptr ? 1U : 0U);
    if (event_returns != nullptr) {
        append_u64(static_cast<std::uint64_t>(event_before));
        append_u64(static_cast<std::uint64_t>(event_after));
        append_u64(event_returns->get_index().size());
        for (std::size_t row = 0; row < event_returns->get_index().size();
             ++row) {
            append_u64(static_cast<std::uint64_t>(
                event_returns->get_index()[row]));
            for (const auto& info : event_returns->get_columns_info<double>()) {
                if (std::get<2>(info) != typeid(double)) {
                    continue;
                }
                const std::string name{std::get<0>(info).c_str()};
                append_text(name);
                append_double(
                    event_returns->get_column<double>(name.c_str())[row]);
            }
        }
    }
    return detail::sha256_hex(payload);
}

}  // namespace

static factor_analysis_model analyze_factor_impl(
    const prepared_factor_data& factor_data,
    const factor_analysis_config& config, const dataframe* event_returns,
    const int event_before, const int event_after) {
    if (factor_data.rows.empty()) {
        throw factor_data_error{
            "factor_data must contain at least one prepared observation"};
    }
    if (factor_data.forward_labels.empty()) {
        throw factor_data_error{
            "factor_data must contain at least one forward-return label"};
    }
    validate_config(config);
    const auto periods = selected_periods(factor_data, config.periods);
    const auto analysis_data = select_periods(factor_data, periods);

    factor_analysis_model model;
    model.factor_data = factor_data;
    model.forward_periods = periods;
    model.quantile_statistics = quantile_statistics(analysis_data);
    model.information_coefficient = factor_information_coefficient(
        analysis_data, config.group_neutral);
    model.mean_information_coefficient = mean_information_coefficient(
        analysis_data, config.group_neutral);
    model.factor_returns = factor_returns(
        analysis_data, config.long_short, config.group_neutral,
        config.equal_weight);
    model.factor_weights = factor_weights(
        analysis_data, config.long_short, config.group_neutral,
        config.equal_weight);
    model.mean_returns_by_quantile = mean_return_by_quantile(
        analysis_data, false, false, config.long_short,
        config.group_neutral);
    model.mean_returns_by_date = mean_return_by_quantile_by_date(
        analysis_data, config.long_short, config.group_neutral);
    model.alpha_beta = factor_alpha_beta(
        analysis_data, config.long_short, config.group_neutral,
        config.equal_weight);

    std::set<int> quantiles;
    for (const auto& row : analysis_data.rows) {
        quantiles.insert(row.quantile);
    }
    if (quantiles.size() >= 2U) {
        const auto spread = mean_return_spread(
            model.mean_returns_by_quantile, *quantiles.rbegin(),
            *quantiles.begin());
        for (std::size_t label = 0; label < spread.labels.size(); ++label) {
            model.mean_return_spread.push_back(
                {spread.labels[label], spread.spreads[label],
                 spread.standard_errors[label]});
        }
    }

    for (const int lag : config.turnover_periods) {
        for (const int quantile : quantiles) {
            model.quantile_turnover.push_back(
                {quantile, lag,
                 quantile_turnover(analysis_data, quantile, lag)});
        }
        model.rank_autocorrelation.push_back(
            {lag, factor_rank_autocorrelation(analysis_data, lag)});
    }

    for (const auto& period : periods) {
        model.factor_cumulative_returns.push_back(
            {period,
             factor_cumulative_returns(
                 analysis_data, period, config.long_short,
                 config.group_neutral, config.equal_weight)});
        model.factor_positions.push_back(
            {period,
             factor_positions(analysis_data, period, config.long_short,
                              config.group_neutral, config.equal_weight)});
    }
    model.quantile_cumulative_returns =
        quantile_cumulative_curves(model.mean_returns_by_date, periods);
    model.time_aggregated_information.push_back(
        monthly_information(model.information_coefficient, "M"));

    if (event_returns != nullptr) {
        if (event_before < 0 || event_after < 0) {
            throw factor_data_error{
                "event_before and event_after must be non-negative integers"};
        }
        if (config.group_neutral) {
            throw factor_data_error{
                "group-neutral event analysis is not supported natively"};
        }
        if (event_returns->get_index().empty()) {
            throw factor_data_error{
                "event_returns must contain at least one row"};
        }
        factor_series quantile_events;
        for (const auto& row : analysis_data.rows) {
            quantile_events.dates.push_back(row.date);
            quantile_events.assets.push_back(row.asset);
            quantile_events.values.push_back(
                static_cast<double>(row.quantile));
        }
        const auto windows = common_start_returns(
            quantile_events, *event_returns, event_before, event_after, true,
            true, config.long_short ? &quantile_events : nullptr);
        factor_event_model event_model;
        event_model.offsets = windows.offsets;
        event_model.event_dates = windows.event_dates;
        std::map<int, std::vector<double>> by_offset;
        std::vector<event_window_cell> ordered = windows.cells;
        std::sort(ordered.begin(), ordered.end(),
                  [](const event_window_cell& left,
                     const event_window_cell& right) {
                      if (left.offset != right.offset) {
                          return left.offset < right.offset;
                      }
                      return left.event_date < right.event_date;
                  });
        for (const auto& cell : ordered) {
            event_model.return_distribution.push_back(cell.value);
            by_offset[cell.offset].push_back(cell.value);
        }
        for (const int offset : event_model.offsets) {
            const auto found = by_offset.find(offset);
            if (found == by_offset.end() || found->second.empty()) {
                event_model.mean_returns.push_back(nan);
                continue;
            }
            const auto& entries = found->second;
            event_model.mean_returns.push_back(
                std::accumulate(entries.begin(), entries.end(), 0.0) /
                static_cast<double>(entries.size()));
        }
        event_model.quantile_average_returns =
            average_cumulative_return_by_quantile(
                analysis_data, *event_returns, event_before, event_after,
                config.long_short);
        model.event_analysis = std::move(event_model);
        model.event_input_snapshot = *event_returns;
    }

    if (config.include_portfolio_inputs) {
        model.portfolio_inputs = build_factor_portfolio_inputs(
            analysis_data, periods.front(), config.long_short,
            config.group_neutral, config.equal_weight);
    }

    model.result_fingerprint = fingerprint_of(
        factor_data, config, periods, event_returns, event_before,
        event_after);
    return model;
}

factor_analysis_model analyze_factor(const prepared_factor_data& factor_data,
                                     const factor_analysis_config& config) {
    return analyze_factor_impl(factor_data, config, nullptr, 0, 0);
}

factor_analysis_model analyze_factor(
    const prepared_factor_data& factor_data,
    const factor_analysis_config& config, const dataframe& event_returns,
    const int event_before, const int event_after) {
    return analyze_factor_impl(factor_data, config, &event_returns,
                               event_before, event_after);
}

ic_inference_result factor_model_inference(const factor_analysis_model& model,
                                           const double alpha) {
    return information_coefficient_inference(model.information_coefficient,
                                             alpha);
}

}  // namespace fincore
