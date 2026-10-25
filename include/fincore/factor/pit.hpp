#pragma once

#include <fincore/factor/data.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace fincore {

/// One point-in-time revision of an asset's factor value.
///
/// The dates must satisfy `as_of <= known_at <= effective_from`.
struct pit_point {
    timestamp as_of = 0; ///< Date the value describes.
    timestamp known_at = 0; ///< Date the value became known; at least `as_of`.
    timestamp effective_from = 0; ///< First effective date; at least `known_at`.
    double value = 0.0; ///< Factor value; must be finite.
};

/// Asset-scoped point-in-time observation used to materialize a panel.
struct pit_observation {
    std::string asset; ///< Non-empty asset identifier.
    pit_point point; ///< Revision of the asset's factor value.
    bool in_universe = true; ///< Whether the asset enters the materialized panel.
};

/// Validates point-in-time ordering and finiteness.
///
/// Each point must carry a finite value, with `known_at` not before `as_of`
/// and `effective_from` not before `known_at`.
///
/// @param points Points to check; an empty batch is valid.
/// @throws factor_data_error When a point violates finiteness or ordering.
void validate_pit_alignment(const std::vector<pit_point>& points);

/// Materializes a factor panel from point-in-time observations.
///
/// For each evaluation date, an asset contributes the revision with the
/// largest (`effective_from`, `known_at`, `as_of`) among revisions that are
/// already known and effective on that date. Assets flagged outside the
/// universe are skipped, and assets without an eligible revision are absent
/// from that date.
///
/// @param observations Point-in-time observations with non-empty assets.
/// @param evaluation_dates Strictly increasing dates to evaluate.
/// @return Long-form panel ordered by date and asset.
/// @throws factor_data_error When dates are unsorted, an asset is empty, a
///         point is invalid, or a revision is duplicated.
factor_series materialize_pit_factor(
    const std::vector<pit_observation>& observations,
    const std::vector<timestamp>& evaluation_dates);

/// Prepares a quantified factor panel from point-in-time observations.
///
/// The observations are first materialized into a factor panel and then
/// prepared against `prices`, so only information known and effective on each
/// evaluation date reaches the output.
///
/// @param observations Point-in-time observations with non-empty assets.
/// @param prices Price panel used for forward returns.
/// @param evaluation_dates Strictly increasing dates to evaluate.
/// @param config Horizons, bucket settings, grouping and loss budget.
/// @return Retained rows, forward labels and the cleaning-loss report.
/// @throws factor_data_error When `config.filter_zscore` is set, no eligible
///         value materializes, or preparation inputs are invalid.
/// @throws factor_loss_exceeded When total cleaning loss exceeds
///         `config.max_loss`.
prepared_factor_data prepare_pit_factor_data(
    const std::vector<pit_observation>& observations,
    const dataframe& prices, const std::vector<timestamp>& evaluation_dates,
    const prepare_factor_config& config = {});

}  // namespace fincore
