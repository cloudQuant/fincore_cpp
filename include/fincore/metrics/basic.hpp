#pragma once

#include <fincore/metrics/returns.hpp>

#include <optional>
#include <string_view>

namespace fincore {

enum class frequency {
    daily,
    weekly,
    monthly,
    quarterly,
    yearly,
};

double annualization_factor(frequency period,
                            std::optional<double> annualization = std::nullopt);

dataframe adjust_returns(const dataframe& returns,
                         double adjustment_factor,
                         std::string_view column = returns_column);

}  // namespace fincore
