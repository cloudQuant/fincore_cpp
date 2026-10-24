#pragma once

#include <fincore/factor/data.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace fincore {

inline constexpr std::int64_t nanoseconds_per_day = 86'400'000'000'000LL;

/// Deterministic trading-day set used by the factor calendar helpers.
///
/// A calendar is defined by local wall dates: seven weekday flags plus a set
/// of holiday day numbers (days since 1970-01-01 in UTC). It intentionally has
/// no timezone object; timestamps are UTC nanoseconds.
struct trading_calendar {
    /// Monday-first weekday mask. ``true`` marks a trading weekday.
    bool weekdays[7] = {true, true, true, true, true, false, false};
    /// Sorted, unique holiday day numbers. Never contains a non-weekday.
    std::vector<std::int64_t> holidays;

    bool is_trading_day(std::int64_t day_number) const noexcept;
    bool is_session(timestamp value) const noexcept;
    /// Number of trading sessions in ``[start_day, end_day)`` when
    /// ``start_day <= end_day``; otherwise the negated session count in
    /// ``(end_day, start_day]`` (NumPy ``busday_count`` half-open semantics).
    std::int64_t busday_count(std::int64_t start_day,
                              std::int64_t end_day) const noexcept;
};

/// A weekday-only calendar matching pandas' default ``Day`` offset.
trading_calendar all_days_calendar();
/// A Monday-Friday calendar with no holidays.
trading_calendar business_day_calendar();

/// Infer trading weekdays and missing-session holidays from factor and price
/// dates. The union is normalized to days; an empty union is rejected.
trading_calendar infer_trading_calendar(
    const std::vector<timestamp>& factor_dates,
    const std::vector<timestamp>& price_dates);

/// Add ``delta`` nanoseconds, counting whole days through ``calendar``.
///
/// The timedelta is normalized to a floor whole-day component plus a
/// non-negative remainder; the day component is rolled by the calendar and
/// the remainder is added afterwards. This matches pandas'
/// ``Timestamp + CalendarOffset(days) + remainder`` behaviour.
timestamp add_custom_calendar_timedelta(timestamp input,
                                        std::int64_t delta,
                                        const trading_calendar& calendar);

std::vector<timestamp> add_custom_calendar_timedelta(
    const std::vector<timestamp>& input, std::int64_t delta,
    const trading_calendar& calendar);

/// Return ``end - start`` with whole-day components measured by ``calendar``.
///
/// The wall-clock remainder is preserved; only the whole-day part is replaced
/// by the busday count. Both endpoints use local wall dates, as in the pinned
/// source.
std::int64_t diff_custom_calendar_timedeltas(timestamp start, timestamp end,
                                             const trading_calendar& calendar);

/// Format a duration using the pinned forward-return label grammar.
///
/// Components follow pandas' normalized representation (floor days and a
/// non-negative remainder), for example ``-1h`` becomes ``"-1D23h"`` and a
/// zero duration becomes ``""``.
std::string timedelta_to_string(std::int64_t delta);

/// Whole-day component of a pandas-style duration string.
std::vector<int> timedelta_strings_to_integers(
    const std::vector<std::string>& sequence);

/// Parse a forward-return duration label into nanoseconds.
///
/// Accepts an optional leading sign followed by one or more
/// ``<digits><unit>`` components, where unit is one of D, h, m, s, ms, us, ns
/// (case-insensitive). Throws ``factor_data_error`` for malformed labels.
std::int64_t parse_timedelta_nanoseconds(const std::string& label);

}  // namespace fincore
