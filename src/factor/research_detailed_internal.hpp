#pragma once

#include <fincore/factor/research.hpp>

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
    const factor_series& panel, const dataframe& prices, const research_spec& spec);

}  // namespace fincore::research_detailed_detail
