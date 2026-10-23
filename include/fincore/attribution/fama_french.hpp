#pragma once

#include <fincore/dataframe.hpp>

#include <optional>
#include <string>
#include <vector>

namespace fincore {

struct idiosyncratic_risk_result {
    std::vector<std::string> assets;
    std::vector<double> specific_volatility;
    double market_beta = 0.0;
};

/// Asset-specific volatility using the source's first-asset market beta.
///
/// When ``market_beta`` is absent, the beta is fitted by OLS of the first
/// asset's excess returns on the five Fama-French columns (MKT, SMB, HML,
/// RMW, CMA) and then applied to every asset, matching the pinned routine.
idiosyncratic_risk_result calculate_idiosyncratic_risk(
    const dataframe& returns, const dataframe& factor_data,
    std::optional<double> market_beta = std::nullopt,
    double risk_free = 0.0);

}  // namespace fincore
