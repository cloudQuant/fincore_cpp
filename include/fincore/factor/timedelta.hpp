#pragma once

#include <fincore/factor/data.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace fincore {

/// Nanoseconds in one calendar day.
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

    /// Checks whether a day number is a trading day.
    ///
    /// @param day_number Days since 1970-01-01 in UTC.
    /// @return `true` when the weekday is enabled and the day is not a
    ///         holiday.
    bool is_trading_day(std::int64_t day_number) const noexcept;
    /// Checks whether a timestamp falls on a trading day.
    ///
    /// @param value Timestamp in UTC nanoseconds.
    /// @return `true` when the timestamp's UTC day is a trading day.
    bool is_session(timestamp value) const noexcept;
    /// Counts trading sessions in ``[start_day, end_day)`` when
    /// ``start_day <= end_day``; otherwise returns the negation of the count
    /// in ``(end_day, start_day]`` (NumPy ``busday_count`` half-open
    /// semantics).
    ///
    /// @param start_day First day number.
    /// @param end_day Exclusive end day number for forward counts.
    /// @return Signed number of trading sessions.
    std::int64_t busday_count(std::int64_t start_day,
                              std::int64_t end_day) const noexcept;
};

/// Returns a calendar with every day marked as a trading day, matching
/// pandas' default ``Day`` offset.
///
/// @return The all-days calendar.
trading_calendar all_days_calendar();
/// Returns a Monday-Friday calendar with no holidays.
///
/// @return The business-day calendar.
trading_calendar business_day_calendar();

/// Infers trading weekdays and missing-session holidays from factor and price
/// dates. The union is normalized to days; an empty union is rejected.
///
/// @param factor_dates Factor observation timestamps in UTC nanoseconds.
/// @param price_dates Price observation timestamps in UTC nanoseconds.
/// @return Calendar whose enabled weekdays and holidays cover the union.
/// @throws factor_data_error When both input date lists are empty.
trading_calendar infer_trading_calendar(
    const std::vector<timestamp>& factor_dates,
    const std::vector<timestamp>& price_dates);

/// Adds ``delta`` nanoseconds, counting whole days through ``calendar``.
///
/// The timedelta is normalized to a floor whole-day component plus a
/// non-negative remainder; the day component is rolled by the calendar and
/// the remainder is added afterwards. This matches pandas'
/// ``Timestamp + CalendarOffset(days) + remainder`` behaviour.
///
/// @param input Starting timestamp or timestamps in UTC nanoseconds.
/// @param delta Signed duration in nanoseconds.
/// @param calendar Calendar used to roll whole-day components.
/// @return The shifted timestamp, or one shifted timestamp per input value.
/// @throws factor_data_error When the shifted timestamp overflows.
timestamp add_custom_calendar_timedelta(timestamp input,
                                        std::int64_t delta,
                                        const trading_calendar& calendar);

/// Adds ``delta`` nanoseconds to each timestamp using ``calendar``.
///
/// Applies the same normalization and whole-day roll rules as the scalar
/// overload and preserves the input order.
///
/// @param input Starting timestamps in UTC nanoseconds.
/// @param delta Signed duration in nanoseconds.
/// @param calendar Calendar used to roll whole-day components.
/// @return One shifted timestamp per input value.
/// @throws factor_data_error When a shifted timestamp overflows.
std::vector<timestamp> add_custom_calendar_timedelta(
    const std::vector<timestamp>& input, std::int64_t delta,
    const trading_calendar& calendar);

/// Returns ``end - start`` with whole-day components measured by ``calendar``.
///
/// The wall-clock remainder is preserved; only the whole-day part is replaced
/// by the busday count. Both endpoints use local wall dates, as in the pinned
/// source.
///
/// @param start Start timestamp in UTC nanoseconds.
/// @param end End timestamp in UTC nanoseconds.
/// @param calendar Calendar used for the whole-day count.
/// @return Signed duration in nanoseconds with the wall-clock remainder.
std::int64_t diff_custom_calendar_timedeltas(timestamp start, timestamp end,
                                             const trading_calendar& calendar);

/// Formats a duration using the pinned forward-return label grammar.
///
/// Components follow pandas' normalized representation (floor days and a
/// non-negative remainder), for example ``-1h`` becomes ``"-1D23h"`` and a
/// zero duration becomes ``""``.
///
/// @param delta Duration in nanoseconds.
/// @return Pinned duration label; empty for a zero duration.
std::string timedelta_to_string(std::int64_t delta);

/// Returns the whole-day component of each pandas-style duration string.
///
/// @param sequence Duration labels to parse.
/// @return One whole-day component per label, in input order.
/// @throws factor_data_error When a label is malformed or a day component
///         does not fit in `int`.
std::vector<int> timedelta_strings_to_integers(
    const std::vector<std::string>& sequence);

/// Parses a forward-return duration label into nanoseconds.
///
/// Accepts an optional leading sign followed by one or more
/// ``<digits><unit>`` components, where unit is one of D, h, m, s, ms, us, ns
/// (case-insensitive). Malformed labels are rejected.
///
/// @param label Duration label such as ``"1D"`` or ``"2h30m"``.
/// @return Parsed duration in nanoseconds, negative when the label starts
///         with `-`.
/// @throws factor_data_error When the label is empty, malformed or names an
///         unsupported unit.
std::int64_t parse_timedelta_nanoseconds(const std::string& label);

}  // namespace fincore
