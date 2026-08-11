#include <fincore/metrics/returns.hpp>

#include <chrono>
#include <cmath>
#include <limits>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace fincore {
namespace {

double compound_wealth(const std::span<const double> returns) {
    double wealth = 1.0;
    for (const double value : returns) {
        wealth *= 1.0 + (std::isnan(value) ? 0.0 : value);
    }
    return wealth;
}

double compound_returns(const std::span<const double> returns) {
    return compound_wealth(returns) - 1.0;
}

double cumulative_value(const double wealth, const double starting_value) {
    return starting_value == 0.0 ? wealth - 1.0 : wealth * starting_value;
}

std::string_view legacy_column_or_throw(
    const std::initializer_list<std::string_view> legacy_column) {
    if (legacy_column.size() > 1U) {
        throw input_error{
            "legacy cumulative column list must contain at most one value"};
    }
    return legacy_column.size() == 0U ? std::string_view{} :
                                       *legacy_column.begin();
}

std::chrono::sys_days utc_day(const timestamp value) {
    using namespace std::chrono;

    const sys_time<nanoseconds> point{nanoseconds{value}};
    return floor<days>(point);
}

int calendar_year(const std::chrono::sys_days day_value) {
    return static_cast<int>(std::chrono::year_month_day{day_value}.year());
}

int calendar_month(const std::chrono::sys_days day_value) {
    return static_cast<int>(
        static_cast<unsigned>(std::chrono::year_month_day{day_value}.month()));
}

unsigned iso_weekday(const std::chrono::sys_days day_value) {
    return std::chrono::weekday{day_value}.iso_encoding();
}

int iso_week_number(const std::chrono::sys_days day_value) {
    using namespace std::chrono;

    const int weekday_number = static_cast<int>(iso_weekday(day_value));
    const sys_days thursday = day_value + days{4 - weekday_number};
    const int iso_year = calendar_year(thursday);
    const sys_days january_fourth{year{iso_year} / January / day{4}};
    const sys_days first_monday =
        january_fourth - days{static_cast<int>(iso_weekday(january_fourth)) - 1};
    return static_cast<int>((day_value - first_monday).count() / 7 + 1);
}

timestamp timestamp_for_day(const std::chrono::sys_days day_value) {
    const auto calendar_day = std::chrono::year_month_day{day_value};
    return utc_timestamp(static_cast<int>(calendar_day.year()),
                         static_cast<unsigned>(calendar_day.month()),
                         static_cast<unsigned>(calendar_day.day()));
}

timestamp canonical_weekly_bucket_start(const int calendar_year_value,
                                        const int iso_week) {
    using namespace std::chrono;

    const sys_days january_first{year{calendar_year_value} / January / day{1}};
    const sys_days december_thirty_first{
        year{calendar_year_value} / December / day{31}};
    for (sys_days candidate = january_first;
         candidate <= december_thirty_first; candidate += days{1}) {
        if (iso_week_number(candidate) != iso_week) {
            continue;
        }

        const sys_days monday =
            candidate - days{static_cast<int>(iso_weekday(candidate)) - 1};
        const sys_days bucket_day =
            monday < january_first ? january_first : monday;
        return timestamp_for_day(bucket_day);
    }

    throw input_error{"return aggregation week is invalid for calendar year"};
}

struct aggregation_key {
    int year;
    int period;

    bool operator<(const aggregation_key& other) const noexcept {
        return year < other.year || (year == other.year && period < other.period);
    }
};

struct aggregation_group {
    timestamp bucket_start;
    double wealth{1.0};
};

struct aggregation_bucket {
    aggregation_key key;
    timestamp start;
};

void validate_aggregation_period(const return_aggregation_period period) {
    switch (period) {
        case return_aggregation_period::weekly:
        case return_aggregation_period::monthly:
        case return_aggregation_period::quarterly:
        case return_aggregation_period::yearly:
            return;
        default:
            throw input_error{"return aggregation period is invalid"};
    }
}

void reject_reserved_aggregation_column(const std::string_view column) {
    if (column == return_aggregation_year_column ||
        column == return_aggregation_period_column) {
        throw input_error{"return aggregation column name is reserved"};
    }
}

aggregation_bucket bucket_for(const timestamp value,
                              const return_aggregation_period period) {
    using namespace std::chrono;

    const sys_days day_value = utc_day(value);
    const int year = calendar_year(day_value);
    const int month = calendar_month(day_value);

    switch (period) {
        case return_aggregation_period::weekly: {
            const int week = iso_week_number(day_value);
            return {{year, week}, canonical_weekly_bucket_start(year, week)};
        }
        case return_aggregation_period::monthly:
            return {{year, month},
                    utc_timestamp(year, static_cast<unsigned>(month), 1)};
        case return_aggregation_period::quarterly: {
            const int quarter = (month - 1) / 3 + 1;
            const unsigned quarter_start_month =
                static_cast<unsigned>((quarter - 1) * 3 + 1);
            return {{year, quarter}, utc_timestamp(year, quarter_start_month, 1)};
        }
        case return_aggregation_period::yearly:
            return {{year, 1}, utc_timestamp(year)};
        default:
            throw input_error{"return aggregation period is invalid"};
    }
}

}  // namespace

dataframe simple_returns(const dataframe& prices, std::string_view price_column) {
    const auto values = require_double_column(prices, price_column);
    const auto& index = prices.get_index();
    if (values.size() < 2) {
        return make_series({}, returns_column, {});
    }

    std::vector<timestamp> result_index(index.begin() + 1, index.end());
    std::vector<double> result_values;
    result_values.reserve(values.size() - 1);
    for (std::size_t position = 1; position < values.size(); ++position) {
        result_values.push_back(values[position] / values[position - 1] - 1.0);
    }
    return make_series(std::move(result_index), returns_column,
                       std::move(result_values));
}

dataframe cumulative_returns(const dataframe& returns, std::string_view column) {
    return cumulative_returns(returns, 0.0, column);
}

dataframe cumulative_returns(
    const dataframe& returns,
    const std::initializer_list<std::string_view> legacy_column) {
    return cumulative_returns(returns, legacy_column_or_throw(legacy_column));
}

dataframe cumulative_returns(const dataframe& returns,
                             const double starting_value,
                             const std::string_view column) {
    const auto values = require_double_column(returns, column);
    std::vector<double> result_values;
    result_values.reserve(values.size());

    double wealth = 1.0;
    for (const double value : values) {
        wealth *= 1.0 + (std::isnan(value) ? 0.0 : value);
        result_values.push_back(cumulative_value(wealth, starting_value));
    }
    return make_series(returns.get_index(), returns_column,
                       std::move(result_values));
}

double cumulative_return(const dataframe& returns, std::string_view column) {
    return cumulative_return(returns, 0.0, column);
}

double cumulative_return(
    const dataframe& returns,
    const std::initializer_list<std::string_view> legacy_column) {
    return cumulative_return(returns, legacy_column_or_throw(legacy_column));
}

double cumulative_return(const dataframe& returns,
                         const double starting_value,
                         const std::string_view column) {
    const auto values = require_double_column(returns, column);
    if (values.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return cumulative_value(compound_wealth(values), starting_value);
}

dataframe normalize(const dataframe& values,
                    const double starting_value,
                    const std::string_view column) {
    const auto input_values = require_double_column(values, column);
    std::vector<double> normalized;
    normalized.reserve(input_values.size());

    if (!input_values.empty()) {
        const double base = input_values.front();
        if (base == 0.0) {
            normalized.assign(input_values.size(),
                              std::numeric_limits<double>::quiet_NaN());
        } else {
            for (const double value : input_values) {
                normalized.push_back(starting_value * value / base);
            }
        }
    }

    return make_series(values.get_index(), column, std::move(normalized));
}

dataframe aggregate_returns(const dataframe& returns,
                            const return_aggregation_period period,
                            const std::string_view column) {
    validate_aggregation_period(period);
    reject_reserved_aggregation_column(column);

    const auto values = require_double_column(returns, column);
    const auto& index = returns.get_index();
    std::map<aggregation_key, aggregation_group> groups;
    for (std::size_t position = 0U; position < index.size(); ++position) {
        const aggregation_bucket bucket = bucket_for(index[position], period);
        auto [group, inserted] = groups.try_emplace(
            bucket.key, aggregation_group{bucket.start});
        static_cast<void>(inserted);
        const double value = values[position];
        group->second.wealth *= 1.0 + (std::isnan(value) ? 0.0 : value);
    }

    std::vector<timestamp> result_index;
    std::vector<int> result_years;
    std::vector<int> result_periods;
    std::vector<double> result_values;
    result_index.reserve(groups.size());
    result_years.reserve(groups.size());
    result_periods.reserve(groups.size());
    result_values.reserve(groups.size());
    for (const auto& [key, group] : groups) {
        result_index.push_back(group.bucket_start);
        result_years.push_back(key.year);
        result_periods.push_back(key.period);
        result_values.push_back(group.wealth - 1.0);
    }

    const std::string output_column{column};
    dataframe result;
    result.load_index(std::move(result_index));
    result.load_column<int>(return_aggregation_year_column.data(),
                            std::move(result_years),
                            hmdf::nan_policy::dont_pad_with_nans);
    result.load_column<int>(return_aggregation_period_column.data(),
                            std::move(result_periods),
                            hmdf::nan_policy::dont_pad_with_nans);
    result.load_column<double>(output_column.c_str(), std::move(result_values),
                               hmdf::nan_policy::dont_pad_with_nans);
    return result;
}

double annual_return(const dataframe& returns,
                     double annualization,
                     std::string_view column) {
    const auto values = require_double_column(returns, column);
    if (values.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    const double ending_wealth = compound_returns(values) + 1.0;
    if (ending_wealth <= 0.0) {
        return -1.0;
    }
    return std::pow(ending_wealth,
                    annualization / static_cast<double>(values.size())) -
           1.0;
}

double annual_volatility(const dataframe& returns,
                         double annualization,
                         std::string_view column) {
    const auto values = require_double_column(returns, column);
    std::vector<double> valid_values;
    valid_values.reserve(values.size());
    for (const double value : values) {
        if (!std::isnan(value)) {
            valid_values.push_back(value);
        }
    }
    if (valid_values.size() < 2) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    double mean = 0.0;
    for (const double value : valid_values) {
        mean += value;
    }
    mean /= static_cast<double>(valid_values.size());

    double squared_deviations = 0.0;
    for (const double value : valid_values) {
        const double deviation = value - mean;
        squared_deviations += deviation * deviation;
    }
    const double sample_variance =
        squared_deviations / static_cast<double>(valid_values.size() - 1);
    return std::sqrt(sample_variance) * std::sqrt(annualization);
}

}  // namespace fincore
