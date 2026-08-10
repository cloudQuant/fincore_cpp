#include <fincore/metrics/drawdown.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

namespace {

struct underwater_event {
    fincore::timestamp peak;
    fincore::timestamp start;
    fincore::timestamp valley;
    std::optional<fincore::timestamp> recovery;
    double value;
    std::size_t duration_periods;
    std::optional<std::size_t> recovery_duration_periods;
    std::size_t start_position;
    std::size_t valley_position;
};

std::vector<underwater_event> scan_underwater_events(
    const fincore::dataframe& returns,
    std::string_view column) {
    const auto values = fincore::require_double_column(returns, column);
    const auto& index = returns.get_index();
    std::vector<underwater_event> events;
    if (values.empty()) {
        return events;
    }

    const auto adjusted_return = [](const double value) {
        return std::isnan(value) ? 0.0 : value;
    };

    double wealth = 100.0 * (1.0 + adjusted_return(values[0]));
    double peak_wealth = wealth;
    fincore::timestamp peak = index[0];
    std::optional<underwater_event> active;

    for (std::size_t position = 1; position < values.size(); ++position) {
        wealth *= 1.0 + adjusted_return(values[position]);
        if (wealth >= peak_wealth) {
            if (active.has_value()) {
                active->recovery = index[position];
                active->recovery_duration_periods =
                    position - active->valley_position;
                events.push_back(*active);
                active.reset();
            }
            peak_wealth = wealth;
            peak = index[position];
        }

        const double drawdown = (wealth - peak_wealth) / peak_wealth;
        if (drawdown < 0.0) {
            if (!active.has_value()) {
                active = underwater_event{peak,
                                          index[position],
                                          index[position],
                                          std::nullopt,
                                          drawdown,
                                          0U,
                                          std::nullopt,
                                          position,
                                          position};
            } else if (drawdown < active->value) {
                active->valley = index[position];
                active->value = drawdown;
                active->duration_periods = position - active->start_position;
                active->valley_position = position;
            }
        }
    }

    if (active.has_value()) {
        events.push_back(*active);
    }
    return events;
}

double elapsed_utc_days(const fincore::timestamp start,
                        const fincore::timestamp end) {
    constexpr __int128 nanoseconds_per_day =
        static_cast<__int128>(86'400) * 1'000'000'000;
    const __int128 elapsed =
        static_cast<__int128>(end) - static_cast<__int128>(start);
    __int128 days = elapsed / nanoseconds_per_day;
    if (elapsed < 0 && elapsed % nanoseconds_per_day != 0) {
        --days;
    }
    return static_cast<double>(days);
}

const underwater_event* maximum_event(
    const std::vector<underwater_event>& events) {
    if (events.empty()) {
        return nullptr;
    }
    const underwater_event* result = &events.front();
    for (const auto& event : events) {
        if (event.value < result->value) {
            result = &event;
        }
    }
    return result;
}

const underwater_event* ranked_event(
    const std::vector<underwater_event>& events,
    const std::size_t rank) {
    if (events.size() <= rank) {
        return nullptr;
    }
    std::vector<const underwater_event*> ranked;
    ranked.reserve(events.size());
    for (const auto& event : events) {
        ranked.push_back(&event);
    }
    std::stable_sort(ranked.begin(), ranked.end(),
                     [](const auto* left, const auto* right) {
                         return left->value < right->value;
                     });
    return ranked[rank];
}

double ranked_drawdown(const fincore::dataframe& returns,
                       const std::string_view column,
                       const std::size_t rank) {
    const auto events = scan_underwater_events(returns, column);
    const auto* event = ranked_event(events, rank);
    return event == nullptr ? std::numeric_limits<double>::quiet_NaN()
                            : event->value;
}

double ranked_drawdown_days(const fincore::dataframe& returns,
                            const std::string_view column,
                            const std::size_t rank) {
    const auto events = scan_underwater_events(returns, column);
    const auto* event = ranked_event(events, rank);
    return event == nullptr
               ? std::numeric_limits<double>::quiet_NaN()
               : elapsed_utc_days(event->start, event->valley);
}

double ranked_recovery_days(const fincore::dataframe& returns,
                            const std::string_view column,
                            const std::size_t rank) {
    const auto events = scan_underwater_events(returns, column);
    const auto* event = ranked_event(events, rank);
    if (event == nullptr || !event->recovery.has_value()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return elapsed_utc_days(event->valley, *event->recovery);
}

}  // namespace

namespace fincore {

dataframe drawdown_series(const dataframe& returns, std::string_view column) {
    const auto values = require_double_column(returns, column);
    std::vector<double> drawdowns;
    drawdowns.reserve(values.size());

    double wealth = 100.0;
    double peak = wealth;
    for (const double value : values) {
        wealth *= 1.0 + (std::isnan(value) ? 0.0 : value);
        peak = std::fmax(peak, wealth);
        drawdowns.push_back((wealth - peak) / peak);
    }
    return make_series(returns.get_index(), returns_column, std::move(drawdowns));
}

double max_drawdown(const dataframe& returns, std::string_view column) {
    const auto values = require_double_column(returns, column);
    if (values.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    const auto drawdowns = drawdown_series(returns, column);
    const auto drawdown_values =
        require_double_column(drawdowns, returns_column);
    double result = std::numeric_limits<double>::quiet_NaN();
    for (const double value : drawdown_values) {
        if (!std::isnan(value) && (std::isnan(result) || value < result)) {
            result = value;
        }
    }
    return result;
}

std::vector<double> get_all_drawdowns(const dataframe& returns,
                                      std::string_view column) {
    const auto events = scan_underwater_events(returns, column);
    std::vector<double> result;
    result.reserve(events.size());
    for (const auto& event : events) {
        result.push_back(event.value);
    }
    return result;
}

std::vector<drawdown_period> get_all_drawdowns_detailed(
    const dataframe& returns,
    std::string_view column) {
    const auto events = scan_underwater_events(returns, column);
    std::vector<drawdown_period> result;
    result.reserve(events.size());
    for (const auto& event : events) {
        result.push_back(drawdown_period{event.start,
                                         event.valley,
                                         event.recovery,
                                         event.value,
                                         event.duration_periods,
                                         event.recovery_duration_periods});
    }
    return result;
}

std::optional<drawdown_window> max_drawdown_window(
    const dataframe& returns,
    std::string_view column) {
    const auto events = scan_underwater_events(returns, column);
    if (events.empty()) {
        return std::nullopt;
    }

    const underwater_event* maximum = &events.front();
    for (const auto& event : events) {
        if (event.value < maximum->value) {
            maximum = &event;
        }
    }
    return drawdown_window{maximum->peak, maximum->valley,
                           maximum->recovery};
}

double max_drawdown_days(const dataframe& returns, std::string_view column) {
    const auto values = require_double_column(returns, column);
    if (values.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const auto events = scan_underwater_events(returns, column);
    const auto* event = maximum_event(events);
    return event == nullptr ? 0.0
                            : elapsed_utc_days(event->peak, event->valley);
}

double max_drawdown_weeks(const dataframe& returns, std::string_view column) {
    return max_drawdown_days(returns, column) / 7.0;
}

double max_drawdown_months(const dataframe& returns, std::string_view column) {
    return max_drawdown_days(returns, column) / 30.0;
}

double max_drawdown_recovery_days(const dataframe& returns,
                                  std::string_view column) {
    const auto values = require_double_column(returns, column);
    if (values.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const auto events = scan_underwater_events(returns, column);
    const auto* event = maximum_event(events);
    if (event == nullptr) {
        return 0.0;
    }
    if (!event->recovery.has_value()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return elapsed_utc_days(event->valley, *event->recovery);
}

double max_drawdown_recovery_weeks(const dataframe& returns,
                                   std::string_view column) {
    return max_drawdown_recovery_days(returns, column) / 7.0;
}

double max_drawdown_recovery_months(const dataframe& returns,
                                    std::string_view column) {
    return max_drawdown_recovery_days(returns, column) / 30.0;
}

double second_max_drawdown(const dataframe& returns,
                           std::string_view column) {
    return ranked_drawdown(returns, column, 1U);
}

double third_max_drawdown(const dataframe& returns, std::string_view column) {
    return ranked_drawdown(returns, column, 2U);
}

double second_max_drawdown_days(const dataframe& returns,
                                std::string_view column) {
    return ranked_drawdown_days(returns, column, 1U);
}

double second_max_drawdown_recovery_days(const dataframe& returns,
                                         std::string_view column) {
    return ranked_recovery_days(returns, column, 1U);
}

double third_max_drawdown_days(const dataframe& returns,
                               std::string_view column) {
    return ranked_drawdown_days(returns, column, 2U);
}

double third_max_drawdown_recovery_days(const dataframe& returns,
                                        std::string_view column) {
    return ranked_recovery_days(returns, column, 2U);
}

}  // namespace fincore
