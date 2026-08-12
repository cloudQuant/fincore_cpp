# Phase 20 Bootstrap Distribution Design

**Date:** 2026-08-12

## Decision

Phase 20 adds the DataFrame-native distribution-producing core of Python
`fincore.simulation.bootstrap.bootstrap`.  The chosen scope is deliberately
between two less useful extremes: it implements all seven built-in source
statistics in one reusable bootstrap sampler, but it does not add percentile
confidence intervals, the multi-statistic summary dictionary, arbitrary Python
callables, `MonteCarlo`, antithetic or Latin-hypercube sampling.

That boundary is useful on its own: callers obtain a typed, indexed bootstrap
distribution that can be passed to the already accepted native metrics and
calendar tools.  It also avoids prematurely specifying percentile/interpolation
or report-table contracts while the random sampling and statistic semantics are
still new at the C++ boundary.

## Alternatives considered

| Option | Scope | Decision |
| --- | --- | --- |
| A | Distribution sampler plus Python's seven named built-ins | **Accepted** |
| B | Also port `bootstrap_ci` and `bootstrap_summary` | Deferred: percentile and summary schemas need their own public contracts |
| C | Mean-only sampler | Rejected: it would hide the source's existing `std`, median, ratio, min, and max behavior behind a later incompatible API |

## Public API and DataFrame shape

The new leaf is `include/fincore/simulation/bootstrap.hpp`; a later acceptance
task exports it through `include/fincore/simulation.hpp`.

```cpp
enum class bootstrap_statistic {
    mean,
    sample_standard_deviation,
    median,
    sharpe_ratio,
    sortino_ratio,
    minimum,
    maximum,
};

inline constexpr std::string_view bootstrap_statistic_column{"bootstrap"};

dataframe bootstrap_distribution(
    const dataframe& returns,
    std::size_t resample_count = 10000U,
    bootstrap_statistic statistic = bootstrap_statistic::mean,
    std::optional<std::uint64_t> seed = std::nullopt,
    std::string_view column = returns_column);
```

The function reads exactly one genuine-`double`, index-length-matched source
column.  Its timestamps are observational only: bootstrap sampling uses source
value order and neither aligns, sorts, deduplicates, resamples calendar periods,
nor modifies the input.  The result owns an ordinal index `{0, ..., n-1}` and
exactly one genuine-`double` column named `bootstrap`; `n` is
`resample_count`.  A zero count is valid and gives the typed empty shape.

The supplied selector must be nonempty and NUL-free before any DataFrame
C-string boundary.  Missing, wrong-typed, short, malformed, all-NaN, infinite,
unrepresentable, or allocation-failing input is reported as `fincore::input_error`.
As in Python, NaNs are dropped before sampling.  Unlike Python's permissive
NumPy flow, signed-infinite input is rejected explicitly; this follows the
accepted native simulation-input safety policy.  Finite degenerate samples can
still intentionally yield IEEE `NaN` or positive infinity for the source's
`std`, Sharpe, and Sortino definitions, and those values remain valid entries
in the output DataFrame.

## Sampling and named statistic semantics

After filtering NaNs, every bootstrap row is a same-length sample drawn with
replacement from the retained source values.  An explicit seed creates a fresh
local `std::mt19937_64` stream; a missing seed obtains a local
`std::random_device` seed.  Bounded sampling uses a rejection-mapped raw engine
word rather than a cross-library distribution object, so the native API can
promise repeated results for the same supported native build and seed.  It does
not promise equality with NumPy `default_rng` for the same integer seed.

The seven enum cases directly express the source map:

- `mean`: arithmetic sample mean.
- `sample_standard_deviation`: sample standard deviation (`ddof=1`), hence
  `NaN` for a one-value sample.
- `median`: midpoint median of the sorted sample.
- `sharpe_ratio`: `mean / sample_std * sqrt(252)`; fewer than two values yield
  `NaN`, while zero standard deviation yields `NaN` only for zero mean and
  positive infinity otherwise, matching Python's helper.
- `sortino_ratio`: applies the same annualized rule to the sample standard
  deviation after replacing only values strictly greater than zero with zero.
- `minimum` and `maximum`: sample extrema.

Unknown enum values, representability failures, and finite-statistic arithmetic
that overflows outside these deliberately specified degenerate ratio cases are
`input_error`; no partial table is returned.  All result index labels, column
name, and values are materialized by value.

## Source oracle and verification

The required Python source oracle uses the user's Anaconda environment.  It
records the real source outputs for a nonconstant seeded fixture and the
deterministic fixtures that bridge different RNG implementations: all-equal
input makes mean/median/min/max invariant, gives zero standard deviation, and
gives positive infinite ratios; a singleton makes sample standard deviation,
Sharpe, and Sortino `NaN`; zero sample count gives an empty ndarray.  A
NaN-containing all-equal fixture proves Python's NaN drop rule.

Native tests begin as a leaf-public-header RED.  GREEN tests prove exact public
signature/schema/order, deterministic Python-compatible cases, per-statistic
relationships on nonconstant samples, seed repetition/divergence, zero-count
typed output, NUL/column/type/length/all-NaN/infinite/unknown-enum failures,
representability guards, and value ownership after source mutation/destruction.
They do not claim nonconstant NumPy random-stream equality.

The later facade task must capture a true umbrella-only RED before adding the
single bootstrap include, then record fresh AppleClang headless, Homebrew LLVM
ASan+UBSan, ImPlot compile/link-only/no-GUI, and invalid-ImGui gates with their
raw logs and actual dependency provenance.  Bootstrap confidence intervals,
summary, callable statistics, Python RNG parity, `MonteCarlo`, providers,
reports, packaging, and GUI runtime remain outside this phase.
