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

}  // namespace fincore
