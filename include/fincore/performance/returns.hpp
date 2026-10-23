#pragma once

#include <fincore/metrics/returns.hpp>

#include <string_view>

namespace fincore {

double twr(const dataframe& returns,
           std::string_view column = returns_column);

}  // namespace fincore
