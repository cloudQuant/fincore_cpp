#pragma once

#include <fincore/dataframe.hpp>
#include <fincore/report/models.hpp>

#include <cstddef>
#include <string>

namespace fincore {

report_document build_portfolio_report(
    const dataframe& returns, const dataframe* benchmark = nullptr,
    std::string title = "Portfolio Report",
    const dataframe* positions = nullptr,
    std::size_t rolling_window = 63U);

}  // namespace fincore
