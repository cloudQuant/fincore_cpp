#pragma once

#include <fincore/dataframe.hpp>

#include <array>
#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fincore {

inline constexpr std::string_view style_asset_column{"asset"};
inline constexpr std::string_view style_name_column{"style"};
inline constexpr std::string_view style_return_column{"return"};
inline constexpr std::string_view style_attribution_contribution_column{
    "contribution"};
inline constexpr std::string_view style_attribution_residual_name{"residual"};

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

[[nodiscard]] dataframe calculate_style_tilts(
    const dataframe& asset_returns,
    std::size_t window = 252U);

[[nodiscard]] dataframe calculate_regression_attribution(
    const dataframe& asset_returns);

[[nodiscard]] dataframe calculate_regression_attribution(
    const dataframe& portfolio_returns,
    const dataframe& style_returns,
    std::span<const std::string_view> selected_styles);

/// Native owning facade for ``StyleResult``.
///
/// The source container exposes ``exposures``, ``returns_by_style`` and
/// ``overall_returns`` plus deterministic summary/dictionary projections. The
/// native class owns copies of all three frames.
class StyleResult {
  public:
    StyleResult(dataframe exposures, dataframe returns_by_style,
                dataframe overall_returns);

    explicit StyleResult(style_analysis_result result);

    [[nodiscard]] const dataframe& exposures() const noexcept;
    [[nodiscard]] const dataframe& returns_by_style() const noexcept;
    [[nodiscard]] const dataframe& overall_returns() const noexcept;

    /// ``style -> average return`` mapping in deterministic key order.
    [[nodiscard]] std::map<std::string, double> style_summary() const;

    /// Source keys ``exposures``, ``returns_by_style`` and
    /// ``overall_returns`` mapped to detached frames.
    [[nodiscard]] std::map<std::string, dataframe> to_dict() const;

  private:
    dataframe exposures_;
    dataframe returns_by_style_;
    dataframe overall_returns_;
};

/// Injected style-factor provider boundary.
///
/// The provider receives an explicit ``factors`` option (``nullopt`` mirrors
/// the source's ``None``) and returns a validated, detached frame.
using style_factor_provider = std::function<dataframe(
    const std::vector<std::string>& tickers,
    const std::optional<std::vector<std::string>>& factors,
    std::string_view library)>;

dataframe fetch_style_factors(
    const std::vector<std::string>& tickers,
    const std::optional<std::vector<std::string>>& factors,
    std::string_view library, const style_factor_provider& provider);

/// Convenience overload mirroring the keyword-only provider in the source.
dataframe fetch_style_factors(
    const std::vector<std::string>& tickers,
    const std::optional<std::vector<std::string>>& factors,
    const style_factor_provider& provider, std::string_view library = "us");

}  // namespace fincore
