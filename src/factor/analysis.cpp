#include <fincore/factor/analysis.hpp>

#include <fincore/detail/digest.hpp>
#include <fincore/detail/factor_compute.hpp>
#include <fincore/factor/exceptions.hpp>

#include <algorithm>
#include <array>
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
            const int digit = label[index] - '0';
            if (component > (std::numeric_limits<int>::max() - digit) / 10) {
                throw factor_data_error{
                    "analysis period labels must fit in a positive day count"};
            }
            component = component * 10 + digit;
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
        if (total > std::numeric_limits<int>::max() - component) {
            throw factor_data_error{
                "analysis period labels must fit in a positive day count"};
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
    using month_group = std::pair<timestamp, std::string>;
    std::map<month_group, std::vector<double>> sums;
    std::map<month_group, std::vector<std::size_t>> counts;
    for (std::size_t date = 0; date < information.dates.size(); ++date) {
        const timestamp month = month_end_of(information.dates[date]);
        const std::string group = information.groups.empty()
                                      ? std::string{}
                                      : information.groups[date];
        const month_group key{month, group};
        auto& row = sums[key];
        auto& row_counts = counts[key];
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
    for (const auto& [key, row] : sums) {
        const auto& [month, group] = key;
        table.dates.push_back(month);
        if (!information.groups.empty()) {
            table.groups.push_back(group);
        }
        for (std::size_t label = 0; label < row.size(); ++label) {
            const std::size_t count = counts[key][label];
            table.means.push_back(count > 0U
                                      ? row[label] /
                                            static_cast<double>(count)
                                      : nan);
        }
    }
    return table;
}

quantile_return_table select_group_rows(
    const quantile_return_table& table, const std::string& group) {
    quantile_return_table selected;
    selected.labels = table.labels;
    const std::size_t labels = table.labels.size();
    for (std::size_t row = 0; row < table.quantiles.size(); ++row) {
        if (table.groups[row] != group) {
            continue;
        }
        selected.quantiles.push_back(table.quantiles[row]);
        selected.groups.push_back(group);
        for (std::size_t label = 0; label < labels; ++label) {
            const std::size_t position = row * labels + label;
            selected.means.push_back(table.means[position]);
            selected.standard_errors.push_back(
                table.standard_errors[position]);
        }
    }
    return selected;
}

quantile_date_return_table select_group_rows(
    const quantile_date_return_table& table, const std::string& group) {
    quantile_date_return_table selected;
    selected.quantiles = table.quantiles;
    selected.labels = table.labels;
    const std::size_t row_width = table.quantiles.size() * table.labels.size();
    for (std::size_t row = 0; row < table.dates.size(); ++row) {
        if (table.groups[row] != group) {
            continue;
        }
        selected.dates.push_back(table.dates[row]);
        selected.groups.push_back(group);
        const std::size_t begin = row * row_width;
        selected.means.insert(selected.means.end(),
                              table.means.begin() +
                                  static_cast<std::ptrdiff_t>(begin),
                              table.means.begin() +
                                  static_cast<std::ptrdiff_t>(begin + row_width));
        selected.standard_errors.insert(
            selected.standard_errors.end(),
            table.standard_errors.begin() +
                static_cast<std::ptrdiff_t>(begin),
            table.standard_errors.begin() +
                static_cast<std::ptrdiff_t>(begin + row_width));
    }
    return selected;
}

factor_ic_table select_group_rows(const factor_ic_table& table,
                                  const std::string& group) {
    factor_ic_table selected;
    selected.labels = table.labels;
    const std::size_t labels = table.labels.size();
    for (std::size_t row = 0; row < table.dates.size(); ++row) {
        if (table.groups[row] != group) {
            continue;
        }
        selected.dates.push_back(table.dates[row]);
        selected.groups.push_back(group);
        const auto begin = table.values.begin() +
                           static_cast<std::ptrdiff_t>(row * labels);
        selected.values.insert(selected.values.end(), begin,
                               begin + static_cast<std::ptrdiff_t>(labels));
    }
    return selected;
}

factor_ic_summary select_group_rows(const factor_ic_summary& summary,
                                    const std::string& group) {
    factor_ic_summary selected;
    selected.labels = summary.labels;
    selected.groups.push_back(group);
    const auto found = std::find(summary.groups.begin(), summary.groups.end(),
                                 group);
    if (found == summary.groups.end()) {
        selected.means.assign(summary.labels.size(), nan);
        return selected;
    }
    const std::size_t row =
        static_cast<std::size_t>(found - summary.groups.begin());
    const auto begin = summary.means.begin() +
                       static_cast<std::ptrdiff_t>(row * summary.labels.size());
    selected.means.insert(selected.means.end(), begin,
                          begin + static_cast<std::ptrdiff_t>(summary.labels.size()));
    return selected;
}

std::vector<factor_spread_row> select_group_rows(
    const quantile_spread_result& spread, const std::string& group) {
    std::vector<factor_spread_row> selected;
    for (std::size_t row = 0; row < spread.labels.size(); ++row) {
        if (spread.groups[row] == group) {
            selected.push_back({spread.labels[row], spread.spreads[row],
                                spread.standard_errors[row], group});
        }
    }
    return selected;
}

factor_time_information_table select_group_rows(
    const factor_time_information_table& table, const std::string& group) {
    factor_time_information_table selected;
    selected.frequency = table.frequency;
    selected.labels = table.labels;
    const std::size_t labels = table.labels.size();
    for (std::size_t row = 0; row < table.dates.size(); ++row) {
        if (table.groups[row] != group) {
            continue;
        }
        selected.dates.push_back(table.dates[row]);
        selected.groups.push_back(group);
        const auto begin = table.means.begin() +
                           static_cast<std::ptrdiff_t>(row * labels);
        selected.means.insert(selected.means.end(), begin,
                              begin + static_cast<std::ptrdiff_t>(labels));
    }
    return selected;
}

void validate_event_input(const dataframe& event_returns) {
    const auto [row_count, column_count] = event_returns.shape();
    const auto columns = event_returns.get_columns_info<double>();
    if (row_count == 0U || column_count == 0U || columns.empty() ||
        event_returns.get_index().size() != row_count ||
        columns.size() != column_count) {
        throw factor_data_error{
            "event_returns must contain a rectangular table of double columns"};
    }
    for (const auto& info : columns) {
        if (std::get<1>(info) != row_count) {
            throw factor_data_error{
                "event_returns double columns must match the index row count"};
        }
    }
}

class fingerprint_sink {
public:
    void u64(const std::uint64_t value) {
        std::array<std::uint8_t, 8> bytes{};
        for (int index = 0; index < 8; ++index) {
            bytes[static_cast<std::size_t>(index)] =
                static_cast<std::uint8_t>(
                    (value >> (56 - 8 * index)) & 0xffU);
        }
        write(std::span<const std::uint8_t>(bytes));
    }

    void i64(const std::int64_t value) {
        u64(static_cast<std::uint64_t>(value));
    }

    void size(const std::size_t value) {
        u64(static_cast<std::uint64_t>(value));
    }

    void double_value(const double value) {
        std::uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        u64(bits);
    }

    void text(const std::string_view value) {
        size(value.size());
        write(std::span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(value.data()),
            value.size()));
    }

    void write(const std::span<const std::uint8_t> data) {
        if (data.size() >= buffer_.size()) {
            flush();
            hasher_.update(data);
            return;
        }
        if (used_ + data.size() > buffer_.size()) {
            flush();
        }
        std::memcpy(buffer_.data() + used_, data.data(), data.size());
        used_ += data.size();
    }

    std::string finish() {
        flush();
        return hasher_.finish_hex();
    }

private:
    void flush() {
        if (used_ > 0U) {
            hasher_.update(
                std::span<const std::uint8_t>(buffer_.data(), used_));
            used_ = 0U;
        }
    }

    detail::sha256_hasher hasher_;
    std::array<std::uint8_t, 65536> buffer_{};
    std::size_t used_ = 0U;
};

std::string fingerprint_of(const prepared_factor_data& data,
                           const factor_analysis_config& config,
                           const std::vector<std::string>& selected_periods,
                           const dataframe* event_returns,
                           const int event_before, const int event_after) {
    fingerprint_sink sink;
    const auto append_u64 = [&sink](const std::uint64_t value) {
        sink.u64(value);
    };
    const auto append_i64 = [&sink](const std::int64_t value) {
        sink.i64(value);
    };
    const auto append_size = [&sink](const std::size_t value) {
        sink.size(value);
    };
    const auto append_double = [&sink](const double value) {
        sink.double_value(value);
    };
    const auto append_text = [&sink](const std::string_view text) {
        sink.text(text);
    };
    static_assert(sizeof(double) == sizeof(std::uint64_t));
    append_text("fincore.factor-analysis.result-fingerprint");
    append_u64(factor_analysis_fingerprint_version);
    append_text("event-input-policy/double-columns-only/v1");

    append_u64(config.long_short ? 1U : 0U);
    append_u64(config.group_neutral ? 1U : 0U);
    append_u64(config.equal_weight ? 1U : 0U);
    append_u64(config.by_group ? 1U : 0U);
    append_u64(config.include_portfolio_inputs ? 1U : 0U);
    append_text(config.portfolio_benchmark_period);
    append_size(config.periods.size());
    for (const auto& period : config.periods) {
        append_text(period);
    }
    append_size(selected_periods.size());
    for (const auto& period : selected_periods) {
        append_text(period);
    }
    append_size(config.turnover_periods.size());
    for (const int lag : config.turnover_periods) {
        append_i64(lag);
    }
    append_size(config.time_aggregation.size());
    for (const auto& frequency : config.time_aggregation) {
        append_text(frequency);
    }

    append_size(data.forward_labels.size());
    for (const auto& label : data.forward_labels) {
        append_text(label);
    }
    append_size(data.loss_report.input_count);
    append_size(data.loss_report.finite_factor_count);
    append_size(data.loss_report.forward_returns_count);
    append_size(data.loss_report.binning_count);
    append_double(data.loss_report.factor_input_loss);
    append_double(data.loss_report.forward_returns_loss);
    append_double(data.loss_report.binning_loss);
    append_double(data.loss_report.total_loss);

    append_size(data.rows.size());
    for (const auto& row : data.rows) {
        append_i64(row.date);
        append_text(row.asset);
        append_double(row.factor);
        append_i64(row.quantile);
        append_u64(row.has_group ? 1U : 0U);
        append_text(row.group);
        append_size(row.forward_returns.size());
        for (const double value : row.forward_returns) {
            append_double(value);
        }
    }
    append_u64(event_returns != nullptr ? 1U : 0U);

    if (event_returns != nullptr) {
        const auto [row_count, column_count] = event_returns->shape();
        const auto columns = event_returns->get_columns_info<double>();
        append_i64(event_before);
        append_i64(event_after);
        append_size(row_count);
        append_size(column_count);
        append_size(event_returns->get_index().size());
        for (const timestamp date : event_returns->get_index()) {
            append_i64(date);
        }
        append_size(columns.size());
        for (const auto& info : columns) {
            const std::string name{std::get<0>(info).c_str()};
            append_text("double");
            append_text(name);
            append_size(std::get<1>(info));
            for (const double value :
                 event_returns->get_column<double>(name.c_str())) {
                append_double(value);
            }
        }
    }
    return sink.finish();
}

}  // namespace

namespace detail {

std::string factor_fingerprint_for_benchmark(
    const prepared_factor_data& data,
    const factor_analysis_config& config) {
    const auto periods = selected_periods(data, config.periods);
    return fingerprint_of(data, config, periods, nullptr, 0, 0);
}

}  // namespace detail

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
    if (event_returns != nullptr) {
        validate_event_input(*event_returns);
    }
    const auto periods = selected_periods(factor_data, config.periods);
    const auto analysis_data = select_periods(factor_data, periods);

    factor_analysis_model model;
    model.factor_data = factor_data;
    model.forward_periods = periods;
    model.config = config;
    model.result_fingerprint_version =
        factor_analysis_fingerprint_version;
    model.quantile_statistics = quantile_statistics(analysis_data);
    model.information_coefficient = factor_information_coefficient(
        analysis_data, config.group_neutral);
    model.mean_information_coefficient =
        detail::summarize_information_coefficient(
            model.information_coefficient);
    if (config.group_neutral) {
        // The source summary IC intentionally ignores group adjustment.
        model.summary_information_coefficient =
            factor_information_coefficient(analysis_data, false, false);
    }
    model.factor_weights = factor_weights(
        analysis_data, config.long_short, config.group_neutral,
        config.equal_weight);
    model.factor_returns = detail::factor_returns_from_weights(
        analysis_data,
        std::span<const factor_weight_row>{model.factor_weights});
    auto quantile_views = detail::build_quantile_return_views(
        analysis_data, false, config.long_short, config.group_neutral, true,
        true);
    model.mean_returns_by_quantile = std::move(quantile_views.aggregate);
    model.mean_returns_by_date = std::move(quantile_views.by_date);
    model.alpha_beta = detail::factor_alpha_beta_from_returns(
        analysis_data, model.factor_returns);

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
             detail::factor_cumulative_returns_from_table(
                 model.factor_returns, period)});
        model.factor_positions.push_back(
            {period,
             factor_positions(analysis_data, period, config.long_short,
                              config.group_neutral, config.equal_weight, {},
                              {}, model.factor_weights)});
    }
    model.quantile_cumulative_returns =
        quantile_cumulative_curves(model.mean_returns_by_date, periods);
    for (const auto& frequency : config.time_aggregation) {
        model.time_aggregated_information.push_back(
            monthly_information(model.information_coefficient, frequency));
    }

    if (config.by_group) {
        std::set<std::string> group_names;
        for (const auto& row : analysis_data.rows) {
            if (!row.has_group) {
                throw factor_data_error{
                    "factor_data must contain groups when by_group=true"};
            }
            group_names.insert(row.group);
        }

        // These tables are computed over the full date universe before the
        // group rows are selected. In particular, ordinary long_short mode
        // demeans returns across all groups for each date.
        const auto grouped_views = detail::build_quantile_return_views(
            analysis_data, true, config.long_short, config.group_neutral,
            true, true);
        const auto& grouped_quantile_returns = grouped_views.aggregate;
        const auto& grouped_date_returns = grouped_views.by_date;
        const auto grouped_information = factor_information_coefficient(
            analysis_data, config.group_neutral, true);
        const auto grouped_mean_information =
            detail::summarize_information_coefficient(grouped_information);
        std::vector<factor_time_information_table> grouped_time_information;
        for (const auto& frequency : config.time_aggregation) {
            grouped_time_information.push_back(
                monthly_information(grouped_information, frequency));
        }
        quantile_spread_result grouped_spread;
        if (quantiles.size() >= 2U) {
            grouped_spread = mean_return_spread(
                grouped_quantile_returns, *quantiles.rbegin(),
                *quantiles.begin());
        }

        for (const auto& group : group_names) {
            factor_group_snapshot snapshot;
            snapshot.group = group;
            snapshot.mean_returns_by_quantile =
                select_group_rows(grouped_quantile_returns, group);
            snapshot.mean_returns_by_date =
                select_group_rows(grouped_date_returns, group);
            snapshot.mean_return_spread =
                select_group_rows(grouped_spread, group);
            snapshot.information_coefficient =
                select_group_rows(grouped_information, group);
            snapshot.mean_information_coefficient =
                select_group_rows(grouped_mean_information, group);
            for (const auto& information : grouped_time_information) {
                snapshot.time_aggregated_information.push_back(
                    select_group_rows(information, group));
            }
            model.group_snapshots.push_back(std::move(snapshot));
        }

        for (const auto& group : group_names) {
            prepared_factor_data group_data;
            group_data.forward_labels = analysis_data.forward_labels;
            group_data.loss_report = analysis_data.loss_report;
            for (const auto& row : analysis_data.rows) {
                if (row.has_group && row.group == group) {
                    group_data.rows.push_back(row);
                }
            }
            factor_group_analysis grouped;
            grouped.group = group;
            grouped.quantile_statistics = quantile_statistics(group_data);
            grouped.factor_weights = factor_weights(
                group_data, config.long_short, false, config.equal_weight);
            grouped.factor_returns = detail::factor_returns_from_weights(
                group_data,
                std::span<const factor_weight_row>{grouped.factor_weights});
            grouped.mean_returns_by_quantile = mean_return_by_quantile(
                group_data, false, false, config.long_short, false);
            grouped.information_coefficient =
                factor_information_coefficient(group_data, false);
            grouped.mean_information_coefficient =
                detail::summarize_information_coefficient(
                    grouped.information_coefficient);
            std::set<int> group_quantiles;
            for (const auto& row : group_data.rows) {
                group_quantiles.insert(row.quantile);
            }
            for (const int lag : config.turnover_periods) {
                for (const int quantile : group_quantiles) {
                    grouped.quantile_turnover.push_back(
                        {quantile, lag,
                         quantile_turnover(group_data, quantile, lag)});
                }
                grouped.rank_autocorrelation.push_back(
                    {lag, factor_rank_autocorrelation(group_data, lag)});
            }
            model.grouped_results.push_back(std::move(grouped));
        }
    }

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
        const auto position_entry = std::find_if(
            model.factor_positions.begin(), model.factor_positions.end(),
            [&periods](const factor_position_series& entry) {
                return entry.period == periods.front();
            });
        if (position_entry != model.factor_positions.end()) {
            model.portfolio_inputs =
                detail::build_factor_portfolio_inputs_from_outputs(
                    analysis_data, analysis_data, periods.front(),
                    model.factor_returns, position_entry->positions);
        } else {
            model.portfolio_inputs =
                detail::build_factor_portfolio_inputs_from_tables(
                    analysis_data, analysis_data, periods.front(),
                    config.long_short, config.group_neutral,
                    config.equal_weight, model.factor_returns,
                    std::span<const factor_weight_row>{model.factor_weights});
        }
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
