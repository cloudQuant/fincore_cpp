#include <fincore/factor/timedelta.hpp>

#include <fincore/factor/exceptions.hpp>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fincore {
namespace {

constexpr std::int64_t nanoseconds_per_hour = 3'600'000'000'000LL;
constexpr std::int64_t nanoseconds_per_minute = 60'000'000'000LL;
constexpr std::int64_t nanoseconds_per_second = 1'000'000'000LL;
constexpr std::int64_t nanoseconds_per_millisecond = 1'000'000LL;
constexpr std::int64_t nanoseconds_per_microsecond = 1'000LL;

std::int64_t floor_div(const std::int64_t value, const std::int64_t divisor) {
    std::int64_t quotient = value / divisor;
    if ((value % divisor != 0) && ((value < 0) != (divisor < 0))) {
        --quotient;
    }
    return quotient;
}

std::int64_t day_number(const timestamp value) {
    return floor_div(value, nanoseconds_per_day);
}

bool in_holidays(const std::vector<std::int64_t>& holidays,
                 const std::int64_t day) {
    return std::binary_search(holidays.begin(), holidays.end(), day);
}

std::int64_t next_trading_day(const trading_calendar& calendar,
                              std::int64_t day) {
    while (!calendar.is_trading_day(day)) {
        ++day;
    }
    return day;
}

std::int64_t previous_trading_day(const trading_calendar& calendar,
                                  std::int64_t day) {
    while (!calendar.is_trading_day(day)) {
        --day;
    }
    return day;
}

// Rolling from a day that is not a session spends that step on finding the
// anchor, which is why the remaining count is one smaller in both directions
// and why a zero shift snaps an off-session timestamp onto the next or previous
// session rather than leaving it where it is.
std::int64_t roll_day(const trading_calendar& calendar, const timestamp value,
                      const std::int64_t whole_days) {
    const std::int64_t day = day_number(value);
    if (calendar.is_trading_day(day)) {
        return day + whole_days;
    }
    if (whole_days >= 0) {
        return next_trading_day(calendar, day) + (whole_days == 0 ? 0 : whole_days - 1);
    }
    return previous_trading_day(calendar, day) + (whole_days + 1);
}

std::int64_t parse_signed_number(const std::string& text,
                                 std::size_t& position) {
    const std::size_t start = position;
    std::int64_t value = 0;
    while (position < text.size() &&
           std::isdigit(static_cast<unsigned char>(text[position])) != 0) {
        const int digit = text[position] - '0';
        // Tested before the multiply and written as a divided bound, so the
        // comparison itself never forms an overflowing intermediate.
        if (value > (std::numeric_limits<std::int64_t>::max() - digit) / 10) {
            throw factor_data_error{"timedelta component is too large"};
        }
        value = value * 10 + digit;
        ++position;
    }
    if (position == start) {
        throw factor_data_error{"timedelta label must contain digits"};
    }
    return value;
}

std::int64_t unit_multiplier(const char unit) {
    switch (unit) {
        case 'd':
        case 'D':
            return nanoseconds_per_day;
        case 'h':
        case 'H':
            return nanoseconds_per_hour;
        case 'm':
        case 'M':
            // Pandas distinguishes m (minute) from M (month); the pinned
            // forward-return grammar only uses the minute suffix.
            return nanoseconds_per_minute;
        case 's':
        case 'S':
            return nanoseconds_per_second;
        default:
            throw factor_data_error{"unsupported timedelta unit"};
    }
}

}  // namespace

bool trading_calendar::is_trading_day(const std::int64_t day) const noexcept {
    // Day zero is a Thursday and the weekday array is Monday-first, hence the
    // offset of three. The negative branch adds seven because C++ modulo keeps
    // the sign of the dividend, so pre-epoch days would otherwise index the
    // array out of order.
    if (day < 0) {
        const std::int64_t weekday = ((day % 7) + 7 + 3) % 7;
        if (!weekdays[static_cast<std::size_t>(weekday)]) {
            return false;
        }
        return !in_holidays(holidays, day);
    }
    const std::int64_t weekday = ((day % 7) + 3) % 7;
    if (!weekdays[static_cast<std::size_t>(weekday)]) {
        return false;
    }
    return !in_holidays(holidays, day);
}

bool trading_calendar::is_session(const timestamp value) const noexcept {
    return is_trading_day(day_number(value));
}

// Half-open on both sides: the end day is never counted, which is what makes
// reversing the arguments negate the same set of days instead of shifting the
// window by one, and what makes an empty range report zero rather than one.
std::int64_t trading_calendar::busday_count(
    const std::int64_t start_day, const std::int64_t end_day) const noexcept {
    if (start_day == end_day) {
        return 0;
    }
    if (start_day < end_day) {
        std::int64_t count = 0;
        for (std::int64_t day = start_day; day < end_day; ++day) {
            if (is_trading_day(day)) {
                ++count;
            }
        }
        return count;
    }
    std::int64_t count = 0;
    for (std::int64_t day = end_day + 1; day <= start_day; ++day) {
        if (is_trading_day(day)) {
            ++count;
        }
    }
    return -count;
}

trading_calendar all_days_calendar() {
    trading_calendar calendar;
    for (bool& weekday : calendar.weekdays) {
        weekday = true;
    }
    return calendar;
}

trading_calendar business_day_calendar() {
    return trading_calendar{};
}

void deduplicate_sorted(std::vector<timestamp>& values) {
    if (values.empty()) {
        return;
    }
    std::size_t write = 1U;
    for (std::size_t read = 1U; read < values.size(); ++read) {
        if (values[read] != values[write - 1U]) {
            values[write++] = values[read];
        }
    }
    values.resize(write);
}

trading_calendar infer_trading_calendar(
    const std::vector<timestamp>& factor_dates,
    const std::vector<timestamp>& price_dates) {
    std::vector<timestamp> union_dates;
    union_dates.reserve(factor_dates.size() + price_dates.size());
    union_dates.insert(union_dates.end(), factor_dates.begin(),
                       factor_dates.end());
    union_dates.insert(union_dates.end(), price_dates.begin(),
                       price_dates.end());
    if (union_dates.empty()) {
        throw factor_data_error{
            "cannot infer a trading calendar from empty indexes"};
    }
    std::sort(union_dates.begin(), union_dates.end());
    deduplicate_sorted(union_dates);

    const std::int64_t first_day = day_number(union_dates.front());
    const std::int64_t last_day = day_number(union_dates.back());

    trading_calendar calendar;
    for (bool& weekday : calendar.weekdays) {
        weekday = false;
    }
    std::vector<std::unordered_set<std::int64_t>> used_by_weekday(7U);
    for (const timestamp value : union_dates) {
        const std::int64_t day = day_number(value);
        const std::int64_t weekday = ((day % 7) + 7 + 3) % 7;
        used_by_weekday[static_cast<std::size_t>(weekday)].insert(day);
        calendar.weekdays[static_cast<std::size_t>(weekday)] = true;
    }
    // A weekday never present in the union stays closed, and inside an observed
    // weekday every day number of the span that never appears becomes a holiday:
    // the inference reads a gap in the data as a market closure, and only marks
    // days whose weekday is open, which is what keeps the holiday list free of
    // non-weekday entries.
    for (std::size_t weekday = 0U; weekday < 7U; ++weekday) {
        if (!calendar.weekdays[weekday]) {
            continue;
        }
        for (std::int64_t day = first_day; day <= last_day; ++day) {
            const std::int64_t current = ((day % 7) + 7 + 3) % 7;
            if (current != static_cast<std::int64_t>(weekday)) {
                continue;
            }
            if (!used_by_weekday[weekday].contains(day)) {
                calendar.holidays.push_back(day);
            }
        }
    }
    std::sort(calendar.holidays.begin(), calendar.holidays.end());
    deduplicate_sorted(calendar.holidays);
    return calendar;
}

timestamp add_custom_calendar_timedelta(const timestamp input,
                                        const std::int64_t delta,
                                        const trading_calendar& calendar) {
    // Whole days move through the calendar while the clock position is carried
    // unchanged from the input, and the sub-day remainder is applied after the
    // roll, so a timestamp keeps its time of day across closures.
    const std::int64_t whole_days = floor_div(delta, nanoseconds_per_day);
    const std::int64_t remainder = delta - whole_days * nanoseconds_per_day;
    const std::int64_t rolled = roll_day(calendar, input, whole_days);
    const std::int64_t time_of_day = input - day_number(input) * nanoseconds_per_day;
    const std::int64_t result = rolled * nanoseconds_per_day + time_of_day + remainder;
    if (result < std::numeric_limits<std::int64_t>::min() ||
        result > std::numeric_limits<std::int64_t>::max()) {
        throw factor_data_error{"calendar timedelta overflow"};
    }
    return result;
}

std::vector<timestamp> add_custom_calendar_timedelta(
    const std::vector<timestamp>& input, const std::int64_t delta,
    const trading_calendar& calendar) {
    std::vector<timestamp> result;
    result.reserve(input.size());
    for (const timestamp value : input) {
        result.push_back(add_custom_calendar_timedelta(value, delta, calendar));
    }
    return result;
}

std::int64_t diff_custom_calendar_timedeltas(
    const timestamp start, const timestamp end,
    const trading_calendar& calendar) {
    // Only the whole-day part of the wall gap is replaced by the session count,
    // so a partial final day stays in the nanosecond remainder instead of being
    // rounded up into the number of trading days between the two timestamps.
    const std::int64_t start_day = day_number(start);
    const std::int64_t end_day = day_number(end);
    const std::int64_t actual_days = calendar.busday_count(start_day, end_day);
    const std::int64_t wall = end - start;
    const std::int64_t component_days = floor_div(wall, nanoseconds_per_day);
    return wall - (component_days - actual_days) * nanoseconds_per_day;
}

std::string timedelta_to_string(const std::int64_t delta) {
    const std::int64_t days = floor_div(delta, nanoseconds_per_day);
    std::int64_t remainder = delta - days * nanoseconds_per_day;
    const std::int64_t hours = remainder / nanoseconds_per_hour;
    remainder -= hours * nanoseconds_per_hour;
    const std::int64_t minutes = remainder / nanoseconds_per_minute;
    remainder -= minutes * nanoseconds_per_minute;
    const std::int64_t seconds = remainder / nanoseconds_per_second;
    remainder -= seconds * nanoseconds_per_second;
    const std::int64_t milliseconds = remainder / nanoseconds_per_millisecond;
    remainder -= milliseconds * nanoseconds_per_millisecond;
    const std::int64_t microseconds = remainder / nanoseconds_per_microsecond;
    const std::int64_t nanoseconds = remainder - microseconds * nanoseconds_per_microsecond;

    std::string result;
    if (days != 0) {
        result += std::to_string(days);
        result.push_back('D');
    }
    const std::pair<std::int64_t, const char*> components[] = {
        {hours, "h"}, {minutes, "m"}, {seconds, "s"},
        {milliseconds, "ms"}, {microseconds, "us"}, {nanoseconds, "ns"}};
    for (const auto& [value, suffix] : components) {
        if (value != 0) {
            result += std::to_string(value);
            result += suffix;
        }
    }
    return result;
}

std::vector<int> timedelta_strings_to_integers(
    const std::vector<std::string>& sequence) {
    std::vector<int> result;
    result.reserve(sequence.size());
    for (const std::string& label : sequence) {
        const std::int64_t nanoseconds = parse_timedelta_nanoseconds(label);
        const std::int64_t days = floor_div(nanoseconds, nanoseconds_per_day);
        if (days < std::numeric_limits<int>::min() ||
            days > std::numeric_limits<int>::max()) {
            throw factor_data_error{"timedelta day component is out of range"};
        }
        result.push_back(static_cast<int>(days));
    }
    return result;
}

std::int64_t parse_timedelta_nanoseconds(const std::string& label) {
    if (label.empty()) {
        throw factor_data_error{"timedelta label must not be empty"};
    }
    std::size_t position = 0U;
    bool negative = false;
    if (label[position] == '+' || label[position] == '-') {
        negative = label[position] == '-';
        ++position;
    }
    if (position == label.size()) {
        throw factor_data_error{"timedelta label must contain a component"};
    }
    std::int64_t total = 0;
    while (position < label.size()) {
        const std::int64_t value = parse_signed_number(label, position);
        if (position >= label.size()) {
            throw factor_data_error{"timedelta component must name a unit"};
        }
        const char unit = label[position];
        ++position;
        if ((unit == 'm' || unit == 'M' || unit == 'u' || unit == 'U' ||
             unit == 'n' || unit == 'N') &&
            position < label.size() &&
            (label[position] == 's' || label[position] == 'S')) {
            const char prefix = static_cast<char>(
                std::tolower(static_cast<unsigned char>(unit)));
            ++position;
            const std::int64_t multiplier =
                prefix == 'm' ? nanoseconds_per_millisecond
                              : prefix == 'u' ? nanoseconds_per_microsecond
                                              : 1LL;
            total += value * multiplier;
            continue;
        }
        total += value * unit_multiplier(unit);
    }
    return negative ? -total : total;
}

}  // namespace fincore
