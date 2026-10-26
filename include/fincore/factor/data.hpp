#pragma once

#include <fincore/dataframe.hpp>

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace fincore {

/// Long-form factor panel with row-aligned dates, assets and values.
///
/// Each index is one observation. Preparation and quantization require the
/// three vectors to have equal lengths and (date, asset) pairs to be unique.
struct factor_series {
    std::vector<timestamp> dates; ///< Observation dates.
    std::vector<std::string> assets; ///< Asset identifiers.
    std::vector<double> values; ///< Factor values, one per row.
};

/// Forward returns for factor observations, stored label by label.
///
/// `dates` and `assets` form the row keys; `values` holds one vector per
/// entry of `labels`, each aligned with those rows.
struct forward_return_table {
    std::vector<std::string> labels; ///< Horizon labels such as `"1D"`.
    std::vector<timestamp> dates; ///< Row dates, parallel to `assets`.
    std::vector<std::string> assets; ///< Row assets, parallel to `dates`.
    std::vector<std::vector<double>> values; ///< Labels x rows, NaN missing.
};

/// Cleaning-loss accounting for one factor preparation.
///
/// Each loss member is the share of `input_count` dropped at that stage, so
/// the three stage losses sum to `total_loss`.
struct factor_loss_report {
    std::size_t input_count = 0U; ///< Rows in the input factor panel.
    std::size_t finite_factor_count = 0U; ///< Rows with a finite factor value.
    std::size_t forward_returns_count = 0U; ///< Rows with all forward labels finite.
    std::size_t binning_count = 0U; ///< Rows retained after quantization.
    double factor_input_loss = 0.0; ///< Share lost to a non-finite factor.
    double forward_returns_loss = 0.0; ///< Share lost to missing forward returns.
    double binning_loss = 0.0; ///< Share dropped when assigning quantiles.
    double total_loss = 0.0; ///< Sum of the three staged losses.
};

/// Loss wording used by the pinned strict facade's stdout projection.
///
/// Returns ``(input_count - forward_returns_count) / input_count`` and 0 for
/// an empty input.
double legacy_forward_returns_loss(const factor_loss_report& report);

/// Asset-to-group mapping used for grouped quantization and preparation.
///
/// An empty `group_by_asset` map disables grouping.
struct factor_grouping {
    std::map<std::string, std::string> group_by_asset; ///< Asset to group name.
    std::map<std::string, std::string> group_labels; ///< Optional group label remapping.
};

/// Bucket assignment settings for a factor cross-section.
struct quantization_config {
    int quantiles = 5; ///< Bucket count when `use_bins` is false.
    bool use_bins = false; ///< Uses equal-width bins instead of quantiles.
    std::vector<double> bins; ///< Explicit bin edges; empty derives them.
    bool by_group = false; ///< Buckets within each (date, group) cross-section.
    bool no_raise = false; ///< Labels failed buckets empty instead of throwing.
    bool zero_aware = false; ///< Separates negative and non-negative values.
};

/// Full configuration for factor preparation.
struct prepare_factor_config {
    int quantiles = 5; ///< Quantile bucket count.
    bool use_bins = false; ///< Uses equal-width bins instead of quantiles.
    std::vector<double> bins; ///< Explicit bin edges; empty derives them.
    std::vector<int> periods{1, 5, 10}; ///< Forward horizons in observations.
    double max_loss = 0.35; ///< Maximum tolerated total cleaning loss in [0, 1].
    /// Quantizes within each (date, group) cross-section.
    bool binning_by_group = false;
    std::optional<double> filter_zscore; ///< Per-asset forward-return z-score cutoff.
    bool zero_aware = false; ///< Separates negative and non-negative values.
    bool cumulative_returns = true; ///< Compounds returns over the horizon when true.
    factor_grouping grouping; ///< Asset-to-group mapping; empty disables grouping.
};

/// One prepared factor observation.
struct factor_row {
    timestamp date = 0; ///< Observation date.
    std::string asset; ///< Asset identifier.
    double factor = 0.0; ///< Finite factor value.
    int quantile = 0; ///< One-based quantile label.
    bool has_group = false; ///< Whether a group label was resolved.
    std::string group; ///< Group label, empty when ungrouped.
    std::vector<double> forward_returns; ///< Returns aligned with `forward_labels`.
};

/// Prepared factor rows with their labels and cleaning-loss report.
struct prepared_factor_data {
    std::vector<factor_row> rows; ///< Retained observations.
    std::vector<std::string> forward_labels; ///< Labels aligned with row returns.
    factor_loss_report loss_report; ///< Cleaning-loss accounting.
};

/// Prepared factor panels, one per forward-return horizon.
struct multi_horizon_prepared_factor_data {
    /// Label and panel pairs in the order of the requested periods.
    std::vector<std::pair<std::string, prepared_factor_data>> by_horizon;
};

/// Computes forward returns for every factor observation from a price panel.
///
/// Labels are the requested periods rendered as `"<period>D"`. A value is NaN
/// when the factor date is absent from `prices`, the horizon runs past the
/// panel, or the base price is zero. When `filter_zscore` is set, forward
/// returns further than that many standard deviations from the per-asset mean
/// are removed independently for each label.
///
/// @param factor Row-aligned factor panel supplying dates and assets.
/// @param prices Price panel indexed by date with one column per factor asset.
/// @param periods Positive horizons in price observations.
/// @param filter_zscore Optional per-asset z-score cutoff; nullopt keeps all.
/// @param cumulative_returns Compounded window return when true, otherwise a
///        one-observation return.
/// @return Label-major forward returns aligned with the factor rows.
/// @throws factor_data_error When the factor index is invalid, a period is not
///         positive, an asset is missing from `prices`, no date is shared, or
///         `filter_zscore` is non-finite.
forward_return_table compute_forward_returns(
    const factor_series& factor, const dataframe& prices,
    const std::vector<int>& periods = {1, 5, 10},
    std::optional<double> filter_zscore = std::nullopt,
    bool cumulative_returns = true);

/// Assigns one-based bucket labels to factor observations.
///
/// Buckets are built per date, or per (date, group) pair when
/// `config.by_group` is true. Non-finite factor values receive no label.
///
/// @param factor Row-aligned factor panel to label.
/// @param config Bucket count, explicit bins and failure handling.
/// @param grouping Asset-to-group mapping required when binning by group.
/// @return One optional label per factor row; empty when no label applies.
/// @throws factor_data_error When no usable quantiles or bins are configured,
///         a group is missing, or bucket edges are not unique.
std::vector<std::optional<int>> quantize_factor(
    const factor_series& factor,
    const quantization_config& config = {},
    const factor_grouping& grouping = {});

/// Prepares a quantified factor panel from raw factor values and prices.
///
/// Forward returns are computed with `config.periods`, observations without a
/// finite value for every label are dropped, and the remainder is quantified
/// within the configured cross-sections.
///
/// @param factor Row-aligned factor panel to prepare.
/// @param prices Price panel used for forward returns.
/// @param config Horizons, bucket settings, grouping and loss budget.
/// @return Retained rows, forward labels and the cleaning-loss report.
/// @throws factor_data_error When an input or configuration value is invalid.
/// @throws factor_loss_exceeded When total cleaning loss exceeds
///         `config.max_loss`.
prepared_factor_data prepare_factor_data(
    const factor_series& factor, const dataframe& prices,
    const prepare_factor_config& config = {});

/// Prepares a quantified factor panel from a supplied forward-return table.
///
/// The table must carry unique labels and unique (date, asset) rows, with one
/// value per row for every label. Observations whose pair is absent or whose
/// value is not finite are dropped and charged to the loss report.
///
/// @param factor Row-aligned factor panel to prepare.
/// @param forward_returns Precomputed forward returns to align by date/asset.
/// @param config Bucket settings, grouping and loss budget.
/// @return Retained rows, forward labels and the cleaning-loss report.
/// @throws factor_data_error When the factor or forward table is malformed.
/// @throws factor_loss_exceeded When total cleaning loss exceeds
///         `config.max_loss`.
prepared_factor_data prepare_factor_data_from_forward_returns(
    const factor_series& factor, const forward_return_table& forward_returns,
    const prepare_factor_config& config = {});

/// Prepares one quantified panel per forward-return horizon.
///
/// Each horizon is bucketed against the cross-section with a finite return
/// for that horizon and checked against `config.max_loss` separately.
///
/// @param factor Row-aligned factor panel to prepare.
/// @param prices Price panel used for forward returns.
/// @param config Horizons, bucket settings, grouping and loss budget.
/// @return Per-horizon prepared panels in the order of `config.periods`.
/// @throws factor_data_error When an input is invalid or `config.filter_zscore`
///         is set, since per-horizon preparation has no full-sample filter.
/// @throws factor_loss_exceeded When a horizon exceeds `config.max_loss`.
multi_horizon_prepared_factor_data prepare_factor_data_by_horizon(
    const factor_series& factor, const dataframe& prices,
    const prepare_factor_config& config = {});

}  // namespace fincore
