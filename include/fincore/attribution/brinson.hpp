#pragma once

#include <fincore/dataframe.hpp>

#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fincore {

inline constexpr std::string_view brinson_allocation_column{"allocation"};
inline constexpr std::string_view brinson_selection_column{"selection"};
inline constexpr std::string_view brinson_interaction_column{"interaction"};
inline constexpr std::string_view brinson_total_column{"total"};
inline constexpr std::string_view brinson_portfolio_return_column{
    "portfolio_return"};
inline constexpr std::string_view brinson_benchmark_return_column{
    "benchmark_return"};

struct brinson_period_result {
    double allocation;
    double selection;
    double interaction;
    double total;
    double portfolio_return;
    double benchmark_return;
};

struct brinson_cumulative_result {
    double allocation;
    double selection;
    double interaction;
    double total;
    double portfolio_cumulative;
    double benchmark_cumulative;
};

brinson_period_result brinson_attribution(
    const dataframe& portfolio_returns,
    const dataframe& benchmark_returns,
    const dataframe& portfolio_weights,
    const dataframe& benchmark_weights,
    std::span<const std::string_view> sector_column_names);

dataframe brinson_results(
    const dataframe& portfolio_returns,
    const dataframe& benchmark_returns,
    const dataframe& portfolio_weights,
    const dataframe& benchmark_weights,
    std::span<const std::string_view> sector_column_names);

brinson_cumulative_result brinson_cumulative(
    const dataframe& portfolio_returns,
    const dataframe& benchmark_returns,
    const dataframe& portfolio_weights,
    const dataframe& benchmark_weights,
    std::span<const std::string_view> sector_column_names);

/// Native owning facade corresponding to the source's stateful
/// ``BrinsonAttribution`` calculator.
///
/// The facade stores an optional sector mapping and applies it to returns
/// (mean), benchmark returns (mean) and weights (sum) before delegating to the
/// canonical :func:`brinson_results` kernel. Missing benchmark returns default
/// to the portfolio returns; missing weights default to equal weights. The
/// native result keeps the kernel's timestamp index and full effect columns
/// (the source's integer ``period`` column and RangeIndex are Python identity,
/// not a native claim).
class BrinsonAttribution {
  public:
    explicit BrinsonAttribution(
        std::map<std::string, std::vector<std::string>> sector_mapping = {});

    [[nodiscard]] const std::map<std::string, std::vector<std::string>>&
    sector_mapping() const noexcept;

    /// ``method`` accepts ``brinson`` and the historical ``brinson_hood``
    /// alias, both selecting the BHB arithmetic decomposition.
    [[nodiscard]] dataframe calculate(
        const dataframe& returns,
        std::optional<dataframe> benchmark_returns = std::nullopt,
        std::optional<dataframe> weights = std::nullopt,
        std::string_view method = "brinson") const;

  private:
    std::map<std::string, std::vector<std::string>> sector_mapping_;
};

}  // namespace fincore
