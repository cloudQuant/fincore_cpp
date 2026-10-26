#include <fincore/metrics/consecutive.hpp>

#include <chrono>
#include <map>

#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <string_view>

namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();

struct consecutive_summary {
    fincore::daily_consecutive_statistics daily{nan, nan, nan, nan};
    double max_single_day_gain{nan};
    double max_single_day_loss{nan};
    std::optional<fincore::timestamp> max_single_day_gain_date;
    std::optional<fincore::timestamp> max_single_day_loss_date;
    std::optional<fincore::timestamp> max_consecutive_up_start_date;
    std::optional<fincore::timestamp> max_consecutive_up_end_date;
    std::optional<fincore::timestamp> max_consecutive_down_start_date;
    std::optional<fincore::timestamp> max_consecutive_down_end_date;
};

consecutive_summary scan_consecutive_returns(
    const fincore::dataframe& returns,
    const std::string_view column) {
    const auto values = fincore::require_double_column(returns, column);
    const auto& index = returns.get_index();
    consecutive_summary summary;
    if (values.empty()) {
        return summary;
    }

    summary.daily.max_consecutive_up_days = 0.0;
    summary.daily.max_consecutive_down_days = 0.0;

    std::size_t current_up_days = 0U;
    std::size_t current_down_days = 0U;
    double current_up_gain = 0.0;
    double current_down_loss = 0.0;
    fincore::timestamp current_up_start{};
    fincore::timestamp current_down_start{};

    // Records are kept by strict comparison, so a tie leaves the earliest dates
    // attached to the longest or largest run, and the run accumulates simple
    // returns across its days rather than compounding them, which is what makes
    // a streak's gain comparable with its length.
    for (std::size_t position = 0U; position < values.size(); ++position) {
        const double value = values[position];
        const auto date = index[position];

        if (!std::isnan(value)) {
            if (std::isnan(summary.max_single_day_gain) ||
                value > summary.max_single_day_gain) {
                summary.max_single_day_gain = value;
                summary.max_single_day_gain_date = date;
            }
            if (std::isnan(summary.max_single_day_loss) ||
                value < summary.max_single_day_loss) {
                summary.max_single_day_loss = value;
                summary.max_single_day_loss_date = date;
            }
        }

        if (value > 0.0) {
            current_down_days = 0U;
            if (current_up_days == 0U) {
                current_up_start = date;
                current_up_gain = value;
            } else {
                current_up_gain += value;
            }
            ++current_up_days;

            if (static_cast<double>(current_up_days) >
                summary.daily.max_consecutive_up_days) {
                summary.daily.max_consecutive_up_days =
                    static_cast<double>(current_up_days);
                summary.max_consecutive_up_start_date = current_up_start;
                summary.max_consecutive_up_end_date = date;
            }
            if (std::isnan(summary.daily.max_consecutive_gain) ||
                current_up_gain > summary.daily.max_consecutive_gain) {
                summary.daily.max_consecutive_gain = current_up_gain;
            }
            continue;
        }

        if (value < 0.0) {
            current_up_days = 0U;
            if (current_down_days == 0U) {
                current_down_start = date;
                current_down_loss = value;
            } else {
                current_down_loss += value;
            }
            ++current_down_days;

            if (static_cast<double>(current_down_days) >
                summary.daily.max_consecutive_down_days) {
                summary.daily.max_consecutive_down_days =
                    static_cast<double>(current_down_days);
                summary.max_consecutive_down_start_date = current_down_start;
                summary.max_consecutive_down_end_date = date;
            }
            if (std::isnan(summary.daily.max_consecutive_loss) ||
                current_down_loss < summary.daily.max_consecutive_loss) {
                summary.daily.max_consecutive_loss = current_down_loss;
            }
            continue;
        }

        // Anything that is neither strictly above nor strictly below zero ends
        // both runs, so a flat print and a NaN both break a streak rather than
        // being stepped over; the counters only restart on the next strictly
        // signed day.
        current_up_days = 0U;
        current_down_days = 0U;
    }

    return summary;
}

}  // namespace

namespace fincore {

daily_consecutive_statistics daily_consecutive_stats(
    const dataframe& returns,
    const std::string_view column) {
    return scan_consecutive_returns(returns, column).daily;
}

double max_consecutive_up_days(const dataframe& returns,
                               const std::string_view column) {
    return scan_consecutive_returns(returns, column)
        .daily.max_consecutive_up_days;
}

double max_consecutive_down_days(const dataframe& returns,
                                 const std::string_view column) {
    return scan_consecutive_returns(returns, column)
        .daily.max_consecutive_down_days;
}

double max_consecutive_gain(const dataframe& returns,
                            const std::string_view column) {
    return scan_consecutive_returns(returns, column).daily.max_consecutive_gain;
}

double max_consecutive_loss(const dataframe& returns,
                            const std::string_view column) {
    return scan_consecutive_returns(returns, column).daily.max_consecutive_loss;
}

double max_single_day_gain(const dataframe& returns,
                           const std::string_view column) {
    return scan_consecutive_returns(returns, column).max_single_day_gain;
}

double max_single_day_loss(const dataframe& returns,
                           const std::string_view column) {
    return scan_consecutive_returns(returns, column).max_single_day_loss;
}

std::optional<timestamp> max_single_day_gain_date(
    const dataframe& returns,
    const std::string_view column) {
    return scan_consecutive_returns(returns, column).max_single_day_gain_date;
}

std::optional<timestamp> max_single_day_loss_date(
    const dataframe& returns,
    const std::string_view column) {
    return scan_consecutive_returns(returns, column).max_single_day_loss_date;
}

std::optional<timestamp> max_consecutive_up_start_date(
    const dataframe& returns,
    const std::string_view column) {
    return scan_consecutive_returns(returns, column)
        .max_consecutive_up_start_date;
}

std::optional<timestamp> max_consecutive_up_end_date(
    const dataframe& returns,
    const std::string_view column) {
    return scan_consecutive_returns(returns, column)
        .max_consecutive_up_end_date;
}

std::optional<timestamp> max_consecutive_down_start_date(
    const dataframe& returns,
    const std::string_view column) {
    return scan_consecutive_returns(returns, column)
        .max_consecutive_down_start_date;
}

std::optional<timestamp> max_consecutive_down_end_date(
    const dataframe& returns,
    const std::string_view column) {
    return scan_consecutive_returns(returns, column)
        .max_consecutive_down_end_date;
}

namespace {

constexpr long long nanoseconds_per_day = 86'400'000'000'000LL;

long long day_number(const fincore::timestamp value) {
    long long quotient = value / nanoseconds_per_day;
    if (value % nanoseconds_per_day != 0 && value < 0) {
        --quotient;
    }
    return quotient;
}

long long weekly_key(const long long day) {
    const long long weekday = ((day % 7) + 7 + 3) % 7;  // Monday = 0
    return day + (6 - weekday + 7) % 7;
}

long long monthly_key(const long long day) {
    const std::chrono::year_month_day date{
        std::chrono::sys_days{std::chrono::days{day}}};
    return static_cast<long long>(static_cast<int>(date.year())) * 12 +
           static_cast<long long>(static_cast<unsigned>(date.month())) - 1;
}

double bucket_return(const std::vector<double>& values) {
    double wealth = 1.0;
    for (const double value : values) {
        wealth *= 1.0 + (std::isnan(value) ? 0.0 : value);
    }
    return wealth - 1.0;
}

double max_consecutive_buckets(const dataframe& returns,
                               const std::string_view column,
                               const bool weekly, const bool positive) {
    const auto values = fincore::require_double_column(returns, column);
    if (values.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const auto& index = returns.get_index();
    std::map<long long, std::vector<double>> buckets;
    for (std::size_t position = 0; position < values.size(); ++position) {
        const long long day = day_number(index[position]);
        const long long key = weekly ? weekly_key(day) : monthly_key(day);
        buckets[key].push_back(values[position]);
    }
    double best = 0.0;
    double current = 0.0;
    for (const auto& entry : buckets) {
        const double bucket = bucket_return(entry.second);
        const bool matches = positive ? bucket > 0.0 : bucket < 0.0;
        if (matches) {
            current += 1.0;
            best = std::max(best, current);
        } else {
            current = 0.0;
        }
    }
    return best;
}

}  // namespace

double max_consecutive_up_weeks(const dataframe& returns,
                                std::string_view column) {
    if (column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    return max_consecutive_buckets(returns, column, true, true);
}

double max_consecutive_down_weeks(const dataframe& returns,
                                  std::string_view column) {
    if (column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    return max_consecutive_buckets(returns, column, true, false);
}

double max_consecutive_up_months(const dataframe& returns,
                                 std::string_view column) {
    if (column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    return max_consecutive_buckets(returns, column, false, true);
}

double max_consecutive_down_months(const dataframe& returns,
                                   std::string_view column) {
    if (column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    return max_consecutive_buckets(returns, column, false, false);
}

std::map<std::string, double> consecutive_stats(
    const dataframe& returns, std::string_view column) {
    if (column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    return {
        {"max_consecutive_up_days", max_consecutive_up_days(returns, column)},
        {"max_consecutive_down_days",
         max_consecutive_down_days(returns, column)},
        {"max_consecutive_up_weeks", max_consecutive_up_weeks(returns, column)},
        {"max_consecutive_down_weeks",
         max_consecutive_down_weeks(returns, column)},
        {"max_consecutive_up_months",
         max_consecutive_up_months(returns, column)},
        {"max_consecutive_down_months",
         max_consecutive_down_months(returns, column)},
    };
}

}  // namespace fincore
