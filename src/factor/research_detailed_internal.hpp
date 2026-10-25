#pragma once

#include <fincore/factor/research.hpp>

#include <cstddef>
#include <span>
#include <vector>

namespace fincore::research_detailed_detail {

struct horizon_input {
    std::string period;
    std::vector<double> label_values;
};

struct label_input {
    std::vector<std::string> periods;
    std::vector<horizon_input> horizons;
};

factor_series select_panel(const factor_series& factors, const research_spec& spec);

// Label vectors follow the selected panel row order, including NaN holes.
label_input build_detailed_label_input(
    const factor_series& panel, const dataframe& prices,
    const research_spec& spec);

// Internal-only stability path for the detailed evaluator. The panel must be
// sorted by (date, asset); ranks are parallel to its rows and retain NaNs for
// missing original values. date_offsets contains one contiguous row range per
// panel date, including dates whose ranks are all NaN.
std::vector<factor_series_result> factor_rank_autocorrelation_ranked_panel(
    const factor_series& panel, std::span<const double> ranks,
    std::span<const std::size_t> date_offsets, const std::vector<int>& lags);

}  // namespace fincore::research_detailed_detail
