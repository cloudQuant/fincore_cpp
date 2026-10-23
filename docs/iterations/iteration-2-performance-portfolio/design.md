# Iteration 2 — Performance (GIPS) and Portfolio Completion: Design Document

**Iteration ID:** iteration-2-performance-portfolio
**Status:** Draft
**Depends on:** Iteration 1 (private normal-quantile helper pattern, drawdown
table value type, metrics kernel).

## 1. Architecture Overview

Two new leaf families:

```
include/fincore/performance/{returns,cashflows,inference,disclosures}.hpp
src/performance/*.cpp + tests/unit/performance_*_test.cpp
include/fincore/portfolio/{capacity,liquidity}.hpp
src/portfolio/*.cpp + tests/unit/portfolio_{capacity,liquidity}_test.cpp
```

New CMake source list entries only; no new targets or dependencies. The
`performance` directory is new to the C++ tree.

## 2. Key Design Decisions

### D1 — One private root finder, shared by mwr/xirr
Both `mwr` and `xirr` solve an IRR-type equation. Decision: a single private
solver (Brent's method, transcribed with the oracle's bracketing and
tolerance semantics) in `src/performance/returns.cpp`. The design doc for
Phase 55 must first record what the Python side actually calls (typically
`numpy_financial.irr`-style or a scipy brentq). **If the oracle pins scipy's
brentq**, the tolerance-parity policy (roadmap §2) applies: fixed fixture,
declared tolerance, deterministic C++ result; the design doc records the
oracle's iteration count/convergence criterion as informational parity.
Root-finder internals never become public API.

### D2 — Cashflow event model
Cashflows enter as a DataFrame (date index + amount column, double), not as
parallel vectors. Rationale: keeps the boundary DataFrame-native per the
foundation design; the oracle's Series-with-index maps 1:1. Validation
(amount column type, index monotonicity, date-vs-returns alignment) follows
the established precedence rules.

### D3 — Disclosure records as value types
GIPS disclosure output is a frozen vector of `{code, text, applicable}` value
records computed once. No locale, no formatting, no I/O. Rendering belongs to
Iteration 6.

### D4 — Capacity/liquidity reuse map
Before implementing Phase 58/59, enumerate the existing portfolio leaves for
reusable primitives (slippage penalty from Phase 6b, turnover from 5b). Any
divergence between the oracle's formula and an existing native primitive is a
finding, resolved in the phase design — either the native primitive is reused
(and its parity re-attested), or the new leaf carries a private helper with a
recorded reason.

## 3. Component Plan

| Phase | Files | New public surface |
|---|---|---|
| 53 | performance/returns.{hpp,cpp} + TU | `twr` |
| 54 | same | `mwr` |
| 55 | same | `xirr` (+ private Brent solver) |
| 56 | performance/cashflows.{hpp,cpp} + TU | `cashflow_adjusted_returns` (+ enumerated helpers) |
| 57 | performance/inference.{hpp,cpp} + TU | `sharpe_confidence_interval` |
| 57b | performance/disclosures.{hpp,cpp} + TU | disclosure record types |
| 58 | portfolio/capacity.{hpp,cpp} + TU | capacity sweep + max-commission/slippage entries |
| 59 | portfolio/liquidity.{hpp,cpp} + TU | liquidity entries |

## 4. Risks and Mitigations

| Risk | Mitigation |
|---|---|
| Root-finder parity mismatch if oracle uses scipy | Declared tolerance policy in Phase 55 design; fixture frozen; convergence-criterion parity documented. |
| Cashflow sign/valuation-date conventions differ from intuition | Transcribe from oracle tests verbatim; add a dedicated convention fixture with named cases. |
| Oracle surface drift (performance module is younger) | Enumerate the public surface at Phase 53 design time and re-check at 57b; drift policy applies. |

## 5. Sequencing

53 → 54 → 55 share the cashflow model and solver; strict order. 56 after 53
(chaining semantics). 57, 57b independent. 58 → 59 (liquidity reuses capacity
sweep shapes). All after Iteration 1 acceptance.
