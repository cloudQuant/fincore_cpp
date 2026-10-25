#pragma once

#include <fincore/metrics/basic.hpp>

#include <string_view>

namespace fincore {

/// Canonical source name of the daily frequency ("daily").
inline constexpr std::string_view daily_frequency_name{"daily"};
/// Canonical source name of the weekly frequency ("weekly").
inline constexpr std::string_view weekly_frequency_name{"weekly"};
/// Canonical source name of the monthly frequency ("monthly").
inline constexpr std::string_view monthly_frequency_name{"monthly"};
/// Canonical source name of the quarterly frequency ("quarterly").
inline constexpr std::string_view quarterly_frequency_name{"quarterly"};
/// Canonical source name of the yearly frequency ("yearly").
inline constexpr std::string_view yearly_frequency_name{"yearly"};

/// Maps one canonical source frequency string to the native `frequency` enum.
///
/// Source contract (`fincore.metrics.frequencies.pandas_frequency`): only the
/// five canonical names are accepted; any other value raises ValueError with
/// the "Period cannot be ...: unknown frequency" message. The native function
/// raises `input_error` with the same predicate and message text.
[[nodiscard]] frequency pandas_frequency(std::string_view period);

/// Returns the pandas resampling alias for `period` (D, W, ME, QE, YE).
///
/// Alias for the pinned pandas (>= 2.2) environment, exposed as provenance for
/// the source mapping; the native code consumes the enum, not the alias string.
[[nodiscard]] std::string_view pandas_frequency_alias(frequency period);

/// Returns the canonical source name for a native frequency.
[[nodiscard]] std::string_view frequency_name(frequency period);

}  // namespace fincore
