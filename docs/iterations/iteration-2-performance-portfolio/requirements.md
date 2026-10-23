# Iteration 2 — Performance (GIPS) and Portfolio Completion: Requirements Specification

**Iteration ID:** iteration-2-performance-portfolio
**Phase range:** Phases 53–59
**Source of truth:** fincore Python `dev@4b8f31b`: `fincore/performance/`, `fincore/portfolio/`
**Status:** Draft

## 1. Purpose and Scope

Port the GIPS-flavored performance measurement domain (time-weighted and
money-weighted returns, cashflow semantics, statistical inference) and
complete the portfolio domain (capacity and liquidity), which currently has
positions/transactions/round-trips but not capacity/liquidity. All items are
closed-form or tolerance-free numerics; this is the lowest-risk functional
extension after Iteration 1.

## 2. Entry Conditions

- Iteration 1 accepted (metrics kernel complete; oracle pinned at `4b8f31b`
  or re-pinned later per drift policy).
- Existing portfolio leaves (`positions`, `transactions`, `round_trips`,
  `slippage`) remain green.

## 3. Functional Requirements

### 3.1 Performance phases

| Phase | Public API | Python source | Requirement summary |
|---|---|---|---|
| 53 | `twr` | `fincore/performance/returns.py` | Time-weighted return with sub-period geometric chaining; sub-period boundaries defined by cashflow events. Lock the chaining identity and the boundary-day convention from oracle tests. |
| 54 | `mwr` | `fincore/performance/returns.py` | Money-weighted return (IRR-based); same root-finding entry point as Phase 55; degenerate-cashflow result classes (no flows, single flow, all-negative) locked to oracle. |
| 55 | `xirr` | `fincore/performance/returns.py` | IRR on irregular dates. Requires a root finder — see design D1. Result classes: no-bracket, multiple-sign-change inputs, near-zero denominators. |
| 56 | cashflow-adjusted returns | `fincore/performance/cashflows.py` | `cashflow_adjusted_returns` and the cashflow-semantics helpers the oracle exposes; flow-sign convention and valuation-date semantics transcribed exactly. |
| 57 | `sharpe_confidence_interval` | `fincore/performance/inference.py` | CI construction; reuse the Iteration 1 private normal-quantile helper's validated patterns; interval bounds at hex-float parity. |
| 57b | GIPS disclosures surface | `fincore/performance/` (disclosures module) | Value-type disclosure records (text + applicability flags) computed from the same inputs; no rendering. Frozen at whatever the oracle exposes — enumerate at design time. |

### 3.2 Portfolio phases

| Phase | Public API | Python source | Requirement summary |
|---|---|---|---|
| 58 | capacity module | `fincore/portfolio/capacity.py` | Capacity sweep (max position sizes at price impact bounds), max-commission/max-slippage style entry points; reuse Iteration 1/Phase 6b slippage primitives where signatures align. |
| 59 | liquidity module | `fincore/portfolio/liquidity.py` | Liquidity-bounded position/value metrics; ADV-fraction semantics transcribed from oracle; empty/degenerate-volume classes. |

### 3.3 Per-phase standard functional requirements

Same standing constraints as Iteration 1 §3.3 (exact signatures, NUL
rejection, structural-validation precedence, NaN/±inf classes, input-order and
scalar-ownership preservation), plus:

- Portfolio phases may take **two-column** inputs (price/volume, value/volume)
  with multi-column validation parity — mirror the oracle's column-name
  contracts exactly.

## 4. Non-Functional Requirements

- Hex-float exact parity for closed-form paths (twr, cashflows, inference).
- Root-finder outputs (mwr, xirr): parity contract defined in the design doc —
  default is bit-exact when the oracle's solver is deterministic on the fixed
  fixture, otherwise the roadmap §2 tolerance policy applies and must be
  declared.
- Standard per-phase gates and evidence conventions as Iteration 1 §4.

## 5. Exit Criteria

- Phases 53–59 accepted through the standard SDD loop.
- `fincore.performance` public surface at parity (enumerate at the final
  acceptance record; anything the oracle adds after `4b8f31b` is handled by
  the drift policy).
- `fincore.portfolio` public surface at parity.
- Iteration-head clean build + full CTest + ASan/UBSan evidence; commits
  pushed.

## 6. Out of Scope

- Report rendering of any performance/portfolio model (Iteration 6).
- `runtime` orchestration of these operations (Iteration 6).
- Any risk-model, optimization, viz, or factor API.
