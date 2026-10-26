#pragma once

#include <fincore/metrics/basic.hpp>
#include <fincore/metrics/alpha_beta.hpp>

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

namespace fincore {

/// Returns the rolling Sharpe ratio at each window end.
///
/// Every complete window of `window` observations produces one row, indexed by
/// its last timestamp; fewer than `window` observations produce an empty typed
/// frame. The factor is `annualization_factor(period, annualization)` and NaN
/// values are dropped by sharpe_ratio. A zero window, or an unknown
/// `frequency` without an explicit `annualization`, raises `input_error`.
dataframe roll_sharpe_ratio(const dataframe& returns,
                            std::size_t window = 252U,
                            double risk_free = 0.0,
                            frequency period = frequency::daily,
                            std::optional<double> annualization = std::nullopt,
                            std::string_view column = returns_column);

/// Returns the maximum drawdown at each window end.
///
/// Wealth restarts at 1 for every window and NaN observations never update the
/// running peak or the result, so a poisoned window reports 0.0 when no finite
/// drawdown is seen. Every complete window produces one row indexed by its last
/// timestamp; fewer than `window` observations produce an empty typed frame. A
/// zero window raises `input_error`.
dataframe roll_max_drawdown(const dataframe& returns,
                            std::size_t window = 252U,
                            std::string_view column = returns_column);

/// Returns the annualized rolling volatility aligned to the input index.
///
/// Rows before the first complete window are NaN, and a window containing NaN
/// yields NaN. Each window uses annual_volatility with
/// `annualization_factor(period, annualization)`. Fewer than `window`
/// observations produce all-NaN values. A zero window, or an unknown
/// `frequency` without an explicit `annualization`, raises `input_error`.
dataframe rolling_volatility(const dataframe& returns,
                             std::size_t window,
                             frequency period = frequency::daily,
                             std::optional<double> annualization = std::nullopt,
                             std::string_view column = returns_column);

/// Returns the rolling Sharpe ratio aligned to the input index.
///
/// The index and warm-up shape match rolling_volatility, the risk-free rate is
/// zero, and a window containing NaN yields NaN. The factor is
/// `annualization_factor(period, annualization)`. A zero window, or an unknown
/// `frequency` without an explicit `annualization`, raises `input_error`.
dataframe rolling_sharpe(const dataframe& returns,
                         std::size_t window,
                         frequency period = frequency::daily,
                         std::optional<double> annualization = std::nullopt,
                         std::string_view column = returns_column);

/// Returns the rolling annualized Jensen alpha at each window end.
///
/// Windows are built from the common timestamps of both frames and delegated
/// to alpha with `risk_free` and `annualization_factor(period, annualization)`.
/// Every complete window produces one row indexed by the aligned window end;
/// fewer than `window` aligned observations, including no common timestamps,
/// produce an empty typed frame. A zero window, an unknown `frequency` without
/// an explicit `annualization`, or an invalid column raises `input_error`.
dataframe roll_alpha(const dataframe& returns, const dataframe& benchmark,
                     std::size_t window = 252U, double risk_free = 0.0,
                     frequency period = frequency::daily,
                     std::optional<double> annualization = std::nullopt,
                     std::string_view returns_column_name = returns_column,
                     std::string_view benchmark_column_name = benchmark_column);

/// Returns the rolling OLS beta at each window end.
///
/// Beta is not annualized, so `period` and `annualization` are ignored. Windows
/// are built from the common timestamps and delegated to beta with
/// `risk_free`. Every complete window produces one row indexed by the aligned
/// window end; fewer than `window` aligned observations produce an empty typed
/// frame. A zero window or an invalid column raises `input_error`.
dataframe roll_beta(const dataframe& returns, const dataframe& benchmark,
                    std::size_t window = 252U, double risk_free = 0.0,
                    frequency period = frequency::daily,
                    std::optional<double> annualization = std::nullopt,
                    std::string_view returns_column_name = returns_column,
                    std::string_view benchmark_column_name = benchmark_column);

/// Returns the annualized rolling volatility at each window end.
///
/// Unlike rolling_volatility, rows exist only for complete windows and NaN
/// observations are dropped by annual_volatility inside each window, so a
/// poisoned window is still evaluated when at least two finite observations
/// remain. Fewer than `window` observations produce an empty typed
/// frame. A zero window, or an unknown `frequency` without an explicit
/// `annualization`, raises `input_error`.
dataframe roll_annual_volatility(
    const dataframe& returns,
    std::size_t window,
    frequency period = frequency::daily,
    std::optional<double> annualization = std::nullopt,
    std::string_view column = returns_column);

/// Returns the rolling annualized Sortino ratio at each window end.
///
/// Every complete window is delegated to sortino_ratio with `required_return`
/// and `annualization_factor(period, annualization)`; NaN values are dropped by
/// the scalar metric. Fewer than `window` observations produce an empty typed
/// frame. A zero window, or an unknown `frequency` without an explicit
/// `annualization`, raises `input_error`.
dataframe roll_sortino_ratio(
    const dataframe& returns,
    std::size_t window,
    double required_return = 0.0,
    frequency period = frequency::daily,
    std::optional<double> annualization = std::nullopt,
    std::string_view column = returns_column);

/// Returns the rolling alpha and beta in the columns "alpha" and "beta".
///
/// Windows are built from the common timestamps and delegated to alpha_beta
/// with `risk_free` and `annualization_factor(period, annualization)`; windows
/// that alpha_beta cannot fit keep NaN values in both columns. Every complete
/// window produces one row indexed by the aligned window end; fewer than
/// `window` aligned observations produce an empty two-column frame. A zero
/// window, or an unknown `frequency` without an explicit `annualization`,
/// raises `input_error`.
dataframe roll_alpha_beta(
    const dataframe& returns,
    const dataframe& benchmark,
    std::size_t window = 252U,
    double risk_free = 0.0,
    frequency period = frequency::daily,
    std::optional<double> annualization = std::nullopt,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);

/// Returns the rolling beta of `returns` against one factor column.
///
/// The result keeps the aligned common index; rows before the first complete
/// window are NaN, as is any window with a non-finite value or zero factor
/// variance. Beta is the covariance over factor variance, with no risk-free
/// adjustment. A zero `rolling_window` or an invalid column raises
/// `input_error`.
dataframe rolling_beta(
    const dataframe& returns,
    const dataframe& factor_returns,
    std::size_t rolling_window = 126U,
    std::string_view returns_column_name = returns_column,
    std::string_view factor_column_name = benchmark_column);

/// Returns one rolling beta column per factor, named after the factor columns.
///
/// Each factor is aligned against `returns` separately using the same warm-up
/// and invalid-window rules as the single-factor overload. Factor names must be
/// non-empty, unique and present as double columns; otherwise `input_error` is
/// raised, as it is for a zero `rolling_window`.
dataframe rolling_beta(
    const dataframe& returns,
    const dataframe& factor_returns,
    std::span<const std::string_view> factor_column_names,
    std::size_t rolling_window = 126U,
    std::string_view returns_column_name = returns_column);

/// Returns the rolling OLS alpha and beta in the columns "alpha" and "beta".
///
/// Windows are built from the common timestamps; beta is the covariance over
/// factor variance and alpha is the mean return minus beta times the mean
/// benchmark return, with no risk-free adjustment. Rows are indexed by the
/// aligned window end, and windows with a non-finite value, zero factor
/// variance or non-finite coefficients are omitted, so the index can be
/// sparse. A zero `rolling_window` or an invalid column raises `input_error`.
dataframe rolling_regression(
    const dataframe& returns,
    const dataframe& benchmark,
    std::size_t rolling_window = 126U,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);

/// Returns the rolling up-capture ratio at each window end.
///
/// Every complete window of the common timestamps is delegated to up_capture
/// with the
/// `annualization_factor(period)` factor, so `period` is the only annualization
/// control. Fewer than `window` aligned observations produce an empty typed
/// frame. A zero window or an unknown `frequency` raises `input_error`.
dataframe roll_up_capture(const dataframe& returns, const dataframe& benchmark,
                          std::size_t window = 252U,
                          frequency period = frequency::daily,
                          std::string_view returns_column_name = returns_column,
                          std::string_view benchmark_column_name = benchmark_column);

/// Returns the rolling down-capture ratio at each window end.
///
/// Every complete window of the common timestamps is delegated to down_capture
/// with the
/// `annualization_factor(period)` factor, so `period` is the only annualization
/// control. Fewer than `window` aligned observations produce an empty typed
/// frame. A zero window or an unknown `frequency` raises `input_error`.
dataframe roll_down_capture(const dataframe& returns, const dataframe& benchmark,
                            std::size_t window = 252U,
                            frequency period = frequency::daily,
                            std::string_view returns_column_name = returns_column,
                            std::string_view benchmark_column_name = benchmark_column);

/// Returns the rolling up-down capture ratio at each window end.
///
/// Every complete window of the common timestamps is delegated to up_down_capture
/// with the
/// `annualization_factor(period)` factor, so `period` is the only annualization
/// control. Fewer than `window` aligned observations produce an empty typed
/// frame. A zero window or an unknown `frequency` raises `input_error`.
dataframe roll_up_down_capture(
    const dataframe& returns, const dataframe& benchmark,
    std::size_t window = 252U, frequency period = frequency::daily,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);

}  // namespace fincore
