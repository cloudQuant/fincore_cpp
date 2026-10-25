#pragma once

#include <fincore/metrics/alpha_beta.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace fincore {

/// Returns the Treynor-Mazuy market-timing coefficient.
///
/// Fits `excess_return = a + b * excess_market + c * excess_market^2` by
/// ordinary least squares on the common timestamps and returns `c`. Only finite
/// pairs are used and `risk_free` is subtracted from both series. Returns NaN
/// with fewer than ten usable pairs, a non-finite `risk_free`, a singular
/// design, or a non-finite coefficient.
double treynor_mazuy_timing(
    const dataframe& returns,
    const dataframe& benchmark,
    double risk_free = 0.0,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);

/// Returns the Henriksson-Merton market-timing coefficient.
///
/// Fits `excess_return = a + b * excess_market + c * down`, where `down` is one
/// when the excess market return is negative and zero otherwise, and returns
/// `c`. Only finite pairs are used and `risk_free` is subtracted from both
/// series. Returns NaN with fewer than ten usable pairs, a non-finite
/// `risk_free`, a singular design, or a non-finite coefficient.
double henriksson_merton_timing(
    const dataframe& returns,
    const dataframe& benchmark,
    double risk_free = 0.0,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);

/// Returns the Treynor-Mazuy coefficient times the mean squared excess market
/// return.
///
/// Uses the same quadratic fit, finite-pair filtering and minimum of ten
/// observations as treynor_mazuy_timing. Returns NaN under the same conditions
/// or when the resulting product is not finite.
double market_timing_return(
    const dataframe& returns,
    const dataframe& benchmark,
    double risk_free = 0.0,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);

/// Returns the Cornell timing measure: the up-market slope minus the
/// down-market slope.
///
/// Fits `excess_return = a + b * up + c * down`, where `up` is
/// `max(0, excess_market)` and `down` is `min(0, excess_market)`, by ordinary
/// least squares and returns `b - c`. Only finite pairs are used and
/// `risk_free` is subtracted from both series. Returns NaN with fewer than ten
/// usable pairs, a non-finite `risk_free`, a singular design, or a non-finite
/// result.
double cornell_timing(
    const dataframe& returns,
    const dataframe& benchmark,
    double risk_free = 0.0,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);

/// One named historical window extracted from a return series.
struct named_returns {
    std::string name;  ///< Window label, for example "Lehman".
    dataframe returns; ///< Returns of the selected column inside the window.
};

/// Returns the pinned historical crisis windows that overlap `returns`.
///
/// Each window is inclusive of both UTC endpoints and contributes an entry only
/// when it holds at least one observation. Entries follow the pinned window
/// order, while observations keep their input order and the input column name;
/// each returned frame owns its data. An embedded NUL, empty or invalid
/// `column` raises `input_error`.
std::vector<named_returns> extract_interesting_date_ranges(
    const dataframe& returns,
    std::string_view column = returns_column);

}  // namespace fincore
