#pragma once

#include <fincore/dataframe.hpp>
#include <fincore/report/models.hpp>

#include <cstddef>
#include <span>
#include <string>

namespace fincore {

struct portfolio_transaction {
    timestamp date = 0;
    std::string symbol;
    double amount = 0.0;
    double price = 0.0;
};

report_document build_portfolio_report(
    const dataframe& returns, const dataframe* benchmark = nullptr,
    std::string title = "Portfolio Report",
    const dataframe* positions = nullptr,
    std::size_t rolling_window = 63U,
    std::span<const portfolio_transaction> transactions = {});

}  // namespace fincore
