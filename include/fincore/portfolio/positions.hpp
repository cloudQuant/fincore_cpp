#pragma once

#include <fincore/dataframe.hpp>

#include <string_view>

namespace fincore {

/// Default name of the cash column in wide position frames.
inline constexpr std::string_view cash_column{"cash"};

/// Returns each numeric position column as a fraction of its row total.
///
/// The output has the same index and one double column per input double
/// column, in input order. NaN cells are excluded from the row total and
/// remain NaN in the output; a zero row total yields NaN. Values are
/// fractions, not percentages.
///
/// @param positions Wide position frame with at least one double column whose
///     values match the frame index length.
/// @return Frame of row-wise position fractions.
/// @throws input_error If no double column exists or a double column length
///     differs from the index length.
dataframe get_percent_alloc(const dataframe& positions);

/// Returns gross long and short position values per timestamp.
///
/// Positive non-cash position values are summed into ``long`` and negative
/// values are summed into ``short`` as positive magnitudes. NaN cells are
/// skipped. The named cash column is excluded when present but is not
/// required.
///
/// @param positions Wide position frame.
/// @param cash_column_name Name of the cash column to exclude.
/// @return Frame with ``long`` and ``short`` double columns aligned to the
///     positions index.
/// @throws input_error If ``cash_column_name`` is empty, no double column
///     exists, or a double column length differs from the index length.
dataframe get_long_short_pos(
    const dataframe& positions,
    std::string_view cash_column_name = cash_column);

/// Returns row-wise long and short position concentration measures.
///
/// Allocation fractions are computed as in get_percent_alloc() over all double
/// columns. For each row, ``max_long`` is the largest positive non-cash
/// allocation, ``median_long`` the median positive allocation,
/// ``median_short`` the median negative allocation and ``max_short`` the most
/// negative allocation. A side without allocations yields NaN.
///
/// @param positions Wide position frame with a required double cash column.
/// @param cash_column_name Name of the cash column excluded from the measures.
/// @return Frame with ``max_long``, ``median_long``, ``median_short`` and
///     ``max_short`` columns.
/// @throws input_error If the cash name is empty, the cash column is absent or
///     not double, no double column exists, or a double column length differs
///     from the index length.
dataframe get_max_median_position_concentration(
    const dataframe& positions,
    std::string_view cash_column_name = cash_column);

/// Returns gross leverage per timestamp.
///
/// Gross leverage is the sum of absolute non-cash position values divided by
/// the row total of all double columns, including cash. NaN cells are skipped;
/// a zero denominator or a non-finite ratio yields NaN.
///
/// @param positions Wide position frame with a required double cash column.
/// @param cash_column_name Name of the cash column excluded from the numerator.
/// @return Frame with a ``gross_lev`` double column.
/// @throws input_error If the cash name is empty, the cash column is absent or
///     not double, no double column exists, or a double column length differs
///     from the index length.
dataframe gross_lev(
    const dataframe& positions,
    std::string_view cash_column_name = cash_column);

/// Symbol and value pair ranked by get_top_long_short_abs().
struct ranked_position {
    std::string symbol;  ///< Position column name.
    double value;        ///< Extremum over the column's rows.
};

/// Ranked long, short and absolute position lists.
struct top_positions {
    /// Largest positive column extrema, in descending order.
    std::vector<ranked_position> long_positions;
    /// Most negative column extrema, in ascending order.
    std::vector<ranked_position> short_positions;
    /// Largest absolute column extrema, in descending order.
    std::vector<ranked_position> absolute_positions;
};

/// One stacked position observation.
struct position_record {
    timestamp date;      ///< Position timestamp.
    std::string symbol;  ///< Position column name.
    double value;        ///< Position value; may be NaN.
};

/// Returns the top long, short and absolute positions by symbol.
///
/// Each non-cash double column contributes its maximum value to the long list
/// when positive, its minimum value to the short list when negative, and its
/// maximum absolute value to the absolute list. Lists are sorted by value
/// (longs and absolutes descending, shorts ascending) and truncated to ``top``
/// entries. NaN rows are ignored; symbols without a qualifying value are
/// omitted.
///
/// @param positions Wide position frame.
/// @param top Maximum number of entries in each returned list.
/// @param cash_column_name Name of the cash column to exclude.
/// @return Ranked long, short and absolute position lists.
/// @throws input_error If ``cash_column_name`` is empty or contains an
///     embedded NUL, no double column exists, or a double column length
///     differs from the index length.
top_positions get_top_long_short_abs(
    const dataframe& positions,
    std::size_t top = 10U,
    std::string_view cash_column_name = cash_column);

/// Returns per-timestamp long and short notional values.
///
/// Each row sums positive non-cash position values into the first vector and
/// absolute negative values into the second. NaN cells are skipped. The named
/// cash column is excluded when present but is not required.
///
/// @param positions Wide position frame.
/// @param cash_column_name Name of the cash column to exclude.
/// @return Pair of long and short vectors, each aligned to the positions
///     index.
/// @throws input_error If ``cash_column_name`` is empty or contains an
///     embedded NUL, no double column exists, or a double column length
///     differs from the index length.
std::pair<std::vector<double>, std::vector<double>> get_long_short_notional(
    const dataframe& positions,
    std::string_view cash_column_name = cash_column);

/// Returns positions in long form, one record per row and non-cash column.
///
/// Records follow input row order and then input column order. NaN values are
/// retained, and the named cash column is excluded when present. The
/// ``pos_in_dollars`` flag is accepted for API compatibility but does not
/// transform values in this implementation.
///
/// @param positions Wide position frame.
/// @param pos_in_dollars Retained for API compatibility; values pass through
///     unchanged.
/// @param cash_column_name Name of the cash column to exclude.
/// @return Stacked position records.
/// @throws input_error If ``cash_column_name`` is empty or contains an
///     embedded NUL, no double column exists, or a double column length
///     differs from the index length.
std::vector<position_record> stack_positions(
    const dataframe& positions,
    bool pos_in_dollars = true,
    std::string_view cash_column_name = cash_column);

/// Symbol-to-sector assignment used by get_sector_exposures().
struct sector_assignment {
    std::string symbol;  ///< Position column name.
    std::string sector;  ///< Output sector column name.
};

/// Numeric identifier and output label for an exposure bucket.
struct sector_definition {
    /// Numeric bucket identifier; sector exposures compare it with panel
    /// cells.
    double id;
    std::string name;    ///< Output column label.
};

/// Long, short, gross and net exposure frames sharing one index.
struct exposure_bundle {
    /// Long exposure values; units follow the producing API.
    dataframe long_exposure;
    /// Short exposure values; sign and units follow the producing API.
    dataframe short_exposure;
    /// Gross exposure values; units follow the producing API.
    dataframe gross_exposure;
    /// Net exposure (long minus short); empty for APIs that do not populate it.
    dataframe net_exposure;
};

/// Returns raw sector exposures by summing assigned positions.
///
/// One column is produced per sector named in ``assignments``, in
/// lexicographic sector order, followed by the cash column copied from
/// ``positions``. Only assignments whose symbol names an existing non-cash
/// double column contribute; unassigned assets are omitted. Values are summed
/// raw positions, not fractions.
///
/// @param positions Wide position frame with a required double cash column.
/// @param assignments Symbol-to-sector assignments.
/// @param cash_column_name Name of the cash column copied to the output.
/// @return Frame indexed like ``positions`` with one column per sector plus
///     cash.
/// @throws input_error If ``cash_column_name`` is empty, no double position
///     column exists, or a double column length differs from the index length.
dataframe get_sector_exposures(
    const dataframe& positions,
    std::span<const sector_assignment> assignments,
    std::string_view cash_column_name = cash_column);

/// Returns the weighted style-factor exposure per timestamp.
///
/// For each row the exposure is ``sum(position * factor) / sum(|position|)``
/// over non-cash asset columns. NaN positions are skipped entirely; a NaN
/// factor contributes zero to the numerator while its position still counts in
/// the denominator. A zero denominator or non-finite result yields 0.0.
///
/// @param positions Wide position frame.
/// @param risk_factors Wide factor panel with the same index as ``positions``
///     and a double column for every non-cash position asset.
/// @param cash_column_name Name of the cash column excluded when present.
/// @return Frame with an ``exposure`` double column aligned to the positions
///     index.
/// @throws input_error If the frames do not share an identical index,
///     ``cash_column_name`` is empty, no double position column exists, or a
///     position column length differs from the index length.
dataframe compute_style_factor_exposures(
    const dataframe& positions,
    const dataframe& risk_factors,
    std::string_view cash_column_name = cash_column);

/// Returns sector long, short, gross and net exposure fractions.
///
/// Assets are matched to definitions by comparing each ``sectors`` cell with
/// sector_definition::id. For each row the bundle reports the sector's share
/// of total long, total short (as a negative fraction) and total gross
/// non-cash positions; net exposure is long minus short. A sector without
/// matched positions contributes zero.
///
/// @param positions Wide position frame.
/// @param sectors Wide sector-id panel with the same index as ``positions``
///     and a double column for every non-cash asset.
/// @param definitions Sector identifiers and output column names.
/// @param cash_column_name Name of the cash column excluded when present.
/// @return Bundle of long, short, gross and net exposure frames with one
///     column per definition.
/// @throws input_error If the frames do not share an identical index,
///     ``cash_column_name`` is empty, no double position column exists, or a
///     position column length differs from the index length.
exposure_bundle compute_sector_exposures(
    const dataframe& positions,
    const dataframe& sectors,
    std::span<const sector_definition> definitions,
    std::string_view cash_column_name = cash_column);

/// Returns market-cap bucket exposure fractions.
///
/// Buckets use inclusive bounds in the units of the ``caps`` panel: ``Micro``
/// covers 50M-300M, ``Small`` 300M-2B, ``Mid`` 2B-10B, ``Large`` 10B-200B and
/// ``Mega`` 200B and above. Values below 50M match no bucket, and a value on a
/// shared boundary can match adjacent buckets. Exposure fractions are computed
/// as in compute_sector_exposures().
///
/// @param positions Wide position frame.
/// @param caps Wide market-cap panel with the same index as ``positions`` and
///     a double column for every non-cash asset.
/// @param cash_column_name Name of the cash column excluded when present.
/// @return Bundle of long, short, gross and net exposure frames with
///     ``Micro``, ``Small``, ``Mid``, ``Large`` and ``Mega`` columns.
/// @throws input_error If the frames do not share an identical index,
///     ``cash_column_name`` is empty, no double position column exists, or a
///     position column length differs from the index length.
exposure_bundle compute_cap_exposures(
    const dataframe& positions,
    const dataframe& caps,
    std::string_view cash_column_name = cash_column);

/// Returns position-to-volume exposure percentages.
///
/// For each row and common share/volume column, ``shares / volume`` is
/// computed. Positive shares feed the long set, negative shares feed the short
/// set as positive magnitudes, and absolute fractions feed the gross set. The
/// reported value is 100 times the linearly interpolated quantile at
/// ``percentile`` of each set, or 0.0 when the set is empty. Only common
/// columns contribute to the quantiles; a nonzero share value in any share
/// column requires a finite, positive matching volume.
///
/// @param shares_held Wide share-count frame.
/// @param volumes Wide share-volume frame with the same index as
///     ``shares_held``.
/// @param percentile Quantile in [0, 1] applied within each row.
/// @return Bundle whose ``long_exposure``, ``short_exposure`` and
///     ``gross_exposure`` frames hold an ``exposure`` column;
///     ``net_exposure`` is empty.
/// @throws input_error If the frames do not share an identical index,
///     ``percentile`` is outside [0, 1], or a nonzero share value has no
///     finite positive matching volume.
exposure_bundle compute_volume_exposures(
    const dataframe& shares_held,
    const dataframe& volumes,
    double percentile = 0.5);

}  // namespace fincore
