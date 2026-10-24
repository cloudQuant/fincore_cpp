#pragma once

#include <fincore/dataframe.hpp>
#include <fincore/error.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fincore::detail {

inline void append_python_float(std::string& result, const double value,
                               const std::string_view context) {
    if (!std::isfinite(value)) {
        throw input_error{std::string(context) +
                          " must not contain non-finite numeric values"};
    }
    if (value == 0.0) {
        result += std::signbit(value) ? "-0.0" : "0.0";
        return;
    }

    char buffer[64];
    const auto conversion =
        std::to_chars(buffer, buffer + sizeof(buffer), value);
    if (conversion.ec != std::errc{}) {
        throw input_error{std::string(context) +
                          " could not be serialized"};
    }
    const char* text = buffer;
    const auto length = static_cast<std::size_t>(conversion.ptr - buffer);
    std::size_t position = 0U;
    const bool negative = length > 0U && text[0] == '-';
    if (negative) {
        ++position;
    }
    char digits[64];
    std::size_t digit_count = 0U;
    std::size_t digits_before_point = 0U;
    bool seen_point = false;
    while (position < length && text[position] != 'e' &&
           text[position] != 'E') {
        if (text[position] == '.') {
            seen_point = true;
        } else {
            digits[digit_count++] = text[position];
            if (!seen_point) {
                ++digits_before_point;
            }
        }
        ++position;
    }
    int exponent = 0;
    if (position < length) {
        int sign = 1;
        ++position;
        if (position < length && (text[position] == '+' ||
                                  text[position] == '-')) {
            sign = text[position] == '-' ? -1 : 1;
            ++position;
        }
        int magnitude = 0;
        while (position < length) {
            magnitude = magnitude * 10 + (text[position] - '0');
            ++position;
        }
        exponent = sign * magnitude;
    }
    const int decimal_position =
        static_cast<int>(digits_before_point) + exponent;

    if (negative) {
        result.push_back('-');
    }
    if (decimal_position <= -4 || decimal_position > 16) {
        result.push_back(digits[0]);
        if (digit_count > 1U) {
            result.push_back('.');
            result.append(digits + 1U, digit_count - 1U);
        }
        const int scientific_exponent = decimal_position - 1;
        result.push_back('e');
        result.push_back(scientific_exponent < 0 ? '-' : '+');
        const int magnitude = std::abs(scientific_exponent);
        if (magnitude < 10) {
            result.push_back('0');
        }
        char exponent_buffer[16];
        const auto exponent_conversion = std::to_chars(
            exponent_buffer, exponent_buffer + sizeof(exponent_buffer),
            magnitude);
        result.append(exponent_buffer,
                      static_cast<std::size_t>(exponent_conversion.ptr -
                                               exponent_buffer));
        return;
    }

    if (decimal_position <= 0) {
        result += "0.";
        result.append(static_cast<std::size_t>(-decimal_position), '0');
        result.append(digits, digit_count);
        return;
    }
    if (static_cast<std::size_t>(decimal_position) >= digit_count) {
        result.append(digits, digit_count);
        result.append(
            static_cast<std::size_t>(decimal_position) - digit_count, '0');
        result += ".0";
        return;
    }
    result.append(digits, static_cast<std::size_t>(decimal_position));
    result.push_back('.');
    result.append(digits + decimal_position,
                  digit_count - static_cast<std::size_t>(decimal_position));
}

inline std::string python_float(const double value,
                         const std::string_view context) {
    std::string result;
    append_python_float(result, value, context);
    return result;
}

inline std::string format_timestamp(const timestamp value) {
    constexpr std::int64_t nanoseconds_per_second = 1'000'000'000LL;
    constexpr std::int64_t seconds_per_day = 86'400LL;
    std::int64_t seconds = value / nanoseconds_per_second;
    std::int64_t nanoseconds = value % nanoseconds_per_second;
    if (nanoseconds < 0) {
        nanoseconds += nanoseconds_per_second;
        --seconds;
    }
    std::int64_t days = seconds / seconds_per_day;
    std::int64_t seconds_of_day = seconds % seconds_per_day;
    if (seconds_of_day < 0) {
        seconds_of_day += seconds_per_day;
        --days;
    }

    days += 719468;
    const std::int64_t era =
        (days >= 0 ? days : days - 146096) / 146097;
    const auto day_of_era =
        static_cast<std::uint64_t>(days - era * 146097);
    const auto year_of_era =
        (day_of_era - day_of_era / 1460U + day_of_era / 36524U -
         day_of_era / 146096U) /
        365U;
    const std::int64_t year_base =
        static_cast<std::int64_t>(year_of_era) + era * 400;
    const auto day_of_year =
        day_of_era -
        (365U * year_of_era + year_of_era / 4U - year_of_era / 100U);
    const auto month_prime = (5U * day_of_year + 2U) / 153U;
    const auto day = day_of_year - (153U * month_prime + 2U) / 5U + 1U;
    const auto month = month_prime < 10U ? month_prime + 3U
                                         : month_prime - 9U;
    const std::int64_t year =
        year_base + (month <= 2U ? 1 : 0);

    char buffer[64];
    std::snprintf(
        buffer, sizeof(buffer), "%04lld-%02d-%02dT%02d:%02d:%02d",
        static_cast<long long>(year), static_cast<int>(month),
        static_cast<int>(day),
        static_cast<int>(seconds_of_day / 3600),
        static_cast<int>((seconds_of_day % 3600) / 60),
        static_cast<int>(seconds_of_day % 60));
    std::string result(buffer);
    if (nanoseconds == 0) {
        return result;
    }
    result.push_back('.');
    if (nanoseconds % 1000 == 0) {
        std::snprintf(buffer, sizeof(buffer), "%06lld",
                      static_cast<long long>(nanoseconds / 1000));
    } else {
        std::snprintf(buffer, sizeof(buffer), "%09lld",
                      static_cast<long long>(nanoseconds));
    }
    result += buffer;
    return result;
}

inline void append_unicode_escape(std::string& output, std::uint32_t code_point) {
    if (code_point > 0x10FFFFU) {
        throw input_error{"report text must be valid UTF-8"};
    }
    static constexpr char hex[] = "0123456789abcdef";
    const auto append_unit = [&output](const std::uint16_t unit) {
        output += "\\u";
        output.push_back(hex[(unit >> 12U) & 0xFU]);
        output.push_back(hex[(unit >> 8U) & 0xFU]);
        output.push_back(hex[(unit >> 4U) & 0xFU]);
        output.push_back(hex[unit & 0xFU]);
    };
    if (code_point <= 0xFFFFU) {
        append_unit(static_cast<std::uint16_t>(code_point));
        return;
    }
    const std::uint32_t adjusted = code_point - 0x10000U;
    append_unit(static_cast<std::uint16_t>(0xD800U + (adjusted >> 10U)));
    append_unit(static_cast<std::uint16_t>(0xDC00U + (adjusted & 0x3FFU)));
}

inline void append_json_string(std::string& output, const std::string_view text) {
    output.push_back('"');
    for (std::size_t position = 0U; position < text.size();) {
        const auto character =
            static_cast<unsigned char>(text[position]);
        if (character < 0x80U) {
            switch (character) {
                case '"':
                    output += "\\\"";
                    break;
                case '\\':
                    output += "\\\\";
                    break;
                case '\b':
                    output += "\\b";
                    break;
                case '\f':
                    output += "\\f";
                    break;
                case '\n':
                    output += "\\n";
                    break;
                case '\r':
                    output += "\\r";
                    break;
                case '\t':
                    output += "\\t";
                    break;
                default:
                    if (character < 0x20U) {
                        append_unicode_escape(output, character);
                    } else {
                        output.push_back(static_cast<char>(character));
                    }
                    break;
            }
            ++position;
            continue;
        }

        std::uint32_t code_point = 0U;
        std::size_t length = 0U;
        if ((character & 0xE0U) == 0xC0U) {
            code_point = character & 0x1FU;
            length = 2U;
        } else if ((character & 0xF0U) == 0xE0U) {
            code_point = character & 0x0FU;
            length = 3U;
        } else if ((character & 0xF8U) == 0xF0U) {
            code_point = character & 0x07U;
            length = 4U;
        } else {
            throw input_error{"report text must be valid UTF-8"};
        }
        if (position + length > text.size()) {
            throw input_error{"report text must be valid UTF-8"};
        }
        for (std::size_t offset = 1U; offset < length; ++offset) {
            const auto continuation =
                static_cast<unsigned char>(text[position + offset]);
            if ((continuation & 0xC0U) != 0x80U) {
                throw input_error{"report text must be valid UTF-8"};
            }
            code_point = (code_point << 6U) | (continuation & 0x3FU);
        }
        append_unicode_escape(output, code_point);
        position += length;
    }
    output.push_back('"');
}

class json_writer {
  public:
    explicit json_writer(std::string& output) : output_(output) {}

    void begin_object() {
        value_prefix();
        output_.push_back('{');
        levels_.push_back({true, true});
    }

    void end_object() {
        const bool empty = levels_.back().first;
        levels_.pop_back();
        close_container('}', empty);
    }

    void begin_array() {
        value_prefix();
        output_.push_back('[');
        levels_.push_back({true, false});
    }

    void end_array() {
        const bool empty = levels_.back().first;
        levels_.pop_back();
        close_container(']', empty);
    }

    void key(const std::string_view name) {
        auto& current = levels_.back();
        if (!current.first) {
            output_.push_back(',');
        }
        current.first = false;
        output_.push_back('\n');
        append_indent(levels_.size());
        append_json_string(output_, name);
        output_ += ": ";
        after_key_ = true;
    }

    void value_string(const std::string_view value) {
        value_prefix();
        append_json_string(output_, value);
    }

    void value_number(const double value, const std::string_view context) {
        value_prefix();
        output_ += python_float(value, context);
    }

    void value_integer(const std::int64_t value) {
        value_prefix();
        output_ += std::to_string(value);
    }

    void value_boolean(const bool value) {
        value_prefix();
        output_ += value ? "true" : "false";
    }

    void value_null() {
        value_prefix();
        output_ += "null";
    }

  private:
    struct level {
        bool first;
        bool object;
    };

    void value_prefix() {
        if (after_key_) {
            after_key_ = false;
            return;
        }
        if (levels_.empty()) {
            return;
        }
        auto& current = levels_.back();
        if (!current.first) {
            output_.push_back(',');
        }
        current.first = false;
        output_.push_back('\n');
        append_indent(levels_.size());
    }

    void close_container(const char closing, const bool empty) {
        if (!empty) {
            output_.push_back('\n');
            append_indent(levels_.size());
        }
        output_.push_back(closing);
    }

    void append_indent(const std::size_t depth) {
        output_.append(depth * 2U, ' ');
    }

    std::string& output_;
    std::vector<level> levels_;
    bool after_key_ = false;
};

}  // namespace fincore::detail
