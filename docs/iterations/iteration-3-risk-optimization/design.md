# Iteration 3 — Risk and Optimization: Design

**Status:** Revised plan. [Shared contract](../delivery-contract.md) applies.

## 1. Solver decision before dependent implementation

Phase 60 is a bounded experimental delivery, not documentation alone. Its
proof may include small harness/build changes that are clearly separated
from production API. Enumerate each algorithm's objective, derivative needs,
constraints, initialization, scale and failure semantics from the pinned
source before choosing a solver.

Compare suitable pinned libraries and bounded internal implementations.
A QP solver candidate such as OSQP addresses a QP subset only; it does not
by itself cover nonlinear GARCH/max-Sharpe/risk-parity problems. Likewise
Nelder-Mead/L-BFGS without a constraint strategy is not a complete replacement
for constrained optimization. A transformed formulation must prove its
feasibility/objective equivalence and supported cases.

The decision records representative easy, ill-conditioned, infeasible and
nonconvergent fixtures; numerical error/residual budgets; time/RSS;
compiler/sanitizer integration; dependency version, transitive requirements,
license/notices and offline preparation. The invalid-ImGui gate applies only
if related wiring changes, not as a fictional solver capability proof.
If no candidate satisfies the required problem classes, dependent phases
remain `BLOCKED` with a bounded alternative decision, not a relaxed pass.

## 2. Kernels, variants and results

Keep solver details private (for example `src/numerics/`); public risk and
optimization headers expose domain configs/results. Share mathematical
primitives only where the variants actually share equations. EGARCH's
log-variance recursion must not be forced into a GARCH variance recursion
merely to satisfy a single-kernel rule. Common validation/objective machinery
may be shared while distinct recursions retain independent tests.

Results own parameter/weight/forecast arrays and labels. Include convergence
and diagnostic fields needed by the mapped Python result, with precise
mapping of internal solver errors. Freeze covariance estimation/ddof,
annualization, risk-free units, constraints and warm-start semantics before
implementation. Never silently normalize or clip a failed solver result
into an apparently feasible portfolio.

## 3. Walk-forward and model provenance

Walk-forward fixtures prove each fit sees only permitted training dates;
forecast t must not consume t's realized return when the contract excludes
it. Enumerate estimators, windows, refit schedule, warmup and missing-date
handling. Lock discrete schedule/exception classes exactly and apply
numerical budgets to predictions/statistics.

Risk specs/reports are immutable values. Phase 67 determines the actual
`inputs_digest` source/serialization recipe rather than assuming CSV → SHA256.
Define label order, encoding, timestamps, floating/nonfinite values and
metadata scope. Require cross-language digest equality only for a proven
common canonical byte schema; otherwise version a native digest and preserve
explicit source provenance without making a false equality claim.

Use Phase 88a's common report section/artifact contracts for projection;
keep the domain `RiskValidationReport` separate from rendering. Phase 91
later renders the already computed result.

## 4. Execution graph

60 → solver-dependent 61/63/68. 61 → 62; required estimators and 65 → 66;
61–66 plus 88a → 67. 64 and independent 65 calculations can proceed after
39 without waiting for GARCH. 68 → 69/70/71 according to their actual shared
helpers, not an invented mandatory sequential chain. Phase 69 exposes the
frontier result required by Iteration 4's frontier chart.
