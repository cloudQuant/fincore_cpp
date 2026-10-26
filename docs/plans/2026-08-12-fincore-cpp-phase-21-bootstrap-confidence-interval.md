# fincore C++ Phase 21 Bootstrap Confidence-Interval Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans` to
> implement this plan task-by-task.

**Goal:** Add a native, finite percentile bootstrap confidence interval that
reuses the accepted DataFrame-native bootstrap distribution exactly once.

**Architecture:** The existing bootstrap leaf gains a small two-double result
and a confidence-interval function.  It validates alpha, delegates all
sampling/statistic work to `bootstrap_distribution`, validates that generated
statistics are finite, and performs checked NumPy-linear percentile
interpolation.  Because Phase 20 already exports `bootstrap.hpp`, a public
simulation-facade consumer is captured in the leaf task before the new symbol
exists; acceptance changes documentation only.

**Tech Stack:** C++23, local cloudQuant/DataFrame, CMake, GoogleTest, source
oracle through `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`,
Homebrew LLVM 22 ASan+UBSan, and optional local Dear ImGui/ImPlot compile/link
verification.

**Baseline:** Phase 20 completes at
`b059e44d046f0f01f081a319ca1854ba5d77c85f` with 324 headless CTest cases.
That commit is a post-trace test/documentation correction; it must remain in
the baseline and must not be amended or omitted from later evidence.

---

## Global constraints

- Extend only the accepted bootstrap leaf.  Do not modify generic DataFrame or
  numeric helpers, Phase 20 distribution behavior/tests other than adding the
  new CI tests, simulation parameters/GBM/correlation/stress sources, metrics,
  attribution, CMake dependency policy, or optional ImPlot source.
- Public API must be exactly:

  ```cpp
  struct bootstrap_confidence_interval_result {
      double lower;
      double upper;
  };

  bootstrap_confidence_interval_result bootstrap_confidence_interval(
      const dataframe& returns,
      std::size_t resample_count = 10000U,
      double alpha = 0.05,
      bootstrap_statistic statistic = bootstrap_statistic::mean,
      std::optional<std::uint64_t> seed = std::nullopt,
      std::string_view column = returns_column);
  ```

- `alpha` must be finite and satisfy `0 < alpha < 1`; reject every invalid
  alpha with `input_error` before delegate work.  The only supported method is
  source `percentile`; do not expose a method enum/string or callable hook.
- Call public `bootstrap_distribution` exactly once after alpha validation,
  with all distribution arguments unchanged.  Do not reimplement sampling,
  statistics, selected-column validation, NaN filtering, RNG, or output
  materialization.  This function inherits Phase 20's native seeded
  repeatability/no-NumPy-RNG-parity boundary.
- For a nonempty *finite* generated distribution, compute source-compatible
  linear percentile cutoffs `alpha / 2` and `1 - alpha / 2`.  Sort copied
  values; use position `p * (n - 1)` and adjacent-order-statistic linear
  interpolation.  Validate every relevant subtraction/product/sum/result and
  throw `input_error` on nonfinite overflow.  Do not change global
  `detail::linear_percentile`.
- Generated `NaN`, `+inf`, or `-inf` statistic values are rejected with
  `input_error` before sorting.  This deliberately differs from Python's
  warning-producing/nonfinite `np.percentile` behavior and must be documented
  and tested.  A zero-count distribution is likewise an `input_error` at the
  interval boundary (although valid for Phase 20 distribution).
- Keep the result a two-double value-owning struct.  Do not return a DataFrame,
  map, tuple alias, summary table, or report object.
- Phase 20 already has `simulation.hpp -> bootstrap.hpp`; do not add or remove
  a facade include.  Capture the true umbrella-only RED while the function is
  absent in Task 92, and prove the accepted automatic facade export afterward.
- Exclude Python `method` variants, callable statistics, `bootstrap_summary`,
  full NumPy nonfinite percentile/RNG parity, Monte Carlo, antithetic/LHS,
  SimResult, providers, reports, packaging, and GUI runtime.
- All Python commands use
  `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`; never bare
  `python` or `python3`.

### Task 91: Record the Phase 21 design and plan

**Files:**

- Create: `docs/design/2026-08-12-phase-21-bootstrap-confidence-interval-design.md`
- Create: `docs/plans/2026-08-12-fincore-cpp-phase-21-bootstrap-confidence-interval.md`

1. Record the API, one-delegate rule, finite linear percentile math, explicit
   Python nonfinite/empty difference, automatic facade-export rationale,
   oracle, exclusions, and TDD/acceptance workflow.
2. Commit only the documents:

   ```sh
   git add docs/design/2026-08-12-phase-21-bootstrap-confidence-interval-design.md \
           docs/plans/2026-08-12-fincore-cpp-phase-21-bootstrap-confidence-interval.md
   git commit -m "docs: plan phase 21 bootstrap confidence interval"
   ```

### Task 92: Add the bootstrap confidence-interval leaf and public facade proof

**Files:**

- Modify: `include/fincore/simulation/bootstrap.hpp`
- Modify: `src/simulation/bootstrap.cpp`
- Create: `tests/unit/bootstrap_confidence_interval_test.cpp`
- Create: `tests/unit/simulation_bootstrap_confidence_interval_umbrella_test.cpp`
- Modify: `tests/CMakeLists.txt`

1. **Create ignored SDD workspace, source oracle, and true public RED.**
   Create
   `.superpowers/sdd/2026-08-12-fincore-cpp-phase-21-bootstrap-confidence-interval/`
   with task brief/ledger/ignored `task-92-report.md`.  In
   `/Users/yunjinqi/Documents/new_projects/fincore`, record:

   ```sh
   /Users/yunjinqi/opt/anaconda3/bin/conda run -n base python -c '
   import numpy as np
   from fincore.simulation.bootstrap import bootstrap_ci
   values = np.array([0.01, -0.01, 0.02, 0.0])
   print("mean", bootstrap_ci(values, n_samples=5, alpha=0.2,
                              statistic="mean", seed=42))
   print("constant_mean", bootstrap_ci(np.array([0.01, 0.01]), 4,
                                        statistic="mean", seed=42))
   print("constant_std", bootstrap_ci(np.array([0.01, 0.01]), 4,
                                       statistic="std", seed=42))
   print("nan_drop", bootstrap_ci(np.array([0.01, np.nan, 0.01]), 3,
                                  statistic="mean", seed=42))
   for alpha in (0.0, 1.0, float("nan"), float("inf")):
       try:
           print("alpha", repr(alpha), bootstrap_ci(values, alpha=alpha, seed=42))
       except Exception as error:
           print("alpha", repr(alpha), type(error).__name__, str(error))
   for name, sample in (("zero_count", np.array([0.01, 0.02])),
                        ("nonfinite_stat", np.array([0.01, 0.01]))):
       try:
           kwargs = {"n_samples": 0} if name == "zero_count" else {"n_samples": 4, "statistic": "sharpe"}
           print(name, bootstrap_ci(sample, seed=42, **kwargs))
       except Exception as error:
           print(name, type(error).__name__, str(error))
   '
   ```

   Record exact output and NumPy warnings.  Add/register two consumers before
   production declaration: one only includes
   `<fincore/simulation/bootstrap.hpp>`, one only includes
   `<fincore/simulation.hpp>`.  `cmake --build build --parallel 4` must fail
   only because the new result/function is absent.  Do not run stale CTest
   after this expected failure.

2. **Implement the minimal one-delegate CI.**  Add the exact result struct and
   function declaration.  In source, validate alpha then call public
   `bootstrap_distribution` exactly once.  Copy/validate its values as
   nonempty and finite, calculate checked linear percentiles, and return the
   two value fields.  Never access private DataFrame internals or duplicate
   sampler/statistic/RNG code; never modify `simulation.hpp`.

3. **Write GREEN behavioral tests.**  Prove exact function type/defaults and
   public leaf/umbrella inclusion; deterministic Python-compatible all-equal
   mean/std and NaN-drop mean intervals; manual finite cutoff endpoint math
   derived from the single public distribution for a nonconstant native seed;
   alpha boundary/NaN/infinity rejection; zero count; generated NaN/+infinity
   rejection; unknown enum/selector/type/value/overflow propagation; no raw
   input-index influence; result value ownership; same-seed equality and no
   NumPy stream claim.  The test's independent interpolation oracle may
   implement only the published finite percentile formula; it may not reproduce
   the bootstrap sampler or private helper.

4. **Run GREEN, scoped review, and commit exact Task 92 scope.**

   ```sh
   cmake --build build --parallel 4
   ctest --test-dir build -R 'BootstrapConfidenceInterval|SimulationBootstrapConfidenceIntervalUmbrella|BootstrapDistribution|GbmPaths|SimulationParameters|CorrelationBreakdown|StressScenarios|SimulationUmbrella|Dataframe' --output-on-failure
   ctest --test-dir build --output-on-failure
   git diff --check
   ```

   A fresh independent reviewer must verify the exact API, alpha ordering,
   one delegate call, finite linear interpolation, deliberate nonfinite/empty
   divergence, generated facade export, source oracle, tests, no duplicate RNG
   logic, and exact five-file scope.  Record both Spec and Task-Quality
   verdicts.  Fix all Critical, Important, or Minor findings in a new scoped
   commit and re-review before proceeding.

   ```sh
   git add include/fincore/simulation/bootstrap.hpp src/simulation/bootstrap.cpp \
           tests/CMakeLists.txt tests/unit/bootstrap_confidence_interval_test.cpp \
           tests/unit/simulation_bootstrap_confidence_interval_umbrella_test.cpp
   git commit -m "feat: add bootstrap confidence interval"
   ```

### Task 93: Accept Phase 21 evidence without redundant facade changes

**Files:**

- Modify: `README.md`
- Create: `docs/acceptance/2026-08-12-phase-21-bootstrap-confidence-interval-acceptance.md`

1. Record why `<fincore/simulation.hpp>` already exports the extended bootstrap
   leaf and why no facade diff is needed; include Task 92 SHA, exact public
   boundary, one-delegate rule, finite percentile definition, explicit Python
   nonfinite/empty divergence, source oracle, RED/GREEN, and exclusions.
2. Retain raw `tee` logs for fresh configured/full CTest, AppleClang headless,
   Homebrew LLVM ASan+UBSan, ImPlot/demo compile/link only (actual build-root
   executable checked but never launched), and invalid-ImGui wrapped diagnostic
   with whitespace-tolerant multiline scanner.  State FetchContent GoogleTest
   provenance from the actual cache; never claim it is local/no-fetch without
   proof.
3. Independently review docs/logs/gates and commit exactly the two files:

   ```sh
   git add README.md \
           docs/acceptance/2026-08-12-phase-21-bootstrap-confidence-interval-acceptance.md
   git commit -m "docs: accept phase 21 bootstrap confidence interval"
   ```

### Task 94: Add non-self-referential Phase 21 traceability

**Files:**

- Modify: `docs/acceptance/2026-08-12-phase-21-bootstrap-confidence-interval-acceptance.md`

1. Record full Task 92 implementation SHA and direct Task 93 initial
   acceptance SHA plus exact scopes.  State Task 94 is documentation-only and
   does not amend prior commits; do not state or anticipate Task 94's own SHA.
2. Run focused
   `BootstrapConfidenceInterval|SimulationBootstrapConfidenceIntervalUmbrella`
   CTest and diff checks, then commit only the acceptance document:

   ```sh
   git add docs/acceptance/2026-08-12-phase-21-bootstrap-confidence-interval-acceptance.md
   git commit -m "docs: trace phase 21 bootstrap confidence interval"
   ```

### Final Phase 21 review

1. Audit Task 92/93/94 ancestry/scopes; exact leaf and automatic facade export;
   source oracle; alpha/delegation/finite-percentile semantics; deliberate
   nonfinite/empty divergence; tests and absence of duplicate sampling/RNG;
   raw gate logs/FetchContent provenance/no-GUI claims; README/acceptance
   wording; and non-self-reference.
2. Fresh-configure AppleClang headless, rerun focused/full CTest and mandatory
   Anaconda source oracle, and publish Spec and Task-Quality verdicts.  Fix all
   Critical, Important, or Minor findings in new scoped commits and re-review;
   do not amend prior accepted commits.
