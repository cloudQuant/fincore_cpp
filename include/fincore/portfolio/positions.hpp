#pragma once

#include <fincore/dataframe.hpp>

#include <string_view>

namespace fincore {

inline constexpr std::string_view cash_column{"cash"};

dataframe get_percent_alloc(const dataframe& positions);

dataframe get_long_short_pos(
    const dataframe& positions,
    std::string_view cash_column_name = cash_column);

dataframe get_max_median_position_concentration(
    const dataframe& positions,
    std::string_view cash_column_name = cash_column);

dataframe gross_lev(
    const dataframe& positions,
    std::string_view cash_column_name = cash_column);

struct ranked_position {
    std::string symbol;
    double value;
};

struct top_positions {
    std::vector<ranked_position> long_positions;
    std::vector<ranked_position> short_positions;
    std::vector<ranked_position> absolute_positions;
};

struct position_record {
    timestamp date;
    std::string symbol;
    double value;
};

top_positions get_top_long_short_abs(
    const dataframe& positions,
    std::size_t top = 10U,
    std::string_view cash_column_name = cash_column);

std::pair<std::vector<double>, std::vector<double>> get_long_short_notional(
    const dataframe& positions,
    std::string_view cash_column_name = cash_column);

std::vector<position_record> stack_positions(
    const dataframe& positions,
    bool pos_in_dollars = true,
    std::string_view cash_column_name = cash_column);

struct sector_assignment {
    std::string symbol;
    std::string sector;
};

struct sector_definition {
    double id;
    std::string name;
};

struct exposure_bundle {
    dataframe long_exposure;
    dataframe short_exposure;
    dataframe gross_exposure;
    dataframe net_exposure;
};

dataframe get_sector_exposures(
    const dataframe& positions,
    std::span<const sector_assignment> assignments,
    std::string_view cash_column_name = cash_column);

dataframe compute_style_factor_exposures(
    const dataframe& positions,
    const dataframe& risk_factors,
    std::string_view cash_column_name = cash_column);

exposure_bundle compute_sector_exposures(
    const dataframe& positions,
    const dataframe& sectors,
    std::span<const sector_definition> definitions,
    std::string_view cash_column_name = cash_column);

exposure_bundle compute_cap_exposures(
    const dataframe& positions,
    const dataframe& caps,
    std::string_view cash_column_name = cash_column);

exposure_bundle compute_volume_exposures(
    const dataframe& shares_held,
    const dataframe& volumes,
    double percentile = 0.5);

}  // namespace fincore
