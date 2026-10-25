#pragma once

#include <fincore/metrics/returns.hpp>

#include <optional>
#include <string_view>

namespace fincore {

/// Compounding period of a return series.
enum class frequency {
    daily,     ///< Daily observations, annualized with 252 periods.
    weekly,    ///< Weekly observations, annualized with 52 periods.
    monthly,   ///< Monthly observations, annualized with 12 periods.
    quarterly, ///< Quarterly observations, annualized with 4 periods.
    yearly,    ///< Yearly observations, annualized with 1 period.
};

/// Returns the number of periods per year used to annualize `period`.
///
/// An explicit `annualization` value is returned unchanged; otherwise the
/// defaults are 252, 52, 12, 4 and 1 for daily through yearly observations.
double annualization_factor(frequency period,
                            std::optional<double> annualization = std::nullopt);

/// Returns a copy of `returns` with `adjustment_factor` subtracted from each
/// value in `column`.
///
/// Timestamps are preserved and the result is stored under `column`; NaN
/// values remain NaN. Throws `input_error` when `column` is absent, is not a
/// double column, or does not match the frame index length.
dataframe adjust_returns(const dataframe& returns,
                         double adjustment_factor,
                         std::string_view column = returns_column);

}  // namespace fincore
