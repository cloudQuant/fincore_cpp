#pragma once

#include <fincore/factor/data.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace fincore {

struct pit_point {
    timestamp as_of = 0;
    timestamp known_at = 0;
    timestamp effective_from = 0;
    double value = 0.0;
};

struct pit_observation {
    std::string asset;
    pit_point point;
    bool in_universe = true;
};

void validate_pit_alignment(const std::vector<pit_point>& points);

factor_series materialize_pit_factor(
    const std::vector<pit_observation>& observations,
    const std::vector<timestamp>& evaluation_dates);

prepared_factor_data prepare_pit_factor_data(
    const std::vector<pit_observation>& observations,
    const dataframe& prices, const std::vector<timestamp>& evaluation_dates,
    const prepare_factor_config& config = {});

}  // namespace fincore
