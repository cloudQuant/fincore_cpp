#pragma once

#include <fincore/error.hpp>

#include <DataFrame/DataFrame.h>

#include <cstdint>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace fincore {

using timestamp = std::int64_t;
using dataframe = hmdf::StdDataFrame<timestamp>;

dataframe make_series(std::vector<timestamp> index,
                      std::string_view column_name,
                      std::vector<double> values);

std::span<const double> require_double_column(const dataframe& frame,
                                              std::string_view column_name);

std::pair<dataframe, dataframe> align_on_common_index(
    const dataframe& left,
    std::string_view left_column,
    const dataframe& right,
    std::string_view right_column);

}  // namespace fincore
