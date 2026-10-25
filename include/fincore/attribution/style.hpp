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

/// Name of the string column holding asset identifiers in exposures.
inline constexpr std::string_view style_asset_column{"asset"};
/// Name of the string column holding style names.
inline constexpr std::string_view style_name_column{"style"};
/// Name of the double column holding style or portfolio returns.
inline constexpr std::string_view style_return_column{"return"};
/// Name of the double column holding style attribution contributions.
inline constexpr std::string_view style_attribution_contribution_column{
    "contribution"};
/// Name of the residual row in style attribution results.
inline constexpr std::string_view style_attribution_residual_name{"residual"};

/// Name of the equal-weight style column.
inline constexpr std::string_view style_equal_weight_column{"equal_weight"};
/// Name of the large-cap size style column.
inline constexpr std::string_view style_large_column{"large"};
/// Name of the mid-cap size style column.
inline constexpr std::string_view style_mid_column{"mid"};
/// Name of the small-cap size style column.
inline constexpr std::string_view style_small_column{"small"};
/// Name of the momentum-winner style column.
inline constexpr std::string_view style_winner_column{"winner"};
/// Name of the momentum-loser style column.
inline constexpr std::string_view style_loser_column{"loser"};
/// Name of the value style column.
inline constexpr std::string_view style_value_column{"value"};
/// Name of the growth style column.
inline constexpr std::string_view style_growth_column{"growth"};
/// Name of the high-volatility style column.
inline constexpr std::string_view style_high_column{"high"};
/// Name of the low-volatility style column.
inline constexpr std::string_view style_low_column{"low"};

/// Inputs controlling style exposure construction.
///
/// Optional per-asset vectors must either be empty or contain one finite value
/// per asset column. When ``market_caps`` is empty the size styles are
/// replaced by ``equal_weight``. ``value_scores`` takes precedence over
/// ``book_to_price`` when both are supplied.
struct style_analysis_options {
    std::span<const double> market_caps{};   ///< Per-asset market caps.
    std::span<const double> book_to_price{}; ///< Per-asset book-to-price values.
    std::span<const double> value_scores{};  ///< Per-asset value scores.
    std::size_t momentum_window{252U};       ///< Trailing momentum observations.
    /// Lower and upper size cutoffs in [0, 1].
    std::array<double, 2U> size_quantiles{.5, .5};
};

/// Frames produced by ``style_analysis``.
struct style_analysis_result {
    dataframe exposures;         ///< Asset rows with one column per style.
    dataframe returns_by_style;  ///< Style names and average style returns.
    dataframe overall_returns;   ///< Equal-weighted returns on the input index.
};

/// Computes style exposures and style return summaries.
///
/// Every asset column must be double, have a distinct non-empty name and
/// contain finite values. The exposure table has one row per asset and one
/// column per constructed style; ``returns_by_style`` reports each style's
/// arithmetic mean return; ``overall_returns`` reports the equal-weighted
/// portfolio return on the input timestamps. A style with no eligible assets
/// receives zero weights and a NaN summary return.
///
/// @param asset_returns Per-asset simple returns; every column is an asset.
/// @param options Style construction options; per-asset vectors must be empty
///     or match the asset count.
/// @return Exposure, style-return summary and overall-return frames, each
///     owning its data.
/// @throws input_error If the frame is empty, contains non-double columns,
///     empty or duplicate asset names, non-finite returns or option values,
///     size quantiles outside [0, 1], or unrepresentable sizes.
[[nodiscard]] style_analysis_result style_analysis(
    const dataframe& asset_returns,
    const style_analysis_options& options = {});

/// Computes per-style return series using default style analysis.
///
/// Runs ``style_analysis`` with default options and passes its exposures to
/// the two-frame overload. Empty-shaped input yields an empty frame.
///
/// @param asset_returns Per-asset simple returns.
/// @return One ``<style>_return`` column per style on an ordinal index.
/// @throws input_error If style analysis or performance alignment rejects the
///     input.
[[nodiscard]] dataframe analyze_performance_by_style(
    const dataframe& asset_returns);

/// Computes per-style return series from explicit style exposures.
///
/// The exposure frame must have an ordinal index, exactly one string asset
/// column named ``asset`` and at least one double style column. Rows are
/// aligned to return columns by asset identity; missing assets contribute zero
/// and extra exposure assets are ignored. A style with a positive aligned
/// weight total is normalized to sum to one; a style with a non-positive total
/// produces an all-zero return series. Empty-shaped inputs, or an exposure
/// frame with no double style columns, yield an empty frame.
///
/// @param asset_returns Per-asset simple returns.
/// @param style_exposures Asset-by-style exposure table.
/// @return A frame with one ``<style>_return`` column per exposure style on an
///     ordinal index.
/// @throws input_error If either frame is malformed, asset names are invalid
///     or duplicated, values are non-finite, or arithmetic overflows.
[[nodiscard]] dataframe analyze_performance_by_style(
    const dataframe& asset_returns,
    const dataframe& style_exposures);

/// Computes rolling cross-sectional style tilt indicators.
///
/// Each output row uses the trailing ``window`` observations ending before
/// that row. Size splits the average rank of the latest return at the
/// one-third and two-thirds cutoffs (lower third ``_large``, upper third
/// ``_small``); winner/loser splits the compounded trailing return at zero;
/// value/growth splits its average rank at the median; high/low volatility
/// splits the sample deviation of at most the last 60 observations at the
/// median, with ties assigned to high volatility. Output columns are
/// phase-major: all ``<asset>_large`` columns, then all ``<asset>_small``
/// columns, then ``<asset>_winner``, ``<asset>_loser``, ``<asset>_value``,
/// ``<asset>_growth``, ``<asset>_high_vol`` and ``<asset>_low_vol``, with
/// assets in input column order within each phase.
///
/// @param asset_returns Per-asset simple returns.
/// @param window Trailing observations used for ranks and momentum; defaults
///     to 252.
/// @return A binary tilt frame whose index starts at the first eligible row,
///     or an empty frame when fewer than three rows or fewer than two usable
///     trailing observations are available.
/// @throws input_error If the frame contains non-double columns, invalid or
///     duplicate asset names, non-finite returns, or an unrepresentable output
///     becomes necessary.
[[nodiscard]] dataframe calculate_style_tilts(
    const dataframe& asset_returns,
    std::size_t window = 252U);

/// Computes automatic style attribution from a default style analysis.
///
/// Runs ``style_analysis`` with default options, builds style return series
/// with ``analyze_performance_by_style``, and assigns each style its centered
/// Pearson correlation with the equal-weighted portfolio return multiplied by
/// the style's mean return. The ``residual`` is the portfolio mean minus the
/// sum of contributions. A style with fewer than three paired observations or
/// near-zero paired deviation contributes zero. Empty-shaped input yields an
/// empty frame.
///
/// @param asset_returns Per-asset simple returns.
/// @return A frame with ``style`` and ``contribution`` columns plus a final
///     ``residual`` row.
/// @throws input_error If style analysis, performance alignment or
///     attribution validation rejects the input.
[[nodiscard]] dataframe calculate_regression_attribution(
    const dataframe& asset_returns);

/// Computes style attribution for explicit portfolio and style returns.
///
/// The portfolio series is the row-wise mean of all double columns in
/// ``portfolio_returns``. Selected style columns are aligned by timestamp; a
/// style contributes its centered Pearson correlation with the portfolio
/// series multiplied by its mean on the aligned timestamps. Styles are emitted
/// in ``style_returns`` column order, followed by the ``residual`` row, which
/// uses the full portfolio mean. Unmatched selectors are ignored. A style with
/// fewer than three paired timestamps or near-zero aligned deviation
/// contributes zero. An empty-shaped portfolio yields an empty frame.
///
/// @param portfolio_returns One or more double portfolio return columns.
/// @param style_returns Candidate style return columns.
/// @param selected_styles Style column names to attribute; empty names,
///     duplicates and ``residual`` are rejected.
/// @return A frame with ``style`` and ``contribution`` columns plus a final
///     ``residual`` row.
/// @throws input_error If selectors are malformed, a non-empty portfolio has
///     invalid columns or values, a selected style column has invalid
///     structure or non-finite values, or arithmetic overflows.
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
    /// Constructs a result from three existing frames.
    ///
    /// The frames are moved into the result and are not validated here.
    ///
    /// @param exposures Asset-by-style exposure frame.
    /// @param returns_by_style Style/return summary frame.
    /// @param overall_returns Portfolio return frame.
    StyleResult(dataframe exposures, dataframe returns_by_style,
                dataframe overall_returns);

    /// Constructs a result by consuming a style analysis result.
    ///
    /// @param result Analysis result whose frames are moved into this object.
    explicit StyleResult(style_analysis_result result);

    /// Returns the owned exposure frame.
    ///
    /// @return Const reference to the asset-by-style exposures.
    [[nodiscard]] const dataframe& exposures() const noexcept;

    /// Returns the owned style/return summary frame.
    ///
    /// @return Const reference to the style and return columns.
    [[nodiscard]] const dataframe& returns_by_style() const noexcept;

    /// Returns the owned portfolio return frame.
    ///
    /// @return Const reference to the overall returns.
    [[nodiscard]] const dataframe& overall_returns() const noexcept;

    /// Returns a ``style -> average return`` mapping in deterministic key
    /// order.
    ///
    /// The ``style`` and ``return`` columns of ``returns_by_style`` are used
    /// when present; a missing return column falls back to the first double
    /// column, and a missing style column labels values by timestamp. Later
    /// duplicate style labels replace earlier values.
    ///
    /// @return Mapping from style label to its stored return.
    /// @throws fincore_error If no numeric return column exists or the style
    ///     and return columns do not align.
    [[nodiscard]] std::map<std::string, double> style_summary() const;

    /// Returns a dictionary of the three owned frames.
    ///
    /// Keys are ``exposures``, ``returns_by_style`` and ``overall_returns``;
    /// each value is a detached copy.
    ///
    /// @return Map from source key to a copied frame.
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

/// Fetches style factors through an injected provider.
///
/// @param tickers Non-empty ticker list with no empty entries.
/// @param factors Optional factor names passed through unchanged;
///     ``std::nullopt`` mirrors the source's ``None``.
/// @param library Factor library name; must not be empty.
/// @param provider Callable returning a non-empty frame with at least one
///     supported column.
/// @return The provider's validated frame, owned by the caller.
/// @throws fincore_error If the tickers, library or provider are invalid, or
///     the provider returns an empty or column-less frame.
dataframe fetch_style_factors(
    const std::vector<std::string>& tickers,
    const std::optional<std::vector<std::string>>& factors,
    std::string_view library, const style_factor_provider& provider);

/// Fetches style factors with the library defaulting to ``us``.
///
/// @param tickers Non-empty ticker list with no empty entries.
/// @param factors Optional factor names passed through unchanged.
/// @param provider Callable returning a non-empty frame with at least one
///     supported column.
/// @param library Factor library name; defaults to ``us``.
/// @return The provider's validated frame, owned by the caller.
/// @throws fincore_error If the tickers, library or provider are invalid, or
///     the provider returns an empty or column-less frame.
dataframe fetch_style_factors(
    const std::vector<std::string>& tickers,
    const std::optional<std::vector<std::string>>& factors,
    const style_factor_provider& provider, std::string_view library = "us");

}  // namespace fincore
