# Iteration 1 — Metrics Completion: Design Document

**Iteration ID:** iteration-1-metrics-completion
**Status:** Draft (per-phase design docs supersede this at implementation time)
**Depends on:** Phase 37 head (accepted); nothing upstream.

## 1. Architecture Overview

No new targets, dependencies, adapters, or facades. All work lands in the
existing per-module leaf pattern:

```
include/fincore/metrics/<leaf>.hpp   (one public declaration per phase)
src/metrics/<leaf>.cpp               (private helpers live beside their consumer)
tests/unit/<leaf>_test.cpp           (registered TU, exact-signature static_asserts)
tests/unit/<leaf>_extended_test.cpp  (boundary hardening)
```

New leaves required: none for ratios/stats (they exist); the drawdown table
machinery (Phase 50) adds a **value type** `drawdown_table` to
`metrics/drawdown.hpp` plus assembly in `src/metrics/drawdown.cpp`. Phase 51
adds static period data in `src/metrics/timing.cpp`. Phase 52 composes
existing `simulation/bootstrap` primitives with `perf_stats`.

## 2. Key Design Decisions

### D1 — Reuse-before-implement ordering
Each ratio phase must first enumerate existing native primitives it can call
(`max_drawdown`, `value_at_risk`, `beta`, `sharpe_ratio`, `annualization_factor`,
bootstrap/CI helpers) and reuse them. Rationale: parity errors compound when
each phase re-derives shared numerics; the Python oracle shares helpers too.
Constraint: reuse does **not** extend to public `skewness`/`kurtosis` where the
oracle uses different filtering — the Phase 38 private-moment-helper precedent
applies (private helper beside the consumer, public stats untouched).

### D2 — Closed-form special functions stay private
`deflated_sharpe_ratio` (Phase 42) needs the normal CDF and inverse CDF.
Decision: private `detail::` implementation in `ratios.cpp` (or a private
shared header inside `src/metrics/` if a second consumer appears), validated
against the Python oracle's `scipy.stats.norm` outputs at hex-float
tolerance via test goldens. No public special-function API, no vendored
math library.

### D3 — Drawdown table is a value type, not a DataFrame
Phase 50's top-N table is returned as a `std::vector<drawdown_row>` (or small
struct holding rows + underwater series view), not a `StdDataFrame`. Rationale:
consumers (reports, viz) need typed field access; the oracle's DataFrame shape
is an implementation detail. Field names and semantics are locked 1:1 to the
Python column names so Iteration 6's report layer can map them directly.

### D4 — Crisis periods as compile-time static data
`extract_interesting_date_ranges` (Phase 51) ports `_historical_periods.py`
to a `static constexpr`-friendly table in `src/metrics/timing.cpp` with the
oracle's exact name strings and date boundaries. Boundary semantics
(half-open vs closed) are transcribed from oracle tests, not inferred.

### D5 — Oracle re-pin as a first-class phase (Phase 39)
The re-pin is not a chore: it produces a committed design doc listing
old/new SHAs, a full-suite re-run, and a classified drift log. This creates
the audit trail that makes "no drift" a measured claim.

## 3. Component Plan

| Phase | Files touched | New public surface |
|---|---|---|
| 38 | ratios.hpp/.cpp + 2 TUs (already in worktree) | `adjusted_sharpe_ratio` |
| 39 | docs only + regenerated goldens in existing tests | none |
| 40–47 | ratios.hpp/.cpp + 2 TUs | one function per phase |
| 48–49 | stats.hpp/.cpp + 2 TUs | `hurst_exponent`, `stutzer_index` |
| 50 | drawdown.hpp/.cpp + 2 TUs | `drawdown_table` value type + `top_drawdowns`-style entry |
| 51 | timing.hpp/.cpp + 2 TUs | `extract_interesting_date_ranges` |
| 52 | perf_stats.hpp/.cpp + 2 TUs | bootstrap variant(s) of `perf_stats` |

## 4. Risks and Mitigations

| Risk | Mitigation |
|---|---|
| Phase 39 drift beyond factor_analysis (contrary to expectation) | Drift log classifies each item; any metrics drift blocks Phase 40 until re-baselined. |
| `hurst_exponent` window policy ambiguity | Design doc must quote oracle source lines and lock window/lag enumeration; no invented defaults. |
| Drawdown table grows beyond one phase's scope | Freeze at table assembly + underwater series; recovery-period enrichment only if already in the oracle's table. |
| Unpushed-commit loss mid-iteration | Entry condition: push completes before Phase 40 starts. |

## 5. Sequencing

38 → 39 strictly first. Phases 40–47 are independent (any order within ratios;
keep numeric order for ledger consistency). 48–49 independent. 50 may start
after 39. 52 requires Phases 20–22 (already accepted) only.
