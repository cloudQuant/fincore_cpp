#include <fincore/calendar.hpp>

#include <chrono>
#include <cstdint>
#include <limits>

namespace fincore {
namespace {

constexpr std::int64_t nanoseconds_per_second = 1'000'000'000LL;
constexpr std::int64_t nanoseconds_per_minute = 60LL * nanoseconds_per_second;
constexpr std::int64_t nanoseconds_per_hour = 60LL * nanoseconds_per_minute;
constexpr std::int64_t nanoseconds_per_day = 24LL * nanoseconds_per_hour;

void reject_invalid_components(unsigned hour,
                               unsigned minute,
                               unsigned second,
                               unsigned nanosecond) {
    if (hour > 23 || minute > 59 || second > 59 ||
        nanosecond >= static_cast<unsigned>(nanoseconds_per_second)) {
        throw input_error{"UTC timestamp contains an invalid time component"};
    }
}

}  // namespace

timestamp utc_timestamp(int year_value,
                        unsigned month_value,
                        unsigned day_value,
                        unsigned hour,
                        unsigned minute,
                        unsigned second,
                        unsigned nanosecond) {
    using namespace std::chrono;

    const auto minimum_year = static_cast<int>(year::min());
    const auto maximum_year = static_cast<int>(year::max());
    if (year_value < minimum_year || year_value > maximum_year) {
        throw input_error{"UTC timestamp year is outside the supported range"};
    }

    const year_month_day calendar_day{
        year{year_value}, month{month_value}, day{day_value}};
    if (!calendar_day.ok()) {
        throw input_error{"UTC timestamp contains an invalid calendar date"};
    }
    reject_invalid_components(hour, minute, second, nanosecond);

    const sys_days midnight{calendar_day};
    const std::int64_t day_count = midnight.time_since_epoch().count();
    constexpr auto minimum_timestamp = std::numeric_limits<timestamp>::min();
    constexpr auto maximum_timestamp = std::numeric_limits<timestamp>::max();
    if (day_count < minimum_timestamp / nanoseconds_per_day ||
        day_count > maximum_timestamp / nanoseconds_per_day) {
        throw input_error{"UTC timestamp is outside the representable range"};
    }

    const std::int64_t time_of_day =
        static_cast<std::int64_t>(hour) * nanoseconds_per_hour +
        static_cast<std::int64_t>(minute) * nanoseconds_per_minute +
        static_cast<std::int64_t>(second) * nanoseconds_per_second + nanosecond;
    const timestamp timestamp_at_midnight = day_count * nanoseconds_per_day;
    if (timestamp_at_midnight > maximum_timestamp - time_of_day) {
        throw input_error{"UTC timestamp is outside the representable range"};
    }

    return timestamp_at_midnight + time_of_day;
}

int utc_year(timestamp value) noexcept {
    using namespace std::chrono;

    const sys_time<nanoseconds> point{nanoseconds{value}};
    const sys_days day{floor<days>(point)};
    return static_cast<int>(year_month_day{day}.year());
}

}  // namespace fincore
