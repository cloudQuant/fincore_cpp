#include <fincore/metrics/frequencies.hpp>

#include <fincore/error.hpp>

#include <string>
#include <string_view>

namespace fincore {
namespace {

[[noreturn]] void throw_unknown_frequency(const std::string_view period) {
    std::string message{"Period cannot be '"};
    message += period;
    message +=
        "': unknown frequency; expected one of ('daily', 'weekly', 'monthly', "
        "'quarterly', 'yearly')";
    throw input_error{message};
}

void validate_frequency(const frequency period) {
    switch (period) {
    case frequency::daily:
    case frequency::weekly:
    case frequency::monthly:
    case frequency::quarterly:
    case frequency::yearly:
        return;
    }
    throw input_error{"unrecognized frequency"};
}

}  // namespace

frequency pandas_frequency(const std::string_view period) {
    if (period == daily_frequency_name) {
        return frequency::daily;
    }
    if (period == weekly_frequency_name) {
        return frequency::weekly;
    }
    if (period == monthly_frequency_name) {
        return frequency::monthly;
    }
    if (period == quarterly_frequency_name) {
        return frequency::quarterly;
    }
    if (period == yearly_frequency_name) {
        return frequency::yearly;
    }
    throw_unknown_frequency(period);
}

// The aliases are the current pandas spellings for period-END labels (ME, QE, YE)
// rather than the deprecated M/Q/Y forms, so a frequency converted here can be
// handed straight to a modern date_range call and mean the same boundary.
std::string_view pandas_frequency_alias(const frequency period) {
    validate_frequency(period);
    switch (period) {
    case frequency::daily:
        return "D";
    case frequency::weekly:
        return "W";
    case frequency::monthly:
        return "ME";
    case frequency::quarterly:
        return "QE";
    case frequency::yearly:
        return "YE";
    }
    throw input_error{"unrecognized frequency"};
}

std::string_view frequency_name(const frequency period) {
    validate_frequency(period);
    switch (period) {
    case frequency::daily:
        return daily_frequency_name;
    case frequency::weekly:
        return weekly_frequency_name;
    case frequency::monthly:
        return monthly_frequency_name;
    case frequency::quarterly:
        return quarterly_frequency_name;
    case frequency::yearly:
        return yearly_frequency_name;
    }
    throw input_error{"unrecognized frequency"};
}

}  // namespace fincore
