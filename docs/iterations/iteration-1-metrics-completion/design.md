# Iteration 1 — Metrics Completion: Design

**Status:** Revised plan. Applies with [delivery-contract.md](../delivery-contract.md).

## 1. Existing architecture and Phase 38 close-out

Use existing `include/fincore/metrics/<leaf>.hpp` and
`src/metrics/<leaf>.cpp`. Phase 38's actual test owners are
`tests/unit/ratios_test.cpp` and `tests/unit/metrics_umbrella_test.cpp`.
Do not invent a `<leaf>_extended_test.cpp` convention: inspect
`tests/CMakeLists.txt` and name real TUs per phase. Existing
`metrics.hpp` already includes `ratios.hpp`; Phase 38 adds no umbrella include.

The current adjusted-Sharpe implementation has ordinary-path checks but
not the full designed boundary matrix. Complete that matrix without changing
the accepted public stats filtering to fit this ratio: its private NaN-only
moment calculation has intentionally different semantics.

## 2. Baseline and scope design

Phase 39 freezes the source/environment manifest and performs paired old/new
oracle comparisons before updating any golden. Keep exact classifications
and both result sets in the drift record. Enumerate public submodule routes,
aliases, catalog operations and consumer scenarios into the capability ledger.
Record historical native acceptance separately from new-pin regression status.

Create the benchmark baseline and data sizes before algorithm changes.
Scope Phase 52b into small residual families using that inventory; preserve
cross-iteration owners for EVT/beta fragility. A missing public route cannot
be dismissed as a Python facade without examining its semantics.

## 3. Numerical reuse and public models

Before each leaf, write a reuse map of native return, drawdown, beta,
annualization, percentile, rolling and bootstrap primitives. Reuse only when
filtering, count, alignment and evaluation order match. Preserve a private
helper with a documented reason where contracts differ.

Normal CDF/inverse and the Stutzer bounded minimizer remain internal. Their
phase designs record algorithms, domains, stopping rules, numerical budgets
and adversarial fixtures. Do not mandate bit equality with SciPy special
functions or make an unmeasured dependency decision. Shared helpers move to
an internal module only when there is an actual second consumer.

Drawdown tables are owning rows with explicit date/recovery status and owned
underwater values. C++ field/type choices may differ from pandas, but a
column/shape/ordering mapping is mandatory. No returned view into a destroyed
input frame. Historical-period data is versioned with the oracle pin.

For bootstrap, reuse accepted simulation primitives where semantics agree.
Provide a test seam for explicit resampling indices or characterize kernels
separately from sampling. Keep native RNG repeatability and the Python
conditional-on-draws comparison as distinct evidence.

## 4. Execution sequence

38 → 39 first. Independent ratio/statistic/table slices then follow their
actual dependencies. Phase 49's scalar-minimizer proof is part of 49's entry,
not a dependency on Phase 60. Phase 52b date aggregation supports any weekly/
monthly consecutive and rolling gaps. Phase 52 builds on accepted bootstrap
primitives and the frozen RNG contract. Close assigned coverage only after
52b and the iteration regression run; retain external domain dependencies.

## 5. Design outputs per slice

Exact native declaration and Python mapping; touched files and CMake test
registration; input/ownership/error table; reusable helpers and deviations;
fixed fixtures plus independent identities; comparison budgets; benchmark
impact; commands and durable evidence paths. The phase design refines the
requirements and cannot silently discard a residual capability.
