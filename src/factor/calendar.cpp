#include <fincore/factor/calendar.hpp>

#include <fincore/factor/exceptions.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fincore {
namespace {

struct code_point_range {
    char32_t first;
    char32_t last;
};

// Unicode 14.0 decimal-number ranges: Python's pinned ``\d`` regex accepts
// every code point in these ranges.
constexpr std::array<code_point_range, 62> unicode_decimal_digits{{
    {0x0030, 0x0039}, {0x0660, 0x0669}, {0x06F0, 0x06F9},
    {0x07C0, 0x07C9}, {0x0966, 0x096F}, {0x09E6, 0x09EF},
    {0x0A66, 0x0A6F}, {0x0AE6, 0x0AEF}, {0x0B66, 0x0B6F},
    {0x0BE6, 0x0BEF}, {0x0C66, 0x0C6F}, {0x0CE6, 0x0CEF},
    {0x0D66, 0x0D6F}, {0x0DE6, 0x0DEF}, {0x0E50, 0x0E59},
    {0x0ED0, 0x0ED9}, {0x0F20, 0x0F29}, {0x1040, 0x1049},
    {0x1090, 0x1099}, {0x17E0, 0x17E9}, {0x1810, 0x1819},
    {0x1946, 0x194F}, {0x19D0, 0x19D9}, {0x1A80, 0x1A89},
    {0x1A90, 0x1A99}, {0x1B50, 0x1B59}, {0x1BB0, 0x1BB9},
    {0x1C40, 0x1C49}, {0x1C50, 0x1C59}, {0xA620, 0xA629},
    {0xA8D0, 0xA8D9}, {0xA900, 0xA909}, {0xA9D0, 0xA9D9},
    {0xA9F0, 0xA9F9}, {0xAA50, 0xAA59}, {0xABF0, 0xABF9},
    {0xFF10, 0xFF19}, {0x104A0, 0x104A9}, {0x10D30, 0x10D39},
    {0x11066, 0x1106F}, {0x110F0, 0x110F9}, {0x11136, 0x1113F},
    {0x111D0, 0x111D9}, {0x112F0, 0x112F9}, {0x11450, 0x11459},
    {0x114D0, 0x114D9}, {0x11650, 0x11659}, {0x116C0, 0x116C9},
    {0x11730, 0x11739}, {0x118E0, 0x118E9}, {0x11950, 0x11959},
    {0x11C50, 0x11C59}, {0x11D50, 0x11D59}, {0x11DA0, 0x11DA9},
    {0x16A60, 0x16A69}, {0x16AC0, 0x16AC9}, {0x16B50, 0x16B59},
    {0x1D7CE, 0x1D7FF}, {0x1E140, 0x1E149}, {0x1E2F0, 0x1E2F9},
    {0x1E950, 0x1E959}, {0x1FBF0, 0x1FBF9}}};

// The binary search compares range ends, so the table above has to stay sorted
// by last code point; the accepted candidate is then checked against its own
// start, which is what makes the disjoint ranges testable in one probe.
bool is_unicode_decimal_digit(const char32_t value) noexcept {
    const auto found = std::lower_bound(
        unicode_decimal_digits.begin(), unicode_decimal_digits.end(), value,
        [](const code_point_range& range, const char32_t code_point) {
            return range.last < code_point;
        });
    return found != unicode_decimal_digits.end() && found->first <= value;
}

// Well-formedness is checked rather than assumed: the 0xC0 and 0xC1 leads are
// excluded because their only encodings are overlong, and surrogates, values
// above U+10FFFF and truncated sequences all fail. A label that does not decode
// is therefore treated as not a forward-return column rather than as a match
// with unknown characters.
bool decode_utf8(const std::string& text,
                 std::vector<char32_t>& code_points) {
    std::size_t position = 0U;
    while (position < text.size()) {
        const auto first = static_cast<unsigned char>(text[position]);
        if (first <= 0x7FU) {
            code_points.push_back(first);
            ++position;
            continue;
        }

        std::size_t continuation_count = 0U;
        char32_t code_point = 0U;
        if (first >= 0xC2U && first <= 0xDFU) {
            continuation_count = 1U;
            code_point = first & 0x1FU;
        } else if (first >= 0xE0U && first <= 0xEFU) {
            continuation_count = 2U;
            code_point = first & 0x0FU;
        } else if (first >= 0xF0U && first <= 0xF4U) {
            continuation_count = 3U;
            code_point = first & 0x07U;
        } else {
            return false;
        }
        if (continuation_count > text.size() - position - 1U) {
            return false;
        }
        for (std::size_t offset = 1U; offset <= continuation_count; ++offset) {
            const auto continuation =
                static_cast<unsigned char>(text[position + offset]);
            if ((continuation & 0xC0U) != 0x80U) {
                return false;
            }
            code_point = (code_point << 6U) | (continuation & 0x3FU);
        }
        if ((continuation_count == 2U && code_point < 0x800U) ||
            (continuation_count == 3U && code_point < 0x10000U) ||
            (code_point >= 0xD800U && code_point <= 0xDFFFU) ||
            code_point > 0x10FFFFU) {
            return false;
        }
        code_points.push_back(code_point);
        position += continuation_count + 1U;
    }
    return true;
}

char32_t ascii_lower(const char32_t value) noexcept {
    if (value >= 'A' && value <= 'Z') {
        return value - 'A' + 'a';
    }
    return value;
}

bool is_s_case_variant(const char32_t value) noexcept {
    return ascii_lower(value) == 's' || value == 0x017FU;
}

// Every digit run has to be followed by a unit, so a bare number never counts.
// Day, hour, minute and second stand alone, while the micro- and nanosecond
// forms are recognized only together with their trailing s, which the long-s
// code point also satisfies; exact-day mode narrows every run to d.
bool matches_forward_return_label(const std::string& label,
                                  const bool exact_day_multiple) {
    std::vector<char32_t> code_points;
    if (!decode_utf8(label, code_points) || code_points.empty()) {
        return false;
    }

    std::size_t position = 0U;
    while (position < code_points.size()) {
        const std::size_t digit_start = position;
        while (position < code_points.size() &&
               is_unicode_decimal_digit(code_points[position])) {
            ++position;
        }
        if (position == digit_start || position == code_points.size()) {
            return false;
        }

        if (exact_day_multiple) {
            if (ascii_lower(code_points[position]) != 'd') {
                return false;
            }
            ++position;
            continue;
        }

        const char32_t first = ascii_lower(code_points[position]);
        if (position + 1U < code_points.size() &&
            is_s_case_variant(code_points[position + 1U]) &&
            (first == 'm' || first == 'u' || first == 'n')) {
            position += 2U;
            continue;
        }
        if (first == 'd' || first == 'h' || first == 'm' ||
            is_s_case_variant(code_points[position])) {
            ++position;
            continue;
        }
        return false;
    }
    return true;
}

}  // namespace

std::vector<std::string> get_forward_returns_columns(
    const std::vector<std::string>& columns,
    const bool require_exact_day_multiple) {
    std::vector<std::string> result;
    result.reserve(columns.size());
    for (const auto& column : columns) {
        if (matches_forward_return_label(column,
                                         require_exact_day_multiple)) {
            result.push_back(column);
        }
    }
    return result;
}

factor_series backshift_returns_series(
    const factor_series& series,
    const std::vector<timestamp>& date_levels,
    const int N) {
    if (N <= 0) {
        throw factor_data_error{"N must be a positive integer"};
    }
    if (series.dates.size() != series.assets.size() ||
        series.dates.size() != series.values.size()) {
        throw factor_data_error{
            "factor dates, assets and values must have equal lengths"};
    }
    if (!std::is_sorted(date_levels.begin(), date_levels.end()) ||
        std::adjacent_find(date_levels.begin(), date_levels.end()) !=
            date_levels.end()) {
        throw factor_data_error{"date levels must be sorted and unique"};
    }

    std::vector<std::size_t> date_positions;
    date_positions.reserve(series.dates.size());
    for (const timestamp date : series.dates) {
        const auto found =
            std::lower_bound(date_levels.begin(), date_levels.end(), date);
        if (found == date_levels.end() || *found != date) {
            throw factor_data_error{
                "factor row date is missing from date levels"};
        }
        date_positions.push_back(
            static_cast<std::size_t>(found - date_levels.begin()));
    }

    // Calendar levels, not rows, define the distance: a value at level five with
    // N of two is relabeled to level three even when the input has no row at
    // level three or four. Rows whose level lies inside the shift are dropped
    // rather than padded, and a frame with fewer levels than the shift has
    // nowhere left to land, so it returns empty.
    const auto shift = static_cast<std::size_t>(N);
    factor_series shifted;
    if (date_levels.size() <= shift) {
        return shifted;
    }
    shifted.dates.reserve(series.dates.size());
    shifted.assets.reserve(series.assets.size());
    shifted.values.reserve(series.values.size());
    for (std::size_t row = 0; row < date_positions.size(); ++row) {
        if (date_positions[row] < shift) {
            continue;
        }
        shifted.dates.push_back(date_levels[date_positions[row] - shift]);
        shifted.assets.push_back(series.assets[row]);
        shifted.values.push_back(series.values[row]);
    }
    return shifted;
}

}  // namespace fincore
