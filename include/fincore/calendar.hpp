#pragma once

#include <fincore/dataframe.hpp>

namespace fincore {

/// Returns the UTC Unix-epoch timestamp for a calendar date and time.
///
/// Components are interpreted in UTC. The month and day default to January 1,
/// and the time components default to midnight. Sub-second precision is carried
/// in nanoseconds.
///
/// @param year Calendar year supported by `std::chrono::year`.
/// @param month Month in [1, 12]; defaults to January.
/// @param day Day of month in [1, 31]; defaults to 1.
/// @param hour Hour in [0, 23]; defaults to 0.
/// @param minute Minute in [0, 59]; defaults to 0.
/// @param second Second in [0, 59]; defaults to 0.
/// @param nanosecond Nanosecond in [0, 999999999]; defaults to 0.
/// @return Signed nanoseconds since 1970-01-01T00:00:00Z.
/// @throws input_error If a component is out of range, the calendar date does
///     not exist, or the timestamp is outside the representable int64 range.
timestamp utc_timestamp(int year,
                        unsigned month = 1,
                        unsigned day = 1,
                        unsigned hour = 0,
                        unsigned minute = 0,
                        unsigned second = 0,
                        unsigned nanosecond = 0);

/// Returns the UTC calendar year containing `value`.
///
/// @param value Signed nanosecond timestamp since the Unix epoch.
/// @return UTC calendar year, including years before 1970.
int utc_year(timestamp value) noexcept;

}  // namespace fincore
