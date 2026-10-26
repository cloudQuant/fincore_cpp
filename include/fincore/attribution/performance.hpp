#pragma once

#include <fincore/dataframe.hpp>

#include <string>
#include <vector>

namespace fincore {

/// Factor-loading table aligned by date, asset and factor labels.
///
/// ``values`` is a flattened ``dates x assets x factors`` buffer; NaN marks a
/// missing loading. ``align_and_warn`` requires its length to match the label
/// dimensions and uses the date and asset labels for alignment.
struct factor_loading_table {
    std::vector<timestamp> dates;      ///< Dates with available loadings.
    std::vector<std::string> assets;   ///< Asset identifiers with loadings.
    std::vector<std::string> factors;  ///< Factor identifiers.
    std::vector<double> values;        ///< Flat loading values, NaN if missing.
};

/// Attribution inputs after asset and date alignment.
struct attribution_alignment {
    dataframe returns;          ///< Returns restricted to aligned dates.
    /// Positions with unloaded non-cash assets and unaligned dates removed.
    dataframe positions;
    dataframe factor_returns;   ///< Factor returns restricted to aligned dates.
    /// Loadings copied unchanged from the input table.
    factor_loading_table factor_loadings;
    /// Alignment warnings, with the missing-asset warning first.
    std::vector<std::string> warnings;
};

/// Aligns attribution inputs to the positions' assets and loading dates.
///
/// Position assets without loadings are dropped (except ``cash``) and dates
/// without loadings are removed from returns, positions and factor returns.
/// Warnings are returned in ``warnings`` instead of being emitted. Transaction
/// turnover checks and stacked Series positions are not represented by this
/// typed surface.
///
/// @param returns Portfolio returns to restrict to aligned dates.
/// @param positions Position frame whose double columns define the assets.
/// @param factor_returns Factor returns to restrict to aligned dates.
/// @param factor_loadings Loading labels and flattened values; its value count
///     must equal ``dates x assets x factors``.
/// @return The aligned frames, the copied loading table and any warnings.
/// @throws input_error If an input index contains adjacent duplicate
///     timestamps, positions are empty, the loading buffer size is
///     inconsistent, or no non-cash position has loadings.
attribution_alignment align_and_warn(
    const dataframe& returns, const dataframe& positions,
    const dataframe& factor_returns,
    const factor_loading_table& factor_loadings);

}  // namespace fincore
