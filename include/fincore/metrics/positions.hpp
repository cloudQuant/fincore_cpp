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

}  // namespace fincore
