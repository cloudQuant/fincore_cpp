#pragma once

#include <fincore/metrics/basic.hpp>

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

namespace fincore {

/// Default name of the column holding benchmark returns.
inline constexpr std::string_view benchmark_column{"benchmark"};

/// Returns the OLS beta of `returns` against `benchmark` on their common
/// timestamps.
///
/// NaN pairs are dropped and `risk_free` is subtracted from both series before
/// the regression. Returns NaN with fewer than two valid pairs, near-zero
/// benchmark variance, or a non-finite covariance/beta result.
double beta(const dataframe& returns,
            const dataframe& benchmark,
            double risk_free = 0.0,
            std::string_view returns_column_name = returns_column,
            std::string_view benchmark_column_name = benchmark_column);

/// Returns the fraction of peer strategy alphas strictly below the strategy's
/// alpha.
///
/// Peer series with fewer than three observations or a NaN alpha are ignored.
/// Returns NaN when the strategy alpha is unavailable or no peer alpha is
/// usable. Embedded NUL characters in the column names raise `input_error`.
double alpha_percentile_rank(
    const dataframe& strategy_returns,
    std::span<const dataframe> peer_returns,
    const dataframe& benchmark,
    double risk_free = 0.0,
    double annualization = daily_annualization,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);

/// Returns the annualized Jensen alpha on the common timestamps.
///
/// NaN pairs are dropped. The mean per-period excess return less beta times the
/// excess benchmark return is compounded at `annualization`. Returns NaN when
/// beta is not finite. `risk_free` is subtracted from both series.
double alpha(const dataframe& returns,
             const dataframe& benchmark,
             double risk_free = 0.0,
             double annualization = daily_annualization,
             std::string_view returns_column_name = returns_column,
             std::string_view benchmark_column_name = benchmark_column);

/// Returns annual alpha per UTC calendar year, indexed by January 1 and stored
/// in column "alpha".
///
/// Each year uses `annualization_factor(period, annualization)` and the same
/// NaN-pair rules as alpha.
dataframe annual_alpha(
    const dataframe& returns,
    const dataframe& benchmark,
    double risk_free = 0.0,
    frequency period = frequency::daily,
    std::optional<double> annualization = std::nullopt,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);

/// Returns annual beta per UTC calendar year, indexed by January 1 and stored
/// in column "beta".
///
/// `period` and `annualization` are ignored because beta is not annualized.
dataframe annual_beta(
    const dataframe& returns,
    const dataframe& benchmark,
    double risk_free = 0.0,
    frequency period = frequency::daily,
    std::optional<double> annualization = std::nullopt,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);

/// Returns {alpha, beta} computed on the common timestamps.
///
/// Only finite pairs are used. Returns {NaN, NaN} when fewer than two pairs
/// remain, `risk_free` or `annualization` is not finite, `annualization` is not
/// positive, or either result is not finite.
std::pair<double, double> alpha_beta(
    const dataframe& returns,
    const dataframe& benchmark,
    double risk_free = 0.0,
    double annualization = daily_annualization,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);

/// Returns {alpha, beta} using only dates with strictly positive benchmark
/// returns.
///
/// Otherwise follows alpha_beta.
std::pair<double, double> up_alpha_beta(
    const dataframe& returns,
    const dataframe& benchmark,
    double risk_free = 0.0,
    double annualization = daily_annualization,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);

/// Returns {alpha, beta} using only dates with non-positive benchmark returns
/// (zero is included).
///
/// Otherwise follows alpha_beta.
std::pair<double, double> down_alpha_beta(
    const dataframe& returns,
    const dataframe& benchmark,
    double risk_free = 0.0,
    double annualization = daily_annualization,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);

}  // namespace fincore
