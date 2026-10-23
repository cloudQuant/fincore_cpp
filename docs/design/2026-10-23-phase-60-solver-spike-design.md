# Phase 60 Solver Spike Design — Decision: Hand-Ported Deterministic Kernels

## Decision

Adopt **Option B**: hand-ported deterministic numerical kernels in
`fincore::detail`, with no new dependency.

- Unconstrained MLE (GARCH/EVT): Nelder-Mead simplex with standard
  reflection/expansion/contraction/shrink coefficients, a 1.05 initial simplex
  scale, and convergence on both function spread and simplex span.
- Convex QP on the probability simplex: projected gradient with backtracking
  and an exact box-constrained simplex projection (bisection on the Lagrange
  multiplier, 200 iterations).
- Risk parity: the existing ERC fixed-point/Newton formulation will be
  implemented in Phase 71 on these primitives.

## Why not vendor (Option A)

- No OSQP checkout or package exists locally; the build is network-free and
  the roadmap requires local checkouts for any vendored dependency.
- Determinism and parity control: the tolerance-parity contract requires
  bit-identical C++ reruns; hand-written kernels make iteration counts and
  stopping rules explicit.
- Build friction: a vendored solver would need to build under the clean,
  sanitizer, ImPlot-optional, and invalid-ImGui configurations. The measured
  spike shows the internal kernels need only one source and one test
  translation unit.

## Evidence

- Clean Release CTest: **610/610** with the new kernels.
- LLVM 22 ASan+UBSan: **610/610**.
- Determinism: Nelder-Mead on a quadratic returns identical parameters and
  iteration counts across two runs (test assertion).
- Convergence: quadratic `[1, -0.5]`; Rosenbrock `[1, 1]` within 1e-4;
  equal-covariance min-variance `[0.5, 0.5]`; diagonal-variance QP
  inverse-variance `[1/7, 2/7, 4/7]`.
- Simplex projection: `[0.6, 0, 0.6] → [0.5, 0, 0.5]` and bounded
  `[-1, 2, 2] → [0, 0.5, 0.5]`.

## Boundary

- These are internal `detail` APIs, not public leaves; public phases (61–71)
  own their signatures and parity contracts.
- Tolerance policy (roadmap §2) applies to downstream solver outputs; the
  spike itself is validated by analytic fixtures rather than a Python oracle,
  since it is a numerical kernel rather than a financial metric.
- A later roadmap amendment may revisit Option A if a maintainable local
  solver becomes available; the decision is binding until then.
