#pragma once

#include <fincore/dataframe.hpp>

#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fincore {

/// Name of the Brinson allocation-effect column.
inline constexpr std::string_view brinson_allocation_column{"allocation"};
/// Name of the Brinson selection-effect column.
inline constexpr std::string_view brinson_selection_column{"selection"};
/// Name of the Brinson interaction-effect column.
inline constexpr std::string_view brinson_interaction_column{"interaction"};
/// Name of the Brinson total-effect column.
inline constexpr std::string_view brinson_total_column{"total"};
/// Name of the Brinson portfolio-return column.
inline constexpr std::string_view brinson_portfolio_return_column{
    "portfolio_return"};
/// Name of the Brinson benchmark-return column.
inline constexpr std::string_view brinson_benchmark_return_column{
    "benchmark_return"};

/// Brinson-Hood-Beebower effects for one period.
///
/// Each effect is the direct arithmetic contribution for that period; no
/// normalization or compounding is applied.
struct brinson_period_result {
    double allocation;       ///< Allocation effect for the period.
    double selection;        ///< Selection effect for the period.
    double interaction;      ///< Interaction effect for the period.
    double total;            ///< Sum of allocation, selection and interaction.
    double portfolio_return; ///< Weighted portfolio return for the period.
    double benchmark_return; ///< Weighted benchmark return for the period.
};

/// Summed Brinson effects and compounded returns across all periods.
struct brinson_cumulative_result {
    double allocation;            ///< Sum of the period allocation effects.
    double selection;             ///< Sum of the period selection effects.
    double interaction;           ///< Sum of the period interaction effects.
    double total;                 ///< Sum of the period total effects.
    double portfolio_cumulative;  ///< Compounded portfolio return, `wealth - 1`.
    double benchmark_cumulative;  ///< Compounded benchmark return, `wealth - 1`.
};

/// Computes single-period Brinson-Hood-Beebower attribution.
///
/// Every input frame must contain a double column for each sector in
/// `sector_column_names` and all four frames must share the same unique index.
/// The supplied weights are used as given, without normalization.
///
/// @param portfolio_returns Per-sector portfolio returns by period.
/// @param benchmark_returns Per-sector benchmark returns by period.
/// @param portfolio_weights Per-sector portfolio weights by period.
/// @param benchmark_weights Per-sector benchmark weights by period.
/// @param sector_column_names Non-empty distinct sector column names; each
///     name must identify a double column in every input frame.
/// @return The allocation, selection, interaction, total, portfolio and
///     benchmark effects for the single period.
/// @throws input_error If the sector names are empty or duplicated, an index
///     contains duplicates, the indexes do not match, a selected column is
///     missing or not a double column, or the result does not contain exactly
///     one row.
brinson_period_result brinson_attribution(
    const dataframe& portfolio_returns,
    const dataframe& benchmark_returns,
    const dataframe& portfolio_weights,
    const dataframe& benchmark_weights,
    std::span<const std::string_view> sector_column_names);

/// Computes one row of Brinson-Hood-Beebower effects per input period.
///
/// Each input frame must contain a double column for every sector in
/// `sector_column_names`, and all four frames must share the same unique
/// index. The result keeps that index and holds the `allocation`, `selection`,
/// `interaction`, `total`, `portfolio_return` and `benchmark_return` columns.
///
/// @param portfolio_returns Per-sector portfolio returns by period.
/// @param benchmark_returns Per-sector benchmark returns by period.
/// @param portfolio_weights Per-sector portfolio weights by period.
/// @param benchmark_weights Per-sector benchmark weights by period.
/// @param sector_column_names Non-empty distinct sector column names; each
///     name must identify a double column in every input frame.
/// @return A frame indexed like `portfolio_returns` with the six effect and
///     return columns.
/// @throws input_error If the sector names are empty or duplicated, an index
///     contains duplicates, the indexes do not match, or a selected column is
///     missing or not a double column.
dataframe brinson_results(
    const dataframe& portfolio_returns,
    const dataframe& benchmark_returns,
    const dataframe& portfolio_weights,
    const dataframe& benchmark_weights,
    std::span<const std::string_view> sector_column_names);

/// Computes summed effects and compounded returns across all periods.
///
/// The four effect fields are arithmetic sums of the per-period
/// `brinson_results` columns, while `portfolio_cumulative` and
/// `benchmark_cumulative` compound the per-period portfolio and benchmark
/// returns as `wealth - 1`.
///
/// @param portfolio_returns Per-sector portfolio returns by period.
/// @param benchmark_returns Per-sector benchmark returns by period.
/// @param portfolio_weights Per-sector portfolio weights by period.
/// @param benchmark_weights Per-sector benchmark weights by period.
/// @param sector_column_names Non-empty distinct sector column names; each
///     name must identify a double column in every input frame.
/// @return The summed effects and compounded portfolio and benchmark returns.
/// @throws input_error If the sector names are empty or duplicated, an index
///     contains duplicates, the indexes do not match, or a selected column is
///     missing or not a double column.
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
/// canonical ``brinson_results`` kernel. Missing benchmark returns default
/// to the portfolio returns; missing weights default to equal weights. The
/// native result keeps the kernel's timestamp index and full effect columns
/// (the source's integer ``period`` column and RangeIndex are Python identity,
/// not a native claim).
class BrinsonAttribution {
  public:
    /// Constructs the facade with an optional sector mapping.
    ///
    /// The mapping is stored as supplied and validated when `calculate` runs.
    ///
    /// @param sector_mapping Sector name to asset-column mapping; empty
    ///     disables sector aggregation.
    explicit BrinsonAttribution(
        std::map<std::string, std::vector<std::string>> sector_mapping = {});

    /// Returns the stored sector-to-asset mapping.
    ///
    /// @return Const reference to the mapping owned by this facade.
    [[nodiscard]] const std::map<std::string, std::vector<std::string>>&
    sector_mapping() const noexcept;

    /// Computes native Brinson attribution for the supplied returns.
    ///
    /// The ``method`` argument accepts ``brinson`` and the historical
    /// ``brinson_hood`` alias, both selecting the BHB arithmetic
    /// decomposition. When a sector mapping is present, returns and benchmark
    /// returns are averaged across each sector's assets and weights are summed
    /// before attribution. Missing benchmark returns default to the portfolio
    /// returns, missing weights default to equal weights, and benchmark
    /// weights are always equal weights.
    ///
    /// @param returns Portfolio returns with one double column per asset.
    /// @param benchmark_returns Optional benchmark returns; defaults to
    ///     ``returns``.
    /// @param weights Optional portfolio weights; defaults to equal weights.
    /// @param method Attribution method name.
    /// @return A frame indexed like ``returns`` with the allocation,
    ///     selection, interaction, total, portfolio-return and
    ///     benchmark-return columns.
    /// @throws fincore_error If ``method`` is unknown, a mapping entry names an
    ///     empty sector or asset list, a mapped asset column length does not
    ///     match the index, or no return column remains.
    /// @throws input_error If a mapped asset column is missing or the Brinson
    ///     kernel rejects the resolved inputs.
    [[nodiscard]] dataframe calculate(
        const dataframe& returns,
        std::optional<dataframe> benchmark_returns = std::nullopt,
        std::optional<dataframe> weights = std::nullopt,
        std::string_view method = "brinson") const;

  private:
    std::map<std::string, std::vector<std::string>> sector_mapping_;
};

}  // namespace fincore
