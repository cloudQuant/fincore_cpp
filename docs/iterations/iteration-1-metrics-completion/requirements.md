# Iteration 1 — Metrics Completion: Requirements

**Phases:** 38 close-out, 39, 40–52, 52b. **Status:** Revised plan; NOT_RUN.
**Oracle:** `4b8f31b1b59707480d5db9603704d83bbf7e6946`, after Phase 39.
The [shared delivery contract](../delivery-contract.md) applies.

## 1. Scope and entry

Close the incomplete Phase 38 acceptance and establish a reproducible
baseline before extending native metric capabilities. Phase 38 code is
already committed in `e2038e0`; implement its missing checks and acceptance
under the original `2354bb5` contract, rather than pretending it is accepted
or attempting to commit the same implementation again. Preserve its exact
signature and exclusions. A push or another worktree is not an entry gate.

Phase 39 must capture the full capability ledger, both oracle revisions,
environment identity, before/after drift and benchmark baseline. No new
metric phase begins with unclassified drift in its dependencies.

## 2. Functional scope

| Phase | Pinned Python route | Required behavior / boundary |
|---|---|---|
| 38 | `metrics.ratios.adjusted_sharpe_ratio` | Complete raw/clean count, dampening, n=19/20/49/50 clamp, NaN/inf, selector and ownership matrix from the existing phase design; fresh acceptance. |
| 39 | Baseline and coverage | Manifest, reproducible oracle generators, old/new comparison, capability/test mapping, benchmark protocol and declared platform matrix. Do not blindly regenerate expected values. |
| 40 | `metrics.ratios.mar_ratio` | Arithmetic mean × annualization divided by absolute max drawdown; not Calmar's CAGR numerator. Lock raw/clean count and zero-drawdown result classes. |
| 41 | `metrics.ratios.conditional_sharpe_ratio` | Select left-tail returns at or below the percentile cutoff. Freeze interpolation, equality, NaN and small-tail behavior. |
| 42 | `metrics.ratios.deflated_sharpe_ratio` | Trial-count/variance contract; normal CDF/inverse with an explicit special-function error budget. |
| 43 | `metrics.ratios.burke_ratio` | Freeze actual drawdown denominator and degenerate cases before choosing reusable helpers. |
| 44 | `metrics.ratios.cal_treynor_ratio` (`treynor_ratio` alias) | Canonical implementation with beta/benchmark alignment and risk-free conventions; alias mapped, not duplicated. |
| 45 | `metrics.ratios.m_squared` | Benchmark volatility/risk-free/annualization conventions; no invented `modigliani_ratio` route. |
| 46 | `metrics.ratios.sterling_ratio` | Oracle-defined drawdown aggregation and windows. |
| 47 | `metrics.ratios.up_capture_return`, `down_capture_return` | Return slices distinct from existing capture ratios; no `up_down_capture_return` exists at this pin. |
| 48 | `metrics.stats.hurst_exponent` | Lag/window enumeration, regression/reduction and short/constant cases characterized from source. |
| 49 | `metrics.stats.stutzer_index` | Bounded scalar optimization of exponential-tilt objective, signed square-root index; not an LPM1 formula. Freeze bounds, convergence, failures and numerical budget before porting. |
| 50 | `metrics.drawdown` table/recovery capabilities | Enumerate table/underwater/recovery and rank APIs; preserve row ordering, dates, unrecovered episodes and numeric classes. Reuse accepted drawdown kernels. |
| 51 | `metrics.timing.extract_interesting_date_ranges` | Historical names and exact date boundaries from pinned static data; empty windows and input calendar contract. |
| 52 | `metrics.perf_stats` bootstrap surface | Explicitly map `perf_stats_bootstrap`, `calc_bootstrap`, `calc_distribution_stats` and existing native equivalents. Fixed-resample kernel parity and native RNG contract are separate. |
| 52b | Residual metric ledger closure | Map/port weekly/monthly consecutive metrics, `r_cubed_turtle`, `roll_annual_volatility`, `roll_sortino_ratio`, aligned alpha/beta rolling variants, and any further Phase 39 gaps in bounded slices. Map correlation aliases to existing equivalents where semantics match. |

Phase 52b's detailed slices are frozen from the ledger before implementation;
each has its own tests and source owner. It cannot be closed with a future
promise. GPD/EVT and beta-fragility metric routes are owned by Iteration 3
Phases 63–64 and remain visible cross-iteration dependencies in domain coverage.

## 3. Common behavior and numerical constraints

Preserve scalar NUL rejection before DataFrame access, structural validation,
NaN/inf classifications, sample counts, order and input immutability. Date-
aware or binary operations explicitly declare alignment and calendar behavior;
the scalar Phase 38 prohibition on alignment is not copied indiscriminately.
Return tables/series as owning types, not scalar-shaped APIs or dangling views.

Existing exact checks stay. Special functions (42), regression (48), scalar
optimization (49) and stochastic composition (52) use their relevant classes
in the shared contract. Same-seed NumPy/C++ sample-stream equality is not
assumed. Phase 49 selects its bounded minimizer before implementation and
need not wait for Phase 60's multivariate/constrained solver decision.

## 4. Exit and exclusions

All assigned phase rows accepted; all Phase 39 drift resolved; actual public
headers/test registrations and oracle cases linked in the ledger; candidate
full CTest and sanitizer evidence retained. Iteration 1 closes its assigned
metric slice. Complete `metrics` domain parity additionally waits for the
mapped Iteration 3 routes and every residual gap; do not claim it merely
because 40–52 finished. PyMC functionality remains an explicit non-port;
existing Bayesian cone acceptance is preserved. Rendering and Python bindings
are outside this iteration.
