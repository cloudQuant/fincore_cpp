#include <fincore/metrics/drawdown.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <span>
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

std::optional<drawdown_window> max_drawdown_underwater(
    const dataframe& underwater, std::string_view column) {
    const auto values = require_double_column(underwater, column);
    const auto& index = underwater.get_index();
    if (values.empty()) {
        return std::nullopt;
    }
    std::size_t valley = 0U;
    double minimum = values[0];
    for (std::size_t position = 1U; position < values.size(); ++position) {
        if (values[position] < minimum) {
            minimum = values[position];
            valley = position;
        }
    }
    if (!std::isfinite(minimum) || minimum >= 0.0) {
        return std::nullopt;
    }
    std::size_t peak = 0U;
    for (std::size_t position = 0U; position <= valley; ++position) {
        if (values[position] == 0.0) {
            peak = position;
        }
    }
    std::optional<timestamp> recovery;
    for (std::size_t position = valley; position < values.size(); ++position) {
        if (values[position] == 0.0) {
            recovery = index[position];
            break;
        }
    }
    return drawdown_window{index[peak], index[valley], recovery};
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

namespace {

struct drawdown_indices {
    std::size_t peak;
    std::size_t valley;
    std::optional<std::size_t> recovery;
};

std::vector<double> underwater_values(std::span<const double> values) {
    std::vector<double> underwater;
    underwater.reserve(values.size());
    double wealth = 1.0;
    double peak = wealth;
    for (const double value : values) {
        wealth *= 1.0 + (std::isnan(value) ? 0.0 : value);
        peak = std::fmax(peak, wealth);
        underwater.push_back((wealth - peak) / peak);
    }
    return underwater;
}

std::vector<drawdown_indices> top_drawdown_indices(
    const std::vector<double>& underwater, const std::size_t top) {
    std::vector<drawdown_indices> result;
    std::vector<std::size_t> active(underwater.size());
    for (std::size_t position = 0; position < active.size(); ++position) {
        active[position] = position;
    }
    for (std::size_t rank = 0; rank < top && !active.empty(); ++rank) {
        double minimum = underwater[active.front()];
        std::size_t valley_slot = 0;
        for (std::size_t slot = 1; slot < active.size(); ++slot) {
            if (underwater[active[slot]] < minimum) {
                minimum = underwater[active[slot]];
                valley_slot = slot;
            }
        }
        if (minimum >= 0.0) {
            break;
        }

        std::size_t peak_slot = 0;
        for (std::size_t slot = 0; slot <= valley_slot; ++slot) {
            if (underwater[active[slot]] == 0.0) {
                peak_slot = slot;
            }
        }
        std::optional<std::size_t> recovery_slot;
        for (std::size_t slot = valley_slot; slot < active.size(); ++slot) {
            if (underwater[active[slot]] == 0.0) {
                recovery_slot = slot;
                break;
            }
        }

        result.push_back(
            {active[peak_slot], active[valley_slot],
             recovery_slot ? std::optional<std::size_t>(active[*recovery_slot])
                           : std::nullopt});

        std::vector<std::size_t> next(active.begin(),
                                      active.begin() +
                                          static_cast<std::ptrdiff_t>(peak_slot) +
                                          1);
        if (recovery_slot) {
            next.insert(next.end(),
                        active.begin() +
                            static_cast<std::ptrdiff_t>(*recovery_slot),
                        active.end());
        }
        active = std::move(next);
    }
    return result;
}

std::size_t business_days_inclusive(const fincore::timestamp start,
                                    const fincore::timestamp end) {
    constexpr long long nanoseconds_per_day = 86'400'000'000'000LL;
    if (end < start) {
        return 0;
    }
    const auto floor_divide = [](const long long value, const long long unit) {
        long long quotient = value / unit;
        if (value % unit != 0 && ((value < 0) != (unit < 0))) {
            --quotient;
        }
        return quotient;
    };
    const long long start_day = floor_divide(start, nanoseconds_per_day);
    const long long end_day = floor_divide(end, nanoseconds_per_day);
    std::size_t count = 0;
    for (long long day = start_day; day <= end_day; ++day) {
        const long long weekday = ((day % 7) + 7 + 3) % 7;
        if (weekday < 5) {
            ++count;
        }
    }
    return count;
}

}  // namespace

std::vector<drawdown_window> get_top_drawdowns(
    const dataframe& returns,
    const std::size_t top,
    std::string_view column) {
    if (column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    const auto values = require_double_column(returns, column);
    const auto& index = returns.get_index();
    std::vector<drawdown_window> result;
    if (values.empty() || top == 0) {
        return result;
    }

    for (const auto& entry :
         top_drawdown_indices(underwater_values(values), top)) {
        result.push_back(
            {index[entry.peak], index[entry.valley],
             entry.recovery ? std::optional<timestamp>(index[*entry.recovery])
                            : std::nullopt});
    }
    return result;
}

std::vector<drawdown_table_row> gen_drawdown_table(
    const dataframe& returns,
    const std::size_t top,
    std::string_view column) {
    if (column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    const auto values = require_double_column(returns, column);
    const auto& index = returns.get_index();
    std::vector<drawdown_table_row> rows(top);
    if (values.empty() || top == 0) {
        return rows;
    }

    const auto underwater = underwater_values(values);
    const auto windows = top_drawdown_indices(underwater, top);
    for (std::size_t row = 0; row < windows.size(); ++row) {
        const auto& window = windows[row];
        rows[row].net_drawdown_percent = -underwater[window.valley] * 100.0;
        rows[row].peak = index[window.peak];
        rows[row].valley = index[window.valley];
        if (window.recovery) {
            rows[row].recovery = index[*window.recovery];
            rows[row].duration_business_days = business_days_inclusive(
                index[window.peak], index[*window.recovery]);
        }
    }
    return rows;
}

std::pair<std::optional<timestamp>, std::optional<timestamp>>
max_drawdown_period(const dataframe& returns, std::string_view column) {
    if (column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    const auto values = require_double_column(returns, column);
    const auto& index = returns.get_index();
    if (values.empty()) {
        return {std::nullopt, std::nullopt};
    }

    const auto underwater = underwater_values(values);
    double minimum = underwater.front();
    std::size_t valley = 0;
    for (std::size_t position = 1; position < underwater.size(); ++position) {
        if (underwater[position] < minimum) {
            minimum = underwater[position];
            valley = position;
        }
    }
    if (minimum >= 0.0) {
        return {index.front(), index.front()};
    }

    std::size_t peak = 0;
    for (std::size_t position = 0; position <= valley; ++position) {
        if (underwater[position] == 0.0) {
            peak = position;
        }
    }
    return {index[peak], index[valley]};
}

}  // namespace fincore
