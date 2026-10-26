# Phase 17 Simulation Parameter-Estimation Design

**Date:** 2026-08-12

## Decision

Phase 17 adds the deterministic calibration primitive behind Python
`fincore.simulation.base.estimate_parameters`.  It converts one named,
genuine-`double` return column from the native DataFrame into annualized drift
and volatility.  This is intentionally the small, non-random foundation for a
later GBM/path-generation slice; it does not introduce a random engine,
simulation path, bootstrap sampler, or `SimResult` replacement.

The Python function accepts an anonymous NumPy array and returns a tuple.  The
native API instead takes the established named `dataframe` series and returns
a self-describing value struct.  The timestamp index is validated indirectly
through the existing double-column boundary but does not participate in the
calculation, matching Python's positional array semantics.

## Public shape

The new leaf is `include/fincore/simulation/parameters.hpp`, later exported by
`include/fincore/simulation.hpp`.

```cpp
struct simulation_parameters {
    double drift;
    double volatility;
};

simulation_parameters estimate_simulation_parameters(
    const dataframe& returns,
    std::size_t periods_per_year = 252,
    std::string_view column = returns_column);
```

For retained observations `r_1, ..., r_n`, it returns:

```text
mean       = sum(r_i) / n
sample_std = sqrt(sum((r_i - mean)^2) / (n - 1))
drift      = mean * periods_per_year
volatility = sample_std * sqrt(periods_per_year)
```

This is the same annualized-drift and sample-volatility rule used by the
Python source.  The struct is value-only; it retains neither the frame nor any
column/index storage.

## Native boundary policy

Python removes NaNs before estimation, so the native leaf likewise discards
NaN observations without changing the order of the retained values.  Native
code is deliberately stricter for all other boundary cases: positive or
negative infinity, fewer than two retained observations, zero
`periods_per_year`, malformed/missing/non-`double` columns, and non-finite
intermediate or final calculations raise `fincore::input_error`.  In
particular, Python's one-observation `NaN` volatility warning is not exposed
as a native result because it is not a usable calibration for a later path
engine.

This is a narrow safety adaptation, not a filtering policy for finite values:
finite returns are neither resampled, sorted, aligned, clipped, compounded,
nor otherwise transformed.  The supplied timestamp index does not affect
which observations remain.  The implementation should use a checked stable
mean/sample-variance computation and reject overflow rather than manufacture
an infinite calibration.

## Source oracle

The local Python source was run via the required Anaconda environment with:

```text
returns = [0.01, NaN, -0.02, 0.03, 0.0]
frequency = 252
estimate_parameters(...) == (1.2599999999999998, 0.3304542328371662)
```

The native tests compare the finite source oracle and separately make the
intentional native error boundaries explicit.  A second annualization value
uses a non-default positive period count to prove that the integer factor is
applied independently to drift and volatility.

## Scope and verification

The leaf begins with a public-header-only RED test and a complete local source
oracle.  GREEN tests cover the source values, NaN removal, custom columns,
timestamp-order independence, sample denominator, invalid column/type/length
inputs, insufficient retained data, infinities, zero period count, and
overflow rejection.  A later umbrella-only C++23 consumer proves the public
export and value-only result shape.

This phase excludes the standalone Python `annualize` helper, generic return
validation, `SimResult`, percentile/VaR/CVaR calculations, GBM,
`gbm_from_returns`, antithetic variates, Latin-hypercube sampling, bootstrap,
Monte Carlo classes, correlation/stress behavior, optimization, providers,
reports, full Python parity, and GUI runtime.  Fresh AppleClang, Homebrew LLVM
ASan+UBSan, ImPlot compile/link-only, and invalid-ImGui configuration evidence
remain acceptance gates; no GUI is launched.
