#pragma once

#include <fincore/simulation/parameters.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace fincore {

/// Prefix of generated geometric-Brownian-motion path columns.
inline constexpr std::string_view gbm_path_column_prefix{"path_"};
/// Prefix of Latin-hypercube dimension columns.
inline constexpr std::string_view latin_hypercube_dimension_column_prefix{
    "lhs_dimension_"};
/// Prefix of generated antithetic mirror columns.
inline constexpr std::string_view antithetic_path_column_prefix{
    "antithetic_"};

/// Samples a Latin hypercube design.
///
/// Every output column is a permutation of ``sample_count`` equal-probability
/// strata in [0, 1), with one uniformly distributed point per stratum. The
/// index is ordinal and columns are named ``lhs_dimension_<n>``. Zero counts
/// produce the corresponding empty shape. A supplied ``seed`` makes the native
/// 64-bit Mersenne Twister stream repeatable; omitted seeds are not
/// reproducible and native draws make no cross-implementation RNG claim.
///
/// @param sample_count Number of rows to generate.
/// @param dimension_count Number of independent dimensions (columns).
/// @param seed Optional deterministic seed.
/// @return A frame with an ordinal index and one double column per dimension.
/// @throws input_error If the requested index, names, stratum count or output
///     shape is unrepresentable, or a sampled point is not finite.
dataframe latin_hypercube_sampling(
    std::size_t sample_count,
    std::size_t dimension_count,
    std::optional<std::uint64_t> seed = std::nullopt);

/// Simulates geometric Brownian motion price paths.
///
/// Each path starts at ``initial_value`` and evolves for
/// ``trunc(total_years / time_step)`` steps using
/// ``S[k + 1] = S[k] * exp((drift - volatility^2 / 2) * time_step +
/// volatility * sqrt(time_step) * Z)`` with standard normal draws ``Z``. The
/// output index is ordinal and paths are named ``path_<n>``; the first row is
/// the initial value.
///
/// @param initial_value Starting price of every path.
/// @param annualized_drift Annualized drift of the process.
/// @param annualized_volatility Annualized volatility of the process.
/// @param total_years Total horizon in years.
/// @param time_step Time between observations in years; must be positive.
/// @param path_count Number of paths; must be positive.
/// @param seed Optional deterministic seed; omitted seeds are not
///     reproducible.
/// @return A frame with one initial row plus one row per truncated step, and
///     one double column per path.
/// @throws input_error If an input is not finite, ``time_step`` is not
///     positive, ``total_years`` is negative, ``path_count`` is zero, or the
///     requested index, names or output shape is unrepresentable.
dataframe geometric_brownian_motion(
    double initial_value,
    double annualized_drift,
    double annualized_volatility,
    double total_years,
    double time_step,
    std::size_t path_count,
    std::optional<std::uint64_t> seed = std::nullopt);

/// Simulates cumulative-return paths calibrated from historical returns.
///
/// Annualized drift and volatility are estimated from the selected return
/// column (NaN values dropped), prices are simulated from 1.0 over ``horizon``
/// periods at ``periods_per_year``, the initial price row is removed, and each
/// path stores ``price_t - 1``. Columns are named ``path_<n>``.
///
/// @param returns Frame containing the selected return column.
/// @param horizon Number of periods to simulate.
/// @param path_count Number of paths; must be positive.
/// @param periods_per_year Positive periods per year used for calibration and
///     step sizing.
/// @param seed Optional deterministic seed; omitted seeds are not
///     reproducible.
/// @param column Double return column used for calibration.
/// @return A frame whose index excludes the initial row and whose path columns
///     hold cumulative returns.
/// @throws input_error If calibration or path generation rejects the input, or
///     the requested names or output shape is unrepresentable.
dataframe gbm_from_returns(
    const dataframe& returns,
    std::size_t horizon = 252U,
    std::size_t path_count = 1000U,
    std::size_t periods_per_year = 252U,
    std::optional<std::uint64_t> seed = std::nullopt,
    std::string_view column = returns_column);

/// Appends antithetic mirror paths to a path frame.
///
/// Every double column is retained, and a companion column named
/// ``antithetic_<name>`` is added with values ``2 * c - x``, where ``c`` is
/// the first value of the first double column. The input index and column
/// order are preserved, and mirror columns follow the source columns.
///
/// @param paths Path frame with only double columns.
/// @return A frame containing the original columns followed by their mirrors.
/// @throws input_error If the frame has no rows or no double columns, contains
///     a non-double column, has mismatched column lengths, a derived name
///     collides with an input name, or a derived name is unrepresentable.
dataframe antithetic_variates(const dataframe& paths);

}  // namespace fincore
