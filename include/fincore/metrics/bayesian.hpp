#pragma once

#include <fincore/dataframe.hpp>
#include <fincore/metrics/helpers.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fincore {

inline constexpr std::string_view bayesian_cone_percentile_1_column{"percentile_1"};
inline constexpr std::string_view bayesian_cone_percentile_5_column{"percentile_5"};
inline constexpr std::string_view bayesian_cone_percentile_25_column{"percentile_25"};
inline constexpr std::string_view bayesian_cone_percentile_50_column{"percentile_50"};
inline constexpr std::string_view bayesian_cone_percentile_75_column{"percentile_75"};
inline constexpr std::string_view bayesian_cone_percentile_95_column{"percentile_95"};
inline constexpr std::string_view bayesian_cone_percentile_99_column{"percentile_99"};

// Default cone widths mirroring the source tuple (1.0, 1.5, 2.0).
inline constexpr std::array<double, 3U> default_cone_standard_deviations{
    1.0, 1.5, 2.0};

[[nodiscard]] dataframe compute_bayes_cone(
    const dataframe& predicted_returns,
    double starting_value = 1.0);

// Per-date fraction of cumulative predicted paths strictly below the realized
// cumulative return.
//
// `predicted_returns` is row-major with one row per sampled path and one
// column per out-of-sample date. `returns_test` must have one value per
// column. Source formula: realized wealth = cumprod(1 + returns) with NaN
// returns treated as 0 (the source `cum_returns` policy), predicted wealth =
// cumprod(1 + preds) per row, score[t] = |{path : cum_pred[path, t] <
// realized[t]}| / path_count. Predicted values must be finite with a finite
// `1 + value`; realized values may be NaN (treated as 0) but not infinite.
// Zero paths yields NaN scores for every date, matching the source mean of an
// empty axis.
[[nodiscard]] std::vector<double> compute_consistency_score(
    const dataframe& returns_test,
    const matrix& predicted_returns,
    std::string_view column = returns_column);

// Draw `num_samples` x `num_days` bootstrap returns from in-sample returns.
//
// RNG: NumPy legacy RandomState-compatible MT19937 with the explicit 32-bit
// `random_seed` (default 0; the source default `None` is not reproduced).
// Integer draws use NumPy's masked rejection (`rk_interval`), so seeded
// sequences match `np.random.RandomState(seed).randint(0, n, size=...)`
// exactly for 32-bit seeds. This is *not* a claim about array seeding or the
// source's non-deterministic `None` default.
//
// `starting_value` is accepted for source-signature parity and ignored, as in
// the source (`_starting_value`). NaN in-sample values are drawn verbatim;
// infinite values raise `input_error`. An empty population with a non-empty
// draw shape raises `input_error` (source `rng.randint(0, 0, ...)` raises
// ValueError).
[[nodiscard]] matrix simulate_paths(
    std::span<const double> in_sample_returns,
    std::size_t num_days,
    double starting_value = 1.0,
    std::size_t num_samples = 1000U,
    std::uint32_t random_seed = 0U);

// Frame form selects one double column. A source DataFrame with several
// columns would make NumPy indexing return a 3-D array; that shape is not
// representable, so callers select the column to bootstrap.
[[nodiscard]] matrix simulate_paths(
    const dataframe& in_sample_returns,
    std::size_t num_days,
    double starting_value = 1.0,
    std::size_t num_samples = 1000U,
    std::uint32_t random_seed = 0U,
    std::string_view column = returns_column);

// Mean/standard-deviation cone from sampled paths.
//
// `samples` is row-major with one row per path and one column per day.
// Source formula: cumulative wealth per path from cumprod(1 + returns) with
// NaN returns as 0; bounds `mean + std * num_std` and `mean - std * num_std`
// for each requested width, with sample standard deviation (ddof = 1).
// `starting_value == 0` yields `cumprod - 1`; otherwise `starting_value *
// cumprod`, matching `cum_returns`. Columns are ordered upper then lower for
// each width, named `cone_<shortest-round-trip-width>` (for example `cone_1`,
// `cone_-1`, `cone_1.5`); Python float labels are represented as native
// strings, not claimed identical. Duplicate widths raise `input_error`.
[[nodiscard]] dataframe summarize_paths(
    const matrix& samples,
    std::span<const double> cone_std = default_cone_standard_deviations,
    double starting_value = 1.0);

[[nodiscard]] dataframe summarize_paths(
    const matrix& samples,
    double cone_std,
    double starting_value = 1.0);

// Native column label for one cone bound width, e.g. cone_1 / cone_-1.
[[nodiscard]] std::string cone_bounds_column_name(double num_std);

// Compose simulate_paths and summarize_paths, as the source does.
[[nodiscard]] dataframe forecast_cone_bootstrap(
    const dataframe& in_sample_returns,
    std::size_t num_days,
    std::span<const double> cone_std = default_cone_standard_deviations,
    double starting_value = 1.0,
    std::size_t num_samples = 1000U,
    std::uint32_t random_seed = 0U,
    std::string_view column = returns_column);

[[nodiscard]] dataframe forecast_cone_bootstrap(
    const dataframe& in_sample_returns,
    std::size_t num_days,
    double cone_std,
    double starting_value = 1.0,
    std::size_t num_samples = 1000U,
    std::uint32_t random_seed = 0U,
    std::string_view column = returns_column);

}  // namespace fincore
