#pragma once

#include <fincore/metrics/returns.hpp>

#include <optional>
#include <string_view>

namespace fincore {

enum class fee_treatment { net, gross };

dataframe cashflow_adjusted_returns(
    const dataframe& valuations,
    const std::optional<dataframe>& cashflows = std::nullopt,
    const std::optional<dataframe>& fees = std::nullopt,
    fee_treatment treatment = fee_treatment::net,
    std::string_view value_column = "value",
    std::string_view amount_column = "amount",
    std::string_view timing_column = "timing",
    std::string_view fees_column = "fees");

double cashflow_adjusted_twr(
    const dataframe& valuations,
    const std::optional<dataframe>& cashflows = std::nullopt,
    const std::optional<dataframe>& fees = std::nullopt,
    fee_treatment treatment = fee_treatment::net,
    std::string_view value_column = "value",
    std::string_view amount_column = "amount",
    std::string_view timing_column = "timing",
    std::string_view fees_column = "fees");

}  // namespace fincore
