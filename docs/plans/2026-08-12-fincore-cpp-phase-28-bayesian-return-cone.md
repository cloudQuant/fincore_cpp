# Phase 28 Bayesian Posterior Return-Cone Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a DataFrame-native, value-owning counterpart to Python's
deterministic `compute_bayes_cone` posterior-return post-processing helper.

**Architecture:** Add one metrics leaf that treats DataFrame rows as forecast
steps and genuine double columns as posterior-return paths. It compounds each
path in raw row order, computes seven fixed linear percentiles across paths at
each step, and returns a typed DataFrame with the original raw index. The
metrics façade exports the leaf only after the leaf implementation has passed
its independent review.

**Tech Stack:** C++23, local cloudQuant/DataFrame, CMake, GoogleTest, accepted
`detail::linear_percentile`, mandatory Anaconda Python source oracle, Homebrew
LLVM 22 ASan+UBSan, and optional local ImPlot compile/link acceptance.

**Baseline:** Accepted Phase 27 ends at
`4a3e38570d12602faa8087d3eb7d557d61483a8c` with 381 headless CTest cases.
Do not amend its leaf, acceptance, trace, or ignored review records.

## Global constraints

- Use only `fincore::dataframe`, `fincore::input_error`, existing
  `detail::linear_percentile`, cloudQuant/DataFrame, and the C++ standard
  library. Do not add Python/NumPy/PyMC embedding, a model or sampler class,
  random engine, cache, selector, callback, provider, plot source, overload,
  or non-local dependency.
- The public leaf API must be exactly:

  ```cpp
  [[nodiscard]] dataframe compute_bayes_cone(
      const dataframe& predicted_returns,
      double starting_value = 1.0);
  ```

- Expose exactly these ordered fixed output names:
  `percentile_1`, `percentile_5`, `percentile_25`, `percentile_50`,
  `percentile_75`, `percentile_95`, and `percentile_99`. Define matching
  public `std::string_view` constants in the leaf header; do not accept custom
  output names.
- The native input orientation is intentionally the transpose of Python's
  ndarray: raw rows are time/forecast steps, every genuine-`double` column is
  one posterior path. Preserve raw index order and values exactly; never sort,
  deduplicate, align, or reinterpret it.
- For each path and row, calculate `wealth *= 1.0 + return`; for each row,
  apply `detail::linear_percentile` to the path wealth values at
  `0.01, .05, .25, .50, .75, .95, .99`, in that order. All output must be
  freshly materialized and value-owning.
- Require at least one input column; every input column must be a genuine
  `double` column and have exactly the raw-index length. Empty index plus one
  or more typed empty double paths is valid and returns a typed empty output.
- Native error policy is stricter than NumPy propagation: `starting_value`,
  every input return, cumulative wealth, and each percentile must be finite.
  Zero/negative starting values and returns at/below `-1.0` remain valid when
  arithmetic stays finite. Map allocation/length errors to `input_error`.
- All Python commands must use
  `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`, never bare
  `python` or `python3`.
- Do not implement PyMC models/sampling, `Bayesian`,
  `compute_consistency_score`, stochastic parity, reports, GUI runtime, or any
  unrelated accepted API change in this phase.

### Task 120: Record Phase 28 design and executable plan

**Files:**

- Create: `docs/design/2026-08-12-phase-28-bayesian-return-cone-design.md`
- Create: `docs/plans/2026-08-12-fincore-cpp-phase-28-bayesian-return-cone.md`

1. Record the selected deterministic native leaf, rejected PyMC/class and
   simulation/plot alternatives, exact API, source-to-DataFrame transpose,
   output schema/order, finite policy, empty semantics, source oracle, test
   matrix, gates, and exclusions.
2. Run `git diff --check`; stage exactly these two documents; run cached diff
   check; commit:

   ```sh
   git commit -m "docs: plan phase 28 bayesian return cone"
   ```

### Task 121: Add the Bayesian posterior return-cone leaf

**Files:**

- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Create: `include/fincore/metrics/bayesian.hpp`
- Create: `src/metrics/bayesian.cpp`
- Create: `tests/unit/bayesian_cone_test.cpp`

1. Create the ignored Phase 28 SDD workspace, brief, ledger, and task report.
   Run and retain this exact Anaconda source oracle from
   `/Users/yunjinqi/Documents/new_projects/fincore`:

   ```sh
   /Users/yunjinqi/opt/anaconda3/bin/conda run -n base python -c '
   import numpy as np
   from fincore.metrics.bayesian import compute_bayes_cone
   preds = np.array([
       [0.01, -0.02, 0.03, 0.00],
       [0.00,  0.01, -0.01, 0.02],
       [-0.02, 0.03, 0.00, 0.01],
   ], dtype=float)
   print("start2", compute_bayes_cone(preds, starting_value=2.0))
   print("one", compute_bayes_cone(np.array([[.1, -.2]], dtype=float)))
   print("empty_time", compute_bayes_cone(np.empty((2, 0))))
   for value in (np.nan, np.inf, -np.inf):
       print("nonfinite_start", value,
             compute_bayes_cone(preds, starting_value=value))
   '
   ```

2. Before adding production declarations, create/register a leaf-only public
   consumer whose only fincore include is `<fincore/metrics/bayesian.hpp>`.
   It static-asserts the exact function pointer/default one-argument call and
   calls the function on a typed DataFrame. Run:

   ```sh
   cmake --build build --parallel 4
   ```

   Record the genuine missing-header/symbol RED. Do not run CTest after the
   failed build.

3. Add exactly the seven fixed output-name constants plus the planned function
   declaration to the leaf header. Keep it self-contained via project public
   headers; do not add a custom result struct or another overload.

4. Implement the minimum algorithm:

   - preflight finite starting value, at least one and only genuine double
     input columns, and exact raw-index length for every path before any
     output is built;
   - retain raw index positions, use column order only as the posterior-path
     population, and compound every path in that row order;
   - reject non-finite `1 + return`, wealth, and percentile results;
   - load the original index and the seven ordered double columns into a new
     DataFrame, including seven typed empty columns for zero rows;
   - translate only `std::bad_alloc`/`std::length_error` to `input_error`.

   Do not call Python, copy/transpose to a public array type, sort the raw
   index, synthesize timestamps, filter samples, special-case signs, retain
   views into the input, or alter `detail::linear_percentile`.

5. Add public tests without private production helpers:

   - exact function-pointer/default expression and all seven constant names;
   - the required Python oracle transposed to three named path columns, with a
     duplicate/non-monotonic raw index; assert total schema/type/order, all
     seven values at every step, and exact index retention;
   - one path where every percentile equals its cumulative path, including a
     negative starting value and a return below `-1.0` that stays finite;
   - typed-empty time input with two genuine empty paths;
   - no columns, mixed/non-double columns, malformed short double column,
     NaN/+/-infinite start, NaN/+/-infinite return, cumulative wealth overflow,
     and interpolation overflow as `input_error` boundaries;
   - source mutation/destruction after the call leaves result index/schema and
     values unchanged.

6. Run:

   ```sh
   cmake --build build --parallel 4
   ctest --test-dir build -R BayesianCone --output-on-failure
   ctest --test-dir build -R 'BayesianCone|MetricsUmbrella|Returns|Dataframe' --output-on-failure
   ctest --test-dir build --output-on-failure
   git diff --check
   ```

7. Commit exactly the five owned paths:

   ```sh
   git add CMakeLists.txt tests/CMakeLists.txt \
           include/fincore/metrics/bayesian.hpp \
           src/metrics/bayesian.cpp tests/unit/bayesian_cone_test.cpp
   git commit -m "feat: add bayesian posterior return cone"
   ```

8. Dispatch a fresh independent read-only spec review, then a code-quality
   review. Correct every Critical, Important, or Minor finding in new scoped
   commits and rerun focused/full CTest; never amend accepted history.

### Task 122: Export and accept the metrics-façade cone

**Files:**

- Modify: `README.md`
- Modify: `include/fincore/metrics.hpp`
- Modify: `tests/CMakeLists.txt`
- Create: `tests/unit/metrics_bayesian_cone_umbrella_test.cpp`
- Create: `docs/acceptance/2026-08-12-phase-28-bayesian-return-cone-acceptance.md`

1. Create/register a consumer with only `<fincore/metrics.hpp>` among fincore
   headers. Before adding the one Bayesian leaf include to the façade, build
   it and record the genuine missing-export RED; do not run a stale CTest.
2. Add only `#include <fincore/metrics/bayesian.hpp>` to the façade. The
   consumer must prove exact function/default expression, raw index including
   duplicates, exact seven-column total typed schema (no hidden non-double
   column), and representative source-oracle endpoints through the façade.
3. Document source mapping, fixed output schema, finite/empty policy, source
   oracle, leaf and façade RED/GREEN evidence, ownership, independent review,
   and exclusions. Keep the acceptance document free of Task 122's SHA until
   Task 123.
4. Retain raw `tee` logs for current focused/full CTest, fresh Apple headless,
   registration-audited Homebrew LLVM ASan+UBSan, ImPlot/demo compile/link-only
   with build-root executable checked but never launched, invalid-ImGui
   multiline diagnostic, and actual GoogleTest FetchContent provenance. If a
   gate attempt corrupts or time-slices CTest, retain it only as a diagnostic
   and use a unique clean retry for acceptance evidence.
5. Dispatch a fresh independent read-only document/gate review. Correct every
   Critical, Important, or Minor finding before committing exactly:

   ```sh
   git add README.md include/fincore/metrics.hpp tests/CMakeLists.txt \
           tests/unit/metrics_bayesian_cone_umbrella_test.cpp \
           docs/acceptance/2026-08-12-phase-28-bayesian-return-cone-acceptance.md
   git commit -m "docs: accept phase 28 bayesian return cone"
   ```

### Task 123: Add non-self-referential traceability

**Files:**

- Modify: `docs/acceptance/2026-08-12-phase-28-bayesian-return-cone-acceptance.md`

1. Record the full Task 121 implementation/fix chain, direct Task 122
   acceptance SHA, and exact scopes. State Task 123 is documentation-only and
   does not amend earlier commits; do not mention or predict its own SHA.
2. Run focused `BayesianCone|MetricsBayesianConeUmbrella` CTest and diff
   checks, then commit only this document:

   ```sh
   git commit -m "docs: trace phase 28 bayesian return cone"
   ```

### Final Phase 28 review

Audit Task 121/122/123 scope and ancestry; API/defaults/constants; exact
source-to-DataFrame orientation and linear-percentile math; raw-index/schema
order, typed-empty behavior, ownership, finite/overflow error boundaries, and
absence of PyMC/class/sampler/RNG/consistency-score scope drift; façade proof;
raw gates, FetchContent provenance, no-GUI claims; and trace wording.
Fresh-configure Apple headless, rerun focused/full CTest and the mandatory
Anaconda oracle, then publish Spec/Task-Quality verdicts. Correct every
Critical, Important, or Minor finding in new scoped commits; do not amend
accepted history.
