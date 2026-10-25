#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>

namespace fincore::detail {

// Below this size, stable_sort avoids the setup cost of eight radix passes.
inline constexpr std::size_t stable_double_order_radix_threshold = 1024U;

// Stable-sort indices by their associated double value. value_at must return
// a double, and callers must remove NaNs first because ordinary floating-point
// less-than does not order them. Infinities are supported. On IEEE-754 binary64
// platforms, the radix path sorts transformed integer keys without floating-
// point arithmetic; other platforms use stable_sort. scratch must be a distinct
// vector and may be retained across calls to reuse its allocation.
template <typename Index, typename ValueAt>
void stable_sort_indices_by_double_value(std::vector<Index>& indices,
                                         std::vector<Index>& scratch,
                                         ValueAt&& value_at) {
    const auto value_of = [&value_at](const Index& index) {
        return static_cast<double>(value_at(index));
    };
    const auto less = [&value_of](const Index& left, const Index& right) {
        return value_of(left) < value_of(right);
    };

    if (indices.size() < stable_double_order_radix_threshold) {
        std::stable_sort(indices.begin(), indices.end(), less);
        return;
    }

    if constexpr (sizeof(double) == sizeof(std::uint64_t) &&
                  std::numeric_limits<double>::is_iec559 &&
                  std::numeric_limits<double>::radix == 2 &&
                  std::numeric_limits<double>::digits == 53 &&
                  std::numeric_limits<double>::max_exponent == 1024 &&
                  std::is_trivially_copyable_v<Index>) {
        scratch.resize(indices.size());

        constexpr std::uint64_t sign_mask =
            std::uint64_t{1U} << 63U;
        const auto key_for = [&value_at](const Index& index) {
            auto bits = std::bit_cast<std::uint64_t>(
                static_cast<double>(value_at(index)));
            // IEEE comparison considers both zero encodings equal. Give them
            // one key so the stable passes preserve their original row order.
            if ((bits & ~sign_mask) == 0U) {
                bits = 0U;
            }
            return (bits & sign_mask) != 0U ? ~bits : (bits ^ sign_mask);
        };

        std::array<std::array<std::size_t, 256U>, 8U> digit_counts{};
        for (const auto& index : indices) {
            const auto key = key_for(index);
            for (unsigned pass = 0U; pass < digit_counts.size(); ++pass) {
                const auto digit = static_cast<std::size_t>(
                    (key >> (pass * 8U)) & 0xffU);
                ++digit_counts[pass][digit];
            }
        }

        std::vector<Index>* source = &indices;
        std::vector<Index>* destination = &scratch;
        for (unsigned pass = 0U; pass < digit_counts.size(); ++pass) {
            auto offsets = digit_counts[pass];
            std::size_t populated_buckets = 0U;
            for (const auto count : offsets) {
                if (count != 0U) {
                    ++populated_buckets;
                }
            }
            if (populated_buckets <= 1U) {
                continue;
            }

            std::size_t next_offset = 0U;
            for (auto& offset : offsets) {
                const auto count = offset;
                offset = next_offset;
                next_offset += count;
            }
            const auto shift = pass * 8U;
            for (const auto& index : *source) {
                const auto digit = static_cast<std::size_t>(
                    (key_for(index) >> shift) & 0xffU);
                (*destination)[offsets[digit]++] = index;
            }
            std::swap(source, destination);
        }
        if (source != &indices) {
            std::copy(source->begin(), source->end(), indices.begin());
        }
    } else {
        std::stable_sort(indices.begin(), indices.end(), less);
    }
}

}  // namespace fincore::detail
