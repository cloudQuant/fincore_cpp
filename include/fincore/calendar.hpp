#pragma once

#include <fincore/dataframe.hpp>

namespace fincore {

timestamp utc_timestamp(int year,
                        unsigned month,
                        unsigned day,
                        unsigned hour = 0,
                        unsigned minute = 0,
                        unsigned second = 0,
                        unsigned nanosecond = 0);

int utc_year(timestamp value) noexcept;

}  // namespace fincore
