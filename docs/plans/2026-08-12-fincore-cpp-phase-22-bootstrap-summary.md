# fincore C++ Phase 22 Bootstrap Summary Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans` to
> implement this plan task-by-task.

**Goal:** Add a finite, value-owning native bootstrap summary that composes the
accepted distribution and confidence-interval semantics for mean, sample
standard deviation, Sharpe ratio, and Sortino ratio.

**Architecture:** Extend the already exported bootstrap leaf with two small
result structs and one summary function.  It validates the summary boundary,
computes finite observed statistics from the accepted retained source values,
calls public `bootstrap_distribution` exactly once for each fixed statistic,
and reuses Phase 21's private finite linear-percentile routine on the same
distribution to calculate standard errors and intervals.  It adds no sampler,
RNG, generic DataFrame helper, map/dictionary, or facade include.

**Tech Stack:** C++23, local cloudQuant/DataFrame, CMake, GoogleTest, source
oracle through `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`,
Homebrew LLVM 22 ASan+UBSan, and optional local Dear ImGui/ImPlot compile/link
verification.

**Baseline:** Phase 21 completes at
`6dbf11ecfe2e9c0abc2d2a8f691006d7f2ca4e20` with 332 headless CTest cases.
It includes the accepted Phase 20 correction and Phase 21 non-self-referential
trace.  It must remain an ancestor; do not amend, squash, or omit it.

---

## Global constraints

- Extend only `bootstrap.hpp`/`bootstrap.cpp` and add summary-specific tests.
  Do not modify generic DataFrame/numeric helpers, Phase 20 distribution
  behavior, Phase 21 CI public behavior/tests, other simulation leaves,
  optional ImPlot source, dependency policy, bindings, reports, or packaging.
- Public API must be exactly:

  ```cpp
  struct bootstrap_summary_statistic {
      double value;
      double standard_error;
      bootstrap_confidence_interval_result confidence_interval;
  };

  struct bootstrap_summary_result {
      bootstrap_summary_statistic mean;
      bootstrap_summary_statistic sample_standard_deviation;
      bootstrap_summary_statistic sharpe_ratio;
      bootstrap_summary_statistic sortino_ratio;
  };

  bootstrap_summary_result bootstrap_summary(
      const dataframe& returns,
      std::size_t resample_count = 10000U,
      double alpha = 0.05,
      std::optional<std::uint64_t> seed = std::nullopt,
      std::string_view column = returns_column);
  ```

- Require finite `alpha` and `0 < alpha < 1`, and require
  `resample_count >= 2`.  Validate alpha before count/delegate work.  All
  violations throw `fincore::input_error`; no partial result is returned.
- Follow the accepted bootstrap source-input policy for every observed field:
  NUL-free nonempty genuine-double selector; drop only NaNs; reject signed
  infinities; require at least one retained value.  Use the same retained finite
  values for `value` fields and distributions.  Copy a bounded selector into an
  owned `std::string` before any DataFrame C-string boundary.
- Calculate exactly four entries in this source order:
  `mean`, `sample_standard_deviation`, `sharpe_ratio`, and `sortino_ratio`.
  For each, call public `bootstrap_distribution` exactly once, preserving
  `returns`, `resample_count`, `seed`, and selected column and changing only the
  enum.  Do not reimplement selection, sampling, MT19937-64, named statistics,
  NaN handling, or DataFrame materialization.  Do not call public CI because it
  would resample again.
- Reuse a private Phase 21 finite distribution/linear-percentile helper for
  both CI and summary.  For each summary entry, require a finite, nonempty
  generated distribution, calculate `standard_error` as sample standard
  deviation (`ddof=1`), and calculate cutoffs at `alpha/2` and `1-alpha/2`
  from that same distribution.  Preserve current CI's one-delegate behavior.
- Require every observed statistic, generated statistic, standard error, and
  endpoint to be finite.  A constant positive return sequence that yields a
  nonfinite Sharpe/Sortino result therefore throws `input_error`; this is a
  deliberate native safe boundary.  It differs from Python's dictionary of
  `NaN`/infinity values and must be recorded/tested rather than hidden.
- Native nonconstant seeded output must not be claimed NumPy-RNG equal.  A
  fixed seed is passed independently to each of the four public distributions,
  which mirrors the source helper's per-statistic fresh-generator boundary.
- `simulation.hpp` already includes `bootstrap.hpp`; do not add/remove a
  facade include.  Capture leaf-only and true facade-only RED consumers while
  the new declarations are absent, then prove automatic export in GREEN.
- Exclude callable statistics, a string-keyed map/DataFrame summary table,
  Python raw-NaN `value` behavior, endpoint alpha support, nonfinite summary
  output, full NumPy RNG parity, Monte Carlo, antithetic/LHS, reports,
  packaging, and GUI runtime.
- All Python commands use
  `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`; never bare
  `python` or `python3`.

### Task 95: Record the Phase 22 design and plan

**Files:**

- Create: `docs/design/2026-08-12-phase-22-bootstrap-summary-design.md`
- Create: `docs/plans/2026-08-12-fincore-cpp-phase-22-bootstrap-summary.md`

1. Record fixed structs/API, four-delegation rule, retained-input policy,
   shared finite percentile behavior, Python safety divergences, facade
   rationale, source oracle, exclusions, and TDD/acceptance workflow.
2. Commit only the documents:

   ```sh
   git add docs/design/2026-08-12-phase-22-bootstrap-summary-design.md \
           docs/plans/2026-08-12-fincore-cpp-phase-22-bootstrap-summary.md
   git commit -m "docs: plan phase 22 bootstrap summary"
   ```

### Task 96: Add the native bootstrap-summary leaf and facade proof

**Files:**

- Modify: `include/fincore/simulation/bootstrap.hpp`
- Modify: `src/simulation/bootstrap.cpp`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/unit/bootstrap_summary_test.cpp`
- Create: `tests/unit/simulation_bootstrap_summary_umbrella_test.cpp`

1. **Create ignored SDD workspace, source oracle, and true public RED.**
   Create `.superpowers/sdd/2026-08-12-fincore-cpp-phase-22-bootstrap-summary/`
   with a task brief, ledger, and ignored `task-96-report.md`.  In
   `/Users/yunjinqi/Documents/new_projects/fincore`, use the mandated Anaconda
   environment to record:

   ```sh
   /Users/yunjinqi/opt/anaconda3/bin/conda run -n base python -c '
   import numpy as np
   from fincore.simulation.bootstrap import bootstrap_summary
   values = np.array([0.01, -0.01, 0.02, 0.0])
   print("basic", bootstrap_summary(values, n_samples=5, alpha=0.2, seed=42))
   for label, sample in (("nan", np.array([0.01, np.nan, 0.01])),
                         ("constant", np.array([0.01, 0.01])),
                         ("singleton", np.array([0.01]))):
       try:
           print(label, bootstrap_summary(sample, n_samples=2, alpha=0.2, seed=42))
       except Exception as error:
           print(label, type(error).__name__, str(error))
   for alpha in (0.0, 1.0, float("nan"), float("inf")):
       try:
           print("alpha", repr(alpha), bootstrap_summary(values, n_samples=2,
                                                          alpha=alpha, seed=42))
       except Exception as error:
           print("alpha", repr(alpha), type(error).__name__, str(error))
   for count in (0, 1):
       try:
           print("count", count, bootstrap_summary(values, n_samples=count,
                                                     alpha=0.2, seed=42))
       except Exception as error:
           print("count", count, type(error).__name__, str(error))
   '
   ```

   Record exact output and warnings.  Add/register leaf-only and
   `<fincore/simulation.hpp>`-only consumers before the declarations.  A build
   must fail only because the types/function are missing; do not run stale
   CTest after the expected failure.

2. **Implement the minimal four-delegation summary.**  Add the exact structs
   and declaration.  Reuse/refactor only private bootstrap helpers so CI still
   delegates once.  Validate alpha then count, own the selector, use retained
   source values for observed values, call public distribution once per fixed
   statistic, validate finite output, calculate standard error and both
   cutoffs from the same copied distribution, and return the fixed struct.
   Never edit `simulation.hpp`.

3. **Write GREEN behavioral tests.**  Prove exact public type/defaults and
   leaf/facade inclusion; direct public-distribution parity for every field and
   all four statistics using independent sample-standard-deviation/linear-
   percentile formulas; same-seed repeatability; expected native-safe
   alpha/count/nonfinite-distribution/degenerate-ratio rejection; NaN-drop
   behavior; selector/type/value propagation; input-index independence;
   output ownership after source/value-name mutation/destruction; and no NumPy
   random-stream claim.  The test may derive a result from public distributions
   but must not copy sampler/RNG/private statistic logic.

4. **Run GREEN, scoped review, and commit exact Task 96 scope.**

   ```sh
   cmake --build build --parallel 4
   ctest --test-dir build -R 'BootstrapSummary|SimulationBootstrapSummaryUmbrella|BootstrapConfidenceInterval|BootstrapDistribution|GbmPaths|SimulationParameters|CorrelationBreakdown|StressScenarios|SimulationUmbrella|Dataframe' --output-on-failure
   ctest --test-dir build --output-on-failure
   git diff --check
   ```

   A fresh independent reviewer must verify exact API/defaults, alpha/count
   ordering, exactly four public distribution calls, shared finite CI helper,
   no duplicate sampling/RNG/statistic logic, input/result ownership, declared
   safe divergences, source oracle, test adequacy, automatic facade export, and
   exact five-file scope.  Record both Spec and Task-Quality verdicts.  Fix all
   Critical, Important, or Minor findings in a new scoped commit and re-review
   before proceeding.

   ```sh
   git add include/fincore/simulation/bootstrap.hpp src/simulation/bootstrap.cpp \
           tests/CMakeLists.txt tests/unit/bootstrap_summary_test.cpp \
           tests/unit/simulation_bootstrap_summary_umbrella_test.cpp
   git commit -m "feat: add bootstrap summary"
   ```

### Task 97: Accept Phase 22 summary evidence without redundant facade changes

**Files:**

- Modify: `README.md`
- Create: `docs/acceptance/2026-08-12-phase-22-bootstrap-summary-acceptance.md`

1. Record the Task 96 SHA, exact public contract, four-delegation/same-
   distribution rule, strict safe boundaries and Python differences, source
   oracle, RED/GREEN, automatic facade rationale, and exclusions.
2. Retain raw `tee` logs for configured/full CTest, fresh AppleClang headless,
   Homebrew LLVM ASan+UBSan, ImPlot/demo compile/link-only (check actual
   build-root executable without launching it), and invalid-ImGui wrapped
   diagnostic with a whitespace-tolerant multiline scan.  State actual
   GoogleTest FetchContent provenance; do not claim a local/no-fetch
   dependency without evidence.
3. Independently review docs/logs/gates and commit exactly the two files:

   ```sh
   git add README.md \
           docs/acceptance/2026-08-12-phase-22-bootstrap-summary-acceptance.md
   git commit -m "docs: accept phase 22 bootstrap summary"
   ```

### Task 98: Add non-self-referential Phase 22 traceability

**Files:**

- Modify: `docs/acceptance/2026-08-12-phase-22-bootstrap-summary-acceptance.md`

1. Record full Task 96 implementation SHA and direct Task 97 initial
   acceptance SHA plus exact scopes.  State Task 98 is documentation-only and
   does not amend prior commits; do not state or anticipate Task 98's own SHA.
2. Run focused `BootstrapSummary|SimulationBootstrapSummaryUmbrella` CTest and
   diff checks, then commit only the acceptance document:

   ```sh
   git add docs/acceptance/2026-08-12-phase-22-bootstrap-summary-acceptance.md
   git commit -m "docs: trace phase 22 bootstrap summary"
   ```

### Final Phase 22 review

1. Audit Task 96/97/98 ancestry/scopes; exact types/defaults; alpha/count
   ordering; four public delegates and same-distribution standard-error/CI
   math; retained-input/result ownership; safe source divergences; source
   oracle; test adequacy; absence of duplicated sampling/RNG/statistic logic;
   automatic facade export; raw gate logs/FetchContent provenance/no-GUI
   claims; README/acceptance wording; and non-self-reference.
2. Fresh-configure AppleClang headless, rerun focused/full CTest and mandatory
   Anaconda source oracle, and publish Spec and Task-Quality verdicts.  Fix all
   Critical, Important, or Minor findings in new scoped commits and re-review;
   do not amend prior accepted commits.
