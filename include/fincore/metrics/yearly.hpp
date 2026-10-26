#pragma once

#include <fincore/metrics/alpha_beta.hpp>
#include <fincore/metrics/basic.hpp>

#include <optional>
#include <string_view>

namespace fincore {

/// Returns the annual return per UTC calendar year in column "annual_return".
///
/// Each year is indexed by its January 1 timestamp and uses annual_return with
/// `annualization_factor(period, annualization)`. Years are sorted ascending,
/// while observations keep their input order inside a year. An empty frame
/// yields an empty typed frame, and an unknown `frequency` without an explicit
/// `annualization` raises `input_error`.
dataframe annual_return_by_year(
    const dataframe& returns,
    frequency period = frequency::daily,
    std::optional<double> annualization = std::nullopt,
    std::string_view column = returns_column);

/// Returns the annualized volatility per UTC calendar year in column
/// "annual_volatility".
///
/// Each year uses annual_volatility, so NaN observations are ignored, with
/// `annualization_factor(period, annualization)`. Years are indexed by January
/// 1 and sorted ascending. An empty frame yields an empty typed frame, and an
/// unknown `frequency` without an explicit `annualization` raises
/// `input_error`.
dataframe annual_volatility_by_year(
    const dataframe& returns,
    frequency period = frequency::daily,
    std::optional<double> annualization = std::nullopt,
    std::string_view column = returns_column);

/// Returns the Sharpe ratio per UTC calendar year in column "sharpe_ratio".
///
/// Each year uses sharpe_ratio with `risk_free` and
/// `annualization_factor(period, annualization)`. Years are indexed by January
/// 1 and sorted ascending. An empty frame yields an empty typed frame, and an
/// unknown `frequency` without an explicit `annualization` raises
/// `input_error`.
dataframe sharpe_ratio_by_year(
    const dataframe& returns,
    double risk_free = 0.0,
    frequency period = frequency::daily,
    std::optional<double> annualization = std::nullopt,
    std::string_view column = returns_column);

/// Returns the maximum drawdown per UTC calendar year in column
/// "max_drawdown".
///
/// Each year uses max_drawdown, so a year that never declines reports 0. Years
/// are indexed by January 1 and sorted ascending. An empty frame yields an
/// empty typed frame.
dataframe max_drawdown_by_year(
    const dataframe& returns,
    std::string_view column = returns_column);

/// Returns the difference between the annual returns of `returns` and
/// `benchmark` on their common timestamps.
///
/// Both annual returns use `annualization_factor(period, annualization)`.
/// Returns NaN when the aligned pair is empty. An unknown `frequency` without
/// an explicit `annualization` raises `input_error`.
double annual_active_return(
    const dataframe& returns,
    const dataframe& benchmark,
    frequency period = frequency::daily,
    std::optional<double> annualization = std::nullopt,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);

/// Returns the active return per UTC calendar year in column
/// "annual_active_return".
///
/// The two frames are aligned on their common timestamps before bucketing, and
/// each year holds the difference of the two annual returns using
/// `annualization_factor(period, annualization)`. Years are indexed by January
/// 1 and sorted ascending; an empty aligned pair yields an empty typed frame.
/// An unknown `frequency` without an explicit `annualization` raises
/// `input_error`.
dataframe annual_active_return_by_year(
    const dataframe& returns,
    const dataframe& benchmark,
    frequency period = frequency::daily,
    std::optional<double> annualization = std::nullopt,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);

/// Returns the information ratio per UTC calendar year in column
/// "information_ratio".
///
/// The two frames are aligned on their common timestamps before bucketing, and
/// each year uses information_ratio with
/// `annualization_factor(period, annualization)`; non-finite pairs are dropped
/// within their own year. Years are indexed by January 1 and sorted ascending;
/// an empty aligned pair yields an empty typed frame. An unknown `frequency`
/// without an explicit `annualization` raises `input_error`.
dataframe information_ratio_by_year(
    const dataframe& returns,
    const dataframe& benchmark,
    frequency period = frequency::daily,
    std::optional<double> annualization = std::nullopt,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);

}  // namespace fincore
