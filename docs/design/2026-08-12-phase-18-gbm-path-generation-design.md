# Phase 18 GBM Path-Generation Design

**Date:** 2026-08-12

## Decision

Phase 18 ports the actual path-propagation core of Python
`fincore.simulation.paths.geometric_brownian_motion`.  It follows the accepted
Phase 17 calibration leaf: callers can now estimate drift/volatility from a
DataFrame and feed those values into a reproducible native Geometric Brownian
Motion generator.  The scope is the GBM propagator only; calibration-to-path
convenience (`gbm_from_returns`), bootstrap, Monte Carlo orchestration, and
other simulation methods stay separate.

Python returns an anonymous NumPy matrix with paths as rows and time as
columns.  A native `dataframe` is time-indexed, so the C++ adaptation
transposes that representation: ordinal rows are simulation steps and
genuine-`double` columns are named `path_0`, `path_1`, ..., in path order.
This gives every simulated path a stable table identity and avoids a secondary
column-name map.

## Public shape

The new leaf is `include/fincore/simulation/paths.hpp`, later exported by
`include/fincore/simulation.hpp`.

```cpp
inline constexpr std::string_view gbm_path_column_prefix{"path_"};

dataframe geometric_brownian_motion(
    double initial_value,
    double annualized_drift,
    double annualized_volatility,
    double total_years,
    double time_step,
    std::size_t path_count,
    std::optional<std::uint64_t> seed = std::nullopt);
```

For `n_steps = trunc(total_years / time_step)`, output has ordinal index
`{0, ..., n_steps}` and exactly `path_count` genuine-`double` columns in
path-number order.  Every column's first value is `initial_value`.  Each next
value uses the source equation:

```text
S(t + dt) = S(t) * exp((mu - 0.5 * sigma^2) * dt
                        + sigma * sqrt(dt) * Z)
```

where `Z` is a standard-normal native shock.  Thus zero volatility gives the
same deterministic geometric sequence as the Python source, independently of
the generator.

## Native randomness and boundaries

With an explicit seed, C++ uses a documented local `std::mt19937_64` stream
and Box-Muller normal transform, so repeated calls in the same supported
native build reproduce the same table.  Without a seed, it seeds the engine
from `std::random_device`.  This is a reproducibility promise for the native
API, not a claim that C++ shock samples equal NumPy `default_rng` samples for
the same integer seed; the two projects deliberately use different generator
algorithms.

All five floating model inputs must be finite.  `time_step` must be strictly
positive, `total_years` nonnegative, and `path_count` nonzero.  Like Python's
`int(T / dt)`, a finite duration shorter than one step produces an
initial-value-only table.  The native leaf rejects unrepresentable step/index,
path-name, multiplication/allocation, exponent, or path-value arithmetic with
`fincore::input_error` rather than emitting partial/infinite DataFrames.  It
does not add a price-sign policy absent from Python: a finite zero or negative
initial value is propagated algebraically.

The output owns all index, labels, and value vectors.  No historical returns
DataFrame, calendar/timestamp alignment, resampling, factor model, provider,
or cache participates in this call.

## Source oracle

The required local source oracle through Anaconda produced:

```text
geometric_brownian_motion(100.0, 0.1, 0.0, 1.0, 0.25, 2, seed=42)
[[100.0, 102.53151205244289, 105.12710963760242,
  107.78841508846317, 110.51709180756478],
 [100.0, 102.53151205244289, 105.12710963760242,
  107.78841508846317, 110.51709180756478]]

geometric_brownian_motion(100.0, 0.0, 0.0, 0.1, 0.25, 1, seed=42)
[[100.0]]
```

Native tests compare the zero-volatility sequence exactly up to normal
floating tolerance, prove the transposed named DataFrame shape, and prove
same-seed native reproducibility.  They do not compare nonzero-volatility
samples against NumPy because that would falsely specify two different RNGs as
one.

## Scope and verification

The leaf begins with a public-header-only RED test plus the complete source
oracle.  GREEN tests cover deterministic source paths, one/zero-step shapes,
named typed schema/order, same-seed equality/different-seed distinction,
unseeded valid construction, zero/negative initial values, all finite input
and representability errors, and full output ownership.  A later
simulation-umbrella-only C++23 consumer proves public export.

This phase excludes `estimate_simulation_parameters` changes, `gbm_from_returns`,
antithetic variates, Latin-hypercube sampling, bootstrap, Monte Carlo,
`SimResult`, correlation/stress behavior, optimization, providers, reports,
full NumPy RNG compatibility, GUI runtime, and packaging/release.  Fresh
AppleClang, Homebrew LLVM ASan+UBSan, ImPlot compile/link-only, and
invalid-ImGui configuration remain acceptance gates; no GUI is launched.
