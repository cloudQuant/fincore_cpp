# Phase 16 Correlation-Breakdown Matrix Design

**Date:** 2026-08-12

## Decision

Phase 16 adds the deterministic matrix primitive of Python
`fincore.simulation.scenarios.generate_correlation_breakdown`.  The Python
function accepts an asset count and returns an unlabeled NumPy matrix.  Native
code accepts explicit asset names and returns a typed `dataframe`, so callers
retain the identity and ordering of every matrix column without an auxiliary
name map.

This slice is intentionally just the matrix constructor.  It does not apply a
correlation shock to price/return paths, validate positive semidefiniteness,
estimate correlations, introduce random simulation, or alter the accepted
stress-scenario engine.

## Public shape

The new leaf is `include/fincore/simulation/correlation.hpp` and is later
exported by `include/fincore/simulation.hpp`.

```cpp
dataframe generate_correlation_breakdown(
    std::span<const std::string_view> asset_names,
    double target_correlation = 1.0);
```

For `N` supplied asset names, the result has ordinal timestamp index
`{0, ..., N - 1}`, exactly `N` genuine `double` columns in caller order, and
no extra columns.  Row `i`, column `j` is:

```text
1.0                when i == j
target_correlation otherwise
```

That is the literal finite-value behavior of the Python source: the diagonal
is forced to `1.0`, and every off-diagonal cell is the supplied target.  The
function deliberately does not impose a positive-semidefinite range or
silently clamp a finite target; callers that need a valid correlation matrix
remain responsible for choosing one.

## Boundary contract

Asset names are copied before any DataFrame C-string boundary.  Each name must
be nonempty, contain no embedded NUL, and be distinct as a complete
`std::string_view`; a bounded non-NUL-terminated view is valid.  This makes
the DataFrame column schema deterministic and prevents C-string truncation.

An empty span is valid and produces the source-equivalent `0 x 0` DataFrame:
an empty index and no data columns.  For nonempty input, the implementation
checks ordinal-index representability and avoids size arithmetic overflow
before allocation.  `target_correlation` must be finite because the native
typed table never manufactures NaN/infinity from an otherwise structural
request.  Finite values outside the conventional `[-1, 1]` range are retained
literally, matching the source's direct fill semantics rather than adding a
statistical policy not present in Python.

All name and numeric vectors are materialized by value.  Mutation or
destruction of the caller's name storage cannot affect the returned matrix.
No input time series, alignment, resampling, factor regression, provider, or
cache is involved.

## Source oracle

The required local Python source was run through the Anaconda environment:

```text
generate_correlation_breakdown(3, 1.0)
[[1.0, 1.0, 1.0], [1.0, 1.0, 1.0], [1.0, 1.0, 1.0]]

generate_correlation_breakdown(3, 0.25)
[[1.0, 0.25, 0.25], [0.25, 1.0, 0.25], [0.25, 0.25, 1.0]]

generate_correlation_breakdown(3, -0.5)
[[1.0, -0.5, -0.5], [-0.5, 1.0, -0.5], [-0.5, -0.5, 1.0]]

generate_correlation_breakdown(0, 0.25)
[]
```

Native tests compare those numeric values exactly and additionally prove the
explicit named-column schema that the Python ndarray cannot carry.

## Scope and verification

The leaf starts with a public-header-only RED test and the complete source
oracle.  GREEN tests prove all source matrices, ordered typed schema,
empty/one/multi-asset handling, finite out-of-range pass-through, strict
name/finite validation, bounded views, and output ownership.  A later
simulation-umbrella-only C++23 consumer proves the public export.

This phase excludes correlation estimation/repair, PSD checks, covariance
conversion, shock application, portfolio optimization, Monte Carlo, bootstrap,
GBM, existing stress-scenario behavior, market/provider data, reports,
releases, full Python simulation parity, and GUI runtime.  Fresh AppleClang,
Homebrew LLVM ASan+UBSan, ImPlot compile/link-only, and invalid-ImGui
configuration evidence are retained at acceptance; no GUI is launched.
