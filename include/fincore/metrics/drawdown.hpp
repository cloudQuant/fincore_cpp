#pragma once

#include <fincore/metrics/returns.hpp>

#include <cstddef>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace fincore {

/// One underwater episode, from the first observation below the running peak
/// through its valley and optional recovery.
struct drawdown_period {
    timestamp start;   ///< First timestamp below the prior peak.
    timestamp valley;  ///< Timestamp of the deepest point.
    std::optional<timestamp> recovery; ///< Return to the prior peak; nullopt
                                       ///< when the episode never recovers.
    double value;      ///< Depth at the valley, a negative fraction of peak.
    std::size_t duration_periods; ///< Observations from start to valley.
    std::optional<std::size_t> recovery_duration_periods; ///< Observations from
                                                          ///< valley to recovery.
};

/// Peak, valley and optional recovery timestamps of a drawdown.
struct drawdown_window {
    timestamp peak;    ///< Timestamp of the running peak before the decline.
    timestamp valley;  ///< Timestamp of the deepest point.
    std::optional<timestamp> recovery; ///< Return to the peak; nullopt when
                                       ///< the episode never recovers.
};

/// One row of gen_drawdown_table.
struct drawdown_table_row {
    double net_drawdown_percent = std::numeric_limits<double>::quiet_NaN();
    ///< Depth as a positive percentage of peak wealth; NaN for an unused row.
    std::optional<timestamp> peak;     ///< Timestamp of the prior peak.
    std::optional<timestamp> valley;   ///< Timestamp of the deepest point.
    std::optional<timestamp> recovery; ///< Return to the peak, if any.
    std::optional<std::size_t> duration_business_days;
    ///< Monday-Friday days from peak through recovery, inclusive; nullopt
    ///< without a recovery.
};

/// Returns the underwater series of `column`: wealth relative to its running
/// peak, as a non-positive fraction.
///
/// NaN returns are treated as zero and the result is stored under
/// `returns_column`. An empty frame yields an empty frame.
dataframe drawdown_series(const dataframe& returns,
                          std::string_view column = returns_column);

/// Returns the deepest drawdown of `column` as a negative fraction.
///
/// Returns 0 when the series never declines and NaN for an empty frame.
double max_drawdown(const dataframe& returns,
                    std::string_view column = returns_column);

/// Returns the depth of every distinct underwater episode in chronological
/// order.
///
/// Values are negative fractions; an episode that has not recovered is
/// included with its current depth.
std::vector<double> get_all_drawdowns(
    const dataframe& returns,
    std::string_view column = returns_column);

/// Returns every distinct underwater episode with its start, valley, recovery
/// and durations, in chronological order.
std::vector<drawdown_period> get_all_drawdowns_detailed(
    const dataframe& returns,
    std::string_view column = returns_column);

/// Returns the peak, valley and optional recovery of the deepest episode.
///
/// Returns nullopt when the series never declines or the input is empty.
std::optional<drawdown_window> max_drawdown_window(
    const dataframe& returns,
    std::string_view column = returns_column);

/// Returns the peak/valley/recovery window from a precomputed underwater
/// series.
///
/// The source helper returns (NaT, NaT, NaT) when the series never goes below
/// zero; the native form returns nullopt for that case and for
/// empty/non-finite data.
std::optional<drawdown_window> max_drawdown_underwater(
    const dataframe& underwater,
    std::string_view column = "drawdown");

/// Returns up to `top` deepest non-overlapping drawdown windows, ordered by
/// depth.
///
/// Ties keep the earlier window. Returns an empty vector for empty input or
/// `top == 0`.
std::vector<drawdown_window> get_top_drawdowns(
    const dataframe& returns,
    std::size_t top = 10,
    std::string_view column = returns_column);

/// Returns `top` table rows for the deepest drawdown windows.
///
/// Ranking matches get_top_drawdowns. Unused rows keep a NaN
/// `net_drawdown_percent` and nullopt timestamps.
std::vector<drawdown_table_row> gen_drawdown_table(
    const dataframe& returns,
    std::size_t top = 10,
    std::string_view column = returns_column);

/// Returns the {peak, valley} timestamps of the maximum drawdown.
///
/// Returns {first timestamp, first timestamp} when the series never declines
/// and {nullopt, nullopt} for an empty frame.
std::pair<std::optional<timestamp>, std::optional<timestamp>>
max_drawdown_period(const dataframe& returns,
                    std::string_view column = returns_column);

/// Returns the elapsed UTC days from the peak to the valley of the deepest
/// drawdown.
///
/// Returns 0 when the series never declines and NaN for an empty frame. The
/// value is fractional when timestamps are not midnight-aligned.
double max_drawdown_days(const dataframe& returns,
                         std::string_view column = returns_column);
/// Returns max_drawdown_days divided by 7 (fractional weeks).
double max_drawdown_weeks(const dataframe& returns,
                          std::string_view column = returns_column);
/// Returns max_drawdown_days divided by 30 (approximate months).
double max_drawdown_months(const dataframe& returns,
                           std::string_view column = returns_column);
/// Returns the elapsed UTC days from the valley to the recovery of the deepest
/// drawdown.
///
/// Returns NaN when the episode has not recovered or the frame is empty, and 0
/// when the series never declines.
double max_drawdown_recovery_days(
    const dataframe& returns,
    std::string_view column = returns_column);
/// Returns max_drawdown_recovery_days divided by 7 (fractional weeks).
double max_drawdown_recovery_weeks(
    const dataframe& returns,
    std::string_view column = returns_column);
/// Returns max_drawdown_recovery_days divided by 30 (approximate months).
double max_drawdown_recovery_months(
    const dataframe& returns,
    std::string_view column = returns_column);
/// Returns the second-deepest drawdown as a negative fraction.
///
/// Returns NaN when fewer than two episodes exist.
double second_max_drawdown(const dataframe& returns,
                           std::string_view column = returns_column);
/// Returns the third-deepest drawdown as a negative fraction.
///
/// Returns NaN when fewer than three episodes exist.
double third_max_drawdown(const dataframe& returns,
                          std::string_view column = returns_column);
/// Returns the elapsed UTC days from the first underwater observation to the
/// valley of the second-deepest drawdown.
///
/// Returns NaN when fewer than two episodes exist.
double second_max_drawdown_days(
    const dataframe& returns,
    std::string_view column = returns_column);
/// Returns the elapsed UTC days from the valley to the recovery of the
/// second-deepest drawdown.
///
/// Returns NaN when that episode has not recovered or fewer than two episodes
/// exist.
double second_max_drawdown_recovery_days(
    const dataframe& returns,
    std::string_view column = returns_column);
/// Returns the elapsed UTC days from the first underwater observation to the
/// valley of the third-deepest drawdown.
///
/// Returns NaN when fewer than three episodes exist.
double third_max_drawdown_days(
    const dataframe& returns,
    std::string_view column = returns_column);
/// Returns the elapsed UTC days from the valley to the recovery of the
/// third-deepest drawdown.
///
/// Returns NaN when that episode has not recovered or fewer than three episodes
/// exist.
double third_max_drawdown_recovery_days(
    const dataframe& returns,
    std::string_view column = returns_column);

}  // namespace fincore
