#pragma once

#include <fincore/metrics/basic.hpp>

#include <string_view>

namespace fincore {

inline constexpr std::string_view daily_frequency_name{"daily"};
inline constexpr std::string_view weekly_frequency_name{"weekly"};
inline constexpr std::string_view monthly_frequency_name{"monthly"};
inline constexpr std::string_view quarterly_frequency_name{"quarterly"};
inline constexpr std::string_view yearly_frequency_name{"yearly"};

// Map one canonical source frequency string to the native `frequency` enum.
//
// Source contract (`fincore.metrics.frequencies.pandas_frequency`): only the
// five canonical names are accepted; any other value raises ValueError with
// the "Period cannot be ...: unknown frequency" message. The native function
// raises `input_error` with the same predicate and message text.
[[nodiscard]] frequency pandas_frequency(std::string_view period);

// pandas resampling alias for the pinned pandas (>= 2.2) environment:
// D, W, ME, QE, YE. Exposed as provenance for the source mapping; the native
// code consumes the enum, not the alias string.
[[nodiscard]] std::string_view pandas_frequency_alias(frequency period);

// Canonical source name for a native frequency.
[[nodiscard]] std::string_view frequency_name(frequency period);

}  // namespace fincore
