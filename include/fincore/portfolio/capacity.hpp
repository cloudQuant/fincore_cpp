#pragma once

#include <fincore/portfolio/transactions.hpp>

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace fincore {

/// Parameters controlling assess_liquidity().
struct capacity_config {
    /// Maximum fraction of a bar's dollar volume assumed executable per day;
    /// must be in (0, 1].
    double max_bar_consumption = 0.2;
    /// Capital in currency units used to scale position allocations; must be
    /// positive.
    double capital_base = 1'000'000.0;
    /// Number of preceding bars in the rolling mean dollar volume; must be at
    /// least 1.
    std::size_t mean_volume_window = 5U;
    /// Optional trailing-day scan limit; when set, must be at least 1.
    std::optional<std::size_t> last_n_days;
};

/// Value-owned results of assess_liquidity().
struct capacity_assessment {
    /// Days to liquidate per non-cash asset, indexed by position dates after
    /// the rolling-volume warm-up.
    dataframe liquidation_days;
    /// Maximum days to liquidate and allocation percentage per symbol, sorted
    /// by symbol.
    std::vector<ticker_liquidity> ticker_maximums;
    /// Maximum bar-consumption percentage per symbol in the scan window,
    /// sorted by symbol.
    std::vector<transaction_liquidity> low_liquidity_transactions;
};

/// Assesses position and transaction liquidity.
///
/// Combines days_to_liquidate_positions(), get_max_days_to_liquidate_by_ticker()
/// and get_low_liquidity_transactions() using one validated configuration.
///
/// @param positions Wide position frame with a double column per asset; a cash
///     column is excluded when present.
/// @param transactions Trades used for the low-liquidity scan.
/// @param prices Bar price frame with the same index as ``positions`` and a
///     double column per position asset.
/// @param volumes Bar share-volume frame with the same index as ``positions``
///     and a double column per position asset.
/// @param config Liquidity parameters, validated before any computation.
/// @return Value-owned liquidation frame and per-symbol liquidity records.
/// @throws input_error If ``config`` is invalid, an input index differs, a
///     transaction amount is non-finite, a trade date is absent from the price
///     index, or a position double column is malformed. The price and volume
///     frames must also provide a double column for every position asset.
capacity_assessment assess_liquidity(
    const dataframe& positions,
    std::span<const trade_record> transactions,
    const dataframe& prices,
    const dataframe& volumes,
    const capacity_config& config = {});

}  // namespace fincore
