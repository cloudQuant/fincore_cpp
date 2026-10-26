#pragma once

#include <fincore/factor/data.hpp>
#include <fincore/factor/timedelta.hpp>

#include <string>
#include <vector>

namespace fincore {

/// Select labels matching the pinned factor forward-return grammar.
///
/// Results keep the input order and duplicates. Invalid labels are skipped.
/// With require_exact_day_multiple, only repeated day components are kept;
/// unlike the Python implementation, the native helper skips silently instead
/// of emitting a warning.
std::vector<std::string> get_forward_returns_columns(
    const std::vector<std::string>& columns,
    bool require_exact_day_multiple = false);

/// Move a backward-looking return series N date-level positions earlier.
///
/// date_levels is the complete, sorted, unique date level from the source
/// MultiIndex, including unused dates. Rows with level positions below N are
/// discarded. Remaining rows retain their order, assets and values (including
/// NaNs) while their dates move to the level N positions earlier. A result is
/// empty when date_levels.size() <= N.
factor_series backshift_returns_series(
    const factor_series& series,
    const std::vector<timestamp>& date_levels,
    int N);

}  // namespace fincore
