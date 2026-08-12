#pragma once

#include <fincore/dataframe.hpp>

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

namespace fincore {

inline constexpr std::string_view style_asset_column{"asset"};
inline constexpr std::string_view style_name_column{"style"};
inline constexpr std::string_view style_return_column{"return"};

inline constexpr std::string_view style_equal_weight_column{"equal_weight"};
inline constexpr std::string_view style_large_column{"large"};
inline constexpr std::string_view style_mid_column{"mid"};
inline constexpr std::string_view style_small_column{"small"};
inline constexpr std::string_view style_winner_column{"winner"};
inline constexpr std::string_view style_loser_column{"loser"};
inline constexpr std::string_view style_value_column{"value"};
inline constexpr std::string_view style_growth_column{"growth"};
inline constexpr std::string_view style_high_column{"high"};
inline constexpr std::string_view style_low_column{"low"};

struct style_analysis_options {
    std::span<const double> market_caps{};
    std::span<const double> book_to_price{};
    std::span<const double> value_scores{};
    std::size_t momentum_window{252U};
    std::array<double, 2U> size_quantiles{.5, .5};
};

struct style_analysis_result {
    dataframe exposures;
    dataframe returns_by_style;
    dataframe overall_returns;
};

[[nodiscard]] style_analysis_result style_analysis(
    const dataframe& asset_returns,
    const style_analysis_options& options = {});

[[nodiscard]] dataframe analyze_performance_by_style(
    const dataframe& asset_returns);

[[nodiscard]] dataframe analyze_performance_by_style(
    const dataframe& asset_returns,
    const dataframe& style_exposures);

}  // namespace fincore
