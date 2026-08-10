#include <fincore/metrics/consecutive.hpp>

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

}  // namespace fincore
