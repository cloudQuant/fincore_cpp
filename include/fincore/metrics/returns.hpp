#pragma once

#include <fincore/calendar.hpp>
#include <fincore/dataframe.hpp>

#include <initializer_list>
#include <string_view>

namespace fincore {

/// Default name of the column holding periodic simple returns.
inline constexpr std::string_view returns_column{"returns"};
/// Default annualization factor for daily observations (252 periods per year).
inline constexpr double daily_annualization = 252.0;
/// Name of the integer calendar-year column added by aggregate_returns.
inline constexpr std::string_view return_aggregation_year_column{"year"};
/// Name of the integer period-within-year column added by aggregate_returns.
inline constexpr std::string_view return_aggregation_period_column{"period"};

/// Calendar bucket used by aggregate_returns.
enum class return_aggregation_period {
    weekly,    ///< ISO-8601 weeks, keyed by week number within the year.
    monthly,   ///< Calendar months.
    quarterly, ///< Calendar quarters.
    yearly,    ///< Calendar years.
};

/// Returns the period-over-period simple returns of `price_column`, dropping
/// the first observation.
///
/// The result is stored under `returns_column` and keeps the timestamp of the
/// later price of each pair. Fewer than two prices yield an empty frame.
dataframe simple_returns(const dataframe& prices, std::string_view price_column);

/// Returns the cumulative growth of `column` as `wealth - 1` at each
/// timestamp, with NaN returns treated as zero.
dataframe cumulative_returns(const dataframe& returns,
                             std::string_view column = returns_column);

/// Returns cumulative_returns for the legacy single-name column form.
///
/// The initializer list may hold at most one name; more than one throws
/// `input_error`.
dataframe cumulative_returns(
    const dataframe& returns,
    std::initializer_list<std::string_view> legacy_column);

/// Returns the cumulative growth of `column` scaled by `starting_value`.
///
/// Each output value is `starting_value * wealth` when `starting_value` is
/// nonzero and `wealth - 1` when it is zero. NaN returns are treated as zero.
dataframe cumulative_returns(const dataframe& returns,
                             double starting_value,
                             std::string_view column = returns_column);

/// Returns the total compounded return of `column`, treating NaN returns as
/// zero; an empty frame yields NaN.
double cumulative_return(const dataframe& returns,
                         std::string_view column = returns_column);

/// Returns cumulative_return for the legacy single-name column form.
///
/// The initializer list may hold at most one name; more than one throws
/// `input_error`.
double cumulative_return(
    const dataframe& returns,
    std::initializer_list<std::string_view> legacy_column);

/// Returns the final cumulative value of `column` scaled by `starting_value`.
///
/// The result is `starting_value * wealth` when `starting_value` is nonzero
/// and `wealth - 1` when it is zero. NaN returns are treated as zero.
double cumulative_return(const dataframe& returns,
                         double starting_value,
                         std::string_view column = returns_column);

/// Returns `values` rescaled so the first value of `column` equals
/// `starting_value`.
///
/// A zero first value makes every output NaN; other NaN values propagate. The
/// result keeps the input timestamps and column name.
dataframe normalize(const dataframe& values,
                    double starting_value = 1.0,
                    std::string_view column = returns_column);

/// Returns returns compounded into calendar buckets of `period`.
///
/// The output index holds each bucket's start timestamp (Monday for weeks,
/// clamped to January 1; the first day of the month or quarter; January 1 for
/// years), with integer `year` and `period` columns and the compounded return
/// under `column`. NaN returns are treated as zero. `column` must not be
/// `return_aggregation_year_column` or `return_aggregation_period_column`;
/// otherwise `input_error` is thrown.
dataframe aggregate_returns(
    const dataframe& returns,
    return_aggregation_period period = return_aggregation_period::monthly,
    std::string_view column = returns_column);

/// Returns the geometric annualized return of `column`.
///
/// Total wealth is raised to `annualization / observation_count`, with NaN
/// returns treated as zero. An empty frame yields NaN and non-positive ending
/// wealth yields -1.
double annual_return(const dataframe& returns,
                     double annualization = daily_annualization,
                     std::string_view column = returns_column);

/// Returns the annualized sample standard deviation of `column`.
///
/// Non-NaN values are used with one degree of freedom and the result is
/// scaled by `sqrt(annualization)`; fewer than two valid values yield NaN.
double annual_volatility(const dataframe& returns,
                         double annualization = daily_annualization,
                         std::string_view column = returns_column);

}  // namespace fincore
