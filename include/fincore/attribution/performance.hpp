#pragma once

#include <fincore/dataframe.hpp>

#include <string>
#include <vector>

namespace fincore {

struct factor_loading_table {
    std::vector<timestamp> dates;
    std::vector<std::string> assets;
    std::vector<std::string> factors;
    std::vector<double> values;  // dates x assets x factors, NaN missing
};

struct attribution_alignment {
    dataframe returns;
    dataframe positions;
    dataframe factor_returns;
    factor_loading_table factor_loadings;
    std::vector<std::string> warnings;
};

/// Align attribution inputs to the positions' assets and loading dates.
///
/// Position assets without loadings are dropped (except ``cash``) and dates
/// without loadings are removed from returns, positions and factor returns.
/// Warnings are returned instead of emitted. Transaction turnover checks and
/// stacked Series positions are not represented by this typed surface.
attribution_alignment align_and_warn(
    const dataframe& returns, const dataframe& positions,
    const dataframe& factor_returns,
    const factor_loading_table& factor_loadings);

}  // namespace fincore
