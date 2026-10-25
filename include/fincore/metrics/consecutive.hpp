#pragma once

#include <fincore/metrics/returns.hpp>

#include <optional>
#include <string_view>

namespace fincore {

/// Daily run statistics from a return series.
struct daily_consecutive_statistics {
    double max_consecutive_up_days;   ///< Longest run of positive observations.
    double max_consecutive_down_days; ///< Longest run of negative observations.
    double max_consecutive_gain;      ///< Largest sum over a positive run.
    double max_consecutive_loss;      ///< Most negative sum over a negative run.
};

/// Returns the daily run statistics of `column`.
///
/// NaN values break a run. An empty input yields all-NaN fields; the gain/loss
/// fields are also NaN when no run of that sign exists, while the run lengths
/// are 0.
daily_consecutive_statistics daily_consecutive_stats(
    const dataframe& returns,
    std::string_view column = returns_column);

/// Returns the longest run of strictly positive observations in `column`.
///
/// Returns 0 when there is none and NaN for empty input.
double max_consecutive_up_days(const dataframe& returns,
                               std::string_view column = returns_column);
/// Returns the longest run of strictly negative observations in `column`.
///
/// Returns 0 when there is none and NaN for empty input.
double max_consecutive_down_days(const dataframe& returns,
                                 std::string_view column = returns_column);
/// Returns the largest sum of returns over a consecutive positive run.
///
/// Returns NaN when no positive run exists.
double max_consecutive_gain(const dataframe& returns,
                            std::string_view column = returns_column);
/// Returns the most negative sum of returns over a consecutive negative run.
///
/// Returns NaN when no negative run exists.
double max_consecutive_loss(const dataframe& returns,
                            std::string_view column = returns_column);

/// Returns the longest run of consecutive calendar weeks (ending Sunday) whose
/// compounded return is positive.
///
/// Weeks without observations are skipped. Returns 0 when there is none and
/// NaN for empty input.
double max_consecutive_up_weeks(const dataframe& returns,
                                std::string_view column = returns_column);
/// Returns the longest run of consecutive calendar weeks (ending Sunday) whose
/// compounded return is negative.
///
/// Weeks without observations are skipped. Returns 0 when there is none and
/// NaN for empty input.
double max_consecutive_down_weeks(const dataframe& returns,
                                  std::string_view column = returns_column);
/// Returns the longest run of consecutive calendar months whose compounded
/// return is positive.
///
/// Months without observations are skipped. Returns 0 when there is none and
/// NaN for empty input.
double max_consecutive_up_months(const dataframe& returns,
                                 std::string_view column = returns_column);
/// Returns the longest run of consecutive calendar months whose compounded
/// return is negative.
///
/// Months without observations are skipped. Returns 0 when there is none and
/// NaN for empty input.
double max_consecutive_down_months(const dataframe& returns,
                                   std::string_view column = returns_column);
/// Returns the largest non-NaN observation in `column`; NaN for empty input.
double max_single_day_gain(const dataframe& returns,
                           std::string_view column = returns_column);
/// Returns the smallest non-NaN observation in `column`; NaN for empty input.
double max_single_day_loss(const dataframe& returns,
                           std::string_view column = returns_column);

/// Returns the timestamp of the largest non-NaN observation in `column`.
///
/// Ties keep the earliest occurrence; returns nullopt when no value exists.
std::optional<timestamp> max_single_day_gain_date(
    const dataframe& returns,
    std::string_view column = returns_column);
/// Returns the timestamp of the smallest non-NaN observation in `column`.
///
/// Ties keep the earliest occurrence; returns nullopt when no value exists.
std::optional<timestamp> max_single_day_loss_date(
    const dataframe& returns,
    std::string_view column = returns_column);
/// Returns the first timestamp of the longest positive run.
///
/// Ties keep the earliest run; returns nullopt when no positive run exists.
std::optional<timestamp> max_consecutive_up_start_date(
    const dataframe& returns,
    std::string_view column = returns_column);
/// Returns the last timestamp of the longest positive run.
///
/// Ties keep the earliest run; returns nullopt when no positive run exists.
std::optional<timestamp> max_consecutive_up_end_date(
    const dataframe& returns,
    std::string_view column = returns_column);
/// Returns the first timestamp of the longest negative run.
///
/// Ties keep the earliest run; returns nullopt when no negative run exists.
std::optional<timestamp> max_consecutive_down_start_date(
    const dataframe& returns,
    std::string_view column = returns_column);
/// Returns the last timestamp of the longest negative run.
///
/// Ties keep the earliest run; returns nullopt when no negative run exists.
std::optional<timestamp> max_consecutive_down_end_date(
    const dataframe& returns,
    std::string_view column = returns_column);

/// Returns a map of the six daily, weekly and monthly up/down run lengths,
/// keyed by their snake_case names.
std::map<std::string, double> consecutive_stats(
    const dataframe& returns,
    std::string_view column = returns_column);

}  // namespace fincore
