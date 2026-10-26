# fincore C++ Phase 15 Factor-Regression Prediction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> `superpowers:subagent-driven-development` to execute this plan task by task,
> with a fresh implementer and independent reviewer for each implementation or
> acceptance task.

**Goal:** Add a deterministic, DataFrame-native consumer for an accepted
factor-regression result, covering the prediction behavior of Python
`FamaFrenchModel.predict` without introducing mutable model state.

**Architecture:** A new `factor_prediction` leaf validates and owns explicit
factor selectors, validates the public coefficient table carried by
`factor_regression_result`, and performs a finite checked rowwise
intercept-plus-beta calculation over a supplied factor DataFrame.  It retains
the raw prediction-frame index exactly.  The accepted attribution umbrella
exports the leaf only after an isolated consumer proves the public surface.

**Tech Stack:** C++23, local cloudQuant/DataFrame, CMake, GoogleTest, local
Python oracle through `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base
python`, Homebrew LLVM 22 ASan+UBSan, and optional local Dear ImGui/ImPlot
compile/link verification.

**Baseline:** Accepted Phase 14 ends at
`b18751974d90d3686b875c9bc1caad385c2e3aa4` with 274 headless CTest cases.
Do not amend its implementation, acceptance, trace, or final-review records.

## Global Constraints

- Use only `fincore::dataframe`, public `factor_regression_result`,
  `require_double_column`, `input_error`, and the C++ standard library.  Do
  not add Eigen, BLAS/LAPACK, NumPy/pandas, Python embedding, a provider/cache,
  an OLS/QR solver, or a model object.
- Add only the factor-prediction leaf and later its attribution-umbrella
  export.  Do not modify Phase 12 factor-regression source/header/test, Phase
  13 rolling-exposure source/header/test, Phase 14 attribution behavior,
  generic DataFrame helpers, metrics/calendar behavior, third-party
  configuration, or ImPlot source.
- The explicit factor-name span is nonempty; each name is nonempty, contains
  no embedded NUL, is distinct as a full string, and is not reserved
  `factor_regression_alpha_name` (`alpha`).  Own all selector strings before a
  DataFrame C-string call.  A bounded non-NUL-terminated view is valid;
  embedded-NUL selectors throw `fincore::input_error`.
- Treat `factor_regression_result` as a caller-owned model value.  Validate the
  consumed `alpha`/coefficient state strictly: finite alpha exactly equals
  coefficient row zero; coefficient table has exactly the genuine string
  `factor` and double `coefficient` columns; each column matches its index
  length; its index is ordinal `{0,...,K}`; labels are exactly `alpha` then the
  caller's requested names in order; all coefficient values are finite.  Do
  not inspect `r_squared`, `fitted_returns`, or `residuals`.
- Selected factor columns must exist, be genuine `double`, match the raw
  prediction frame index length, and contain finite values.  Unselected
  malformed metadata is irrelevant.  Prediction does not require unique,
  ordered, or training-matching timestamps because it performs no alignment;
  output preserves the supplied raw index/order including duplicates.
- The result has exactly one genuine `double` column
  `factor_prediction_column` (`"predicted_returns"`) and no extra column.  Its
  index exactly equals the input factor frame index.  Each row is the native
  source analogue `alpha + sum(beta[j] * selected_factor[j])`, evaluated in
  requested factor order.  Reject a nonfinite product/running sum/final result
  as `input_error`; never publish accidental nonfinite output.
- A valid model and selected empty factor frame produce a typed-empty output.
  The result owns its index/value storage after model/frame/name mutation or
  destruction.
- All Python commands must use
  `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`; never use bare
  `python` or `python3`.
- Exclude stateful model lifecycle, standard 3/5/momentum factor selection,
  refitting, QR changes, residual/idiosyncratic-risk calculations, rolling
  exposures, factor attribution, Newey-West/p-values, providers/caches,
  Python dict compatibility, reports/releases, full Python parity, and GUI
  runtime.

---

### Task 69: Add the native factor-regression prediction leaf

**Files:**

- Create: `include/fincore/attribution/factor_prediction.hpp`
- Create: `src/attribution/factor_prediction.cpp`
- Create: `tests/unit/factor_prediction_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Consumes:** `fincore::dataframe`, `timestamp`, `require_double_column`,
`input_error`, and public `factor_regression_result` constants/types.

**Produces:**

~~~cpp
inline constexpr std::string_view factor_prediction_column{"predicted_returns"};

dataframe factor_regression_predictions(
    const factor_regression_result& regression,
    const dataframe& factor_returns,
    std::span<const std::string_view> factor_column_names);
~~~

- [ ] **Step 1: Record the source oracle and create a public-header RED test**

  Create `tests/unit/factor_prediction_test.cpp`, include only
  `<fincore/attribution/factor_prediction.hpp>` among fincore headers, and
  register it in `tests/CMakeLists.txt`.  Do not create the public header or
  register the source before RED is captured.

  Run this exact local-source oracle and save its complete output in the
  ignored Task 69 report:

  ~~~sh
  cd /Users/yunjinqi/Documents/new_projects/fincore
  /Users/yunjinqi/opt/anaconda3/bin/conda run -n base python -c '
  import numpy as np
  import pandas as pd
  from fincore.attribution.fama_french import FamaFrenchModel

  index = pd.date_range("2024-01-01", periods=8, freq="D")
  factors = pd.DataFrame({
      "MKT": [.01, -.02, .03, .00, .015, -.01, .025, .005],
      "SMB": [.02, .01, -.01, .03, .00, .015, -.02, .01],
      "HML": [.00, .02, .01, -.01, .025, -.005, .015, -.02],
  }, index=index)
  residual = np.array([.0005, -.001, .0, .0015, -.0005, .00025, -.00075, .00025])
  returns = (.001 + 1.5 * factors["MKT"] - .5 * factors["SMB"] +
             .25 * factors["HML"] + residual)
  model = FamaFrenchModel("3factor")
  model.fit(returns, factors)
  for value in model.predict(factors):
      print(repr(float(value)))
  '
  ~~~

  Assert those values at `1e-12`:

  ~~~text
  0.0068203580214942685
  -0.02992443386996989
  0.0533598324629174
  -0.015348761635518682
  0.029356958651646607
  -0.0227002927762671
  0.05139868826161392
  -0.0009623491159163553
  ~~~

  Run:

  ~~~sh
  cmake --build build --parallel 4
  ~~~

  Expected: compilation fails only because the public prediction header/API is
  absent.  Do not treat an old executable or no-tests result as RED evidence.

- [ ] **Step 2: Implement the strict stateless predictor**

  Create the header with all declarations needed by the public
  `factor_regression_result` consumer.  In the source, keep selector ownership,
  coefficient-table validation, numeric checks, and DataFrame materialization
  helpers private.

  Validate/copy selected names and build views into owned storage.  Validate
  the regression coefficient schema, ordinal index, exact alpha/name binding,
  and finite coefficients before reading selected prediction-factor spans.
  Then calculate every prediction in raw factor-frame row order with checked
  finite multiplication and addition.  Preserve the raw index without index
  sorting, joining, uniqueness enforcement, or reference to historical
  training rows.  Load one owned double `predicted_returns` column with
  `dont_pad_with_nans`; register only the new source in `CMakeLists.txt`.

- [ ] **Step 3: Add boundary regressions and run GREEN**

  Extend the same leaf-isolated public-header test to prove:

  - source-oracle values, one genuine double column/no extras, and exact raw
    timestamp/index order;
  - a nonmonotonic duplicate-index prediction frame retains each row and its
    rowwise direct alpha-plus-beta formula; reordered selected names work only
    with a correspondingly reordered accepted fit/coefficient table;
  - valid selected empty frames return a typed-empty `predicted_returns`
    column; selected nonempty data never borrow model/frame/name storage after
    mutation/destruction;
  - empty, duplicate, reserved-alpha, embedded-NUL factor names; bounded
    non-NUL-terminated valid names; missing/wrong-type/short selected factor
    columns; selected NaN/`+inf`/`-inf`; and finite-product/running-sum
    overflow all throw `fincore::input_error`;
  - malformed public model state throws: nonfinite/mismatched alpha;
    non-ordinal/missing/extra/wrong-type/short coefficient table columns;
    wrong/missing/reordered labels; and nonfinite coefficient values.  Mutated
    `r_squared`, `fitted_returns`, or `residuals` alone do not affect a valid
    prediction because the public source prediction does not consume them;
  - unrelated malformed unselected factor metadata has no effect.

  Run:

  ~~~sh
  cmake --build build --parallel 4
  ctest --test-dir build -R 'FactorPrediction|FactorRegression|FactorExposures|FactorAttribution|Brinson|Dataframe|Returns|PerfAttrib' --output-on-failure
  ctest --test-dir build --output-on-failure
  git diff --check
  ~~~

  Expected: source/edge tests and prior attribution/DataFrame contracts pass;
  full CTest is green and `git diff --check` has no output.

- [ ] **Step 4: Review and commit only Task 69 paths**

  Review selector ownership, model-schema strictness, raw index policy,
  source-defined calculation, finite checks, typed-empty handling, output
  ownership, and absence of state/model/provider surface.  Stage only:

  ~~~sh
  git add CMakeLists.txt tests/CMakeLists.txt \
      include/fincore/attribution/factor_prediction.hpp \
      src/attribution/factor_prediction.cpp \
      tests/unit/factor_prediction_test.cpp
  git commit -m "feat: add factor regression predictions"
  ~~~

  Expected: one exact Task 69 implementation commit and an ignored report with
  RED, complete source oracle, GREEN/full evidence, and scope audit.

### Task 70: Export the predictor and accept Phase 15

**Files:**

- Modify: `include/fincore/attribution.hpp`
- Create: `tests/unit/attribution_factor_prediction_umbrella_test.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `README.md`
- Create: `docs/acceptance/2026-08-12-phase-15-factor-regression-prediction-acceptance.md`

**Consumes:** Task 69 public API and established retained fresh-gate practice.

- [ ] **Step 1: Add an attribution-umbrella-only RED consumer**

  Create the consumer with only `<fincore/attribution.hpp>` among fincore
  headers.  It must make a minimal full-rank fit, predict a small
  nonmonotonic/duplicate-index factor frame, and inspect exact raw index and
  one-double-column output.  It must not include a factor-prediction,
  factor-regression, exposure, metric, or source header.

- [ ] **Step 2: Run umbrella RED**

  ~~~sh
  cmake --build build --parallel 4
  ~~~

  Expected: compilation fails solely because the existing attribution umbrella
  does not export the prediction leaf; a stale executable is not evidence.

- [ ] **Step 3: Export and document the bounded prediction surface**

  Add exactly the factor-prediction include to
  `include/fincore/attribution.hpp`.  Update README and create an acceptance
  record with the function, model/selector requirements, raw-index policy,
  typed-empty behavior, source oracle/RED/GREEN evidence, Task 69 full SHA,
  and exclusions.  State clearly that this is stateless use of a supplied
  regression result—not a stateful Python model/provider or GUI-runtime claim.

- [ ] **Step 4: Run umbrella GREEN plus retained fresh gates**

  First run configured focused/full tests.  Then retain every fresh command's
  terminal output in non-overlapping directories with `tee`; run scan commands
  only after CTest completes.  If LLVM configuration fails only because of a
  transient FetchContent/network fault, retain it as environmental evidence and
  use one fresh retry directory for acceptance proof; never overwrite it or
  silently reuse it.  Do not launch a GUI.

  ~~~sh
  set -euo pipefail

  APPLE_DIR="$(mktemp -d /tmp/fincore-phase15-apple.XXXXXX)"
  cmake -S . -B "$APPLE_DIR" -DCMAKE_BUILD_TYPE=Debug -DFINCORE_BUILD_IMPLOT=OFF \
    2>&1 | tee "$APPLE_DIR/configure.log"
  cmake --build "$APPLE_DIR" --parallel 4 2>&1 | tee "$APPLE_DIR/build.log"
  ctest --test-dir "$APPLE_DIR" --output-on-failure 2>&1 | tee "$APPLE_DIR/ctest.log"
  {
    printf '%s\n' "inputs: $APPLE_DIR/configure.log $APPLE_DIR/build.log $APPLE_DIR/ctest.log"
    if rg -n 'warning:' "$APPLE_DIR/configure.log" "$APPLE_DIR/build.log" "$APPLE_DIR/ctest.log"; then
      echo 'unexpected AppleClang warning match' >&2
      exit 1
    else
      scan_rc=$?
      printf 'rg exit status: %s (expected no-match)\n' "$scan_rc"
      test "$scan_rc" -eq 1
    fi
  } > "$APPLE_DIR/warnings-scan-final.log"

  LLVM_DIR="$(mktemp -d /tmp/fincore-phase15-llvm.XXXXXX)"
  cmake -S . -B "$LLVM_DIR" -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
    -DCMAKE_BUILD_TYPE=Debug -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON \
    -DFINCORE_BUILD_IMPLOT=OFF 2>&1 | tee "$LLVM_DIR/configure.log"
  cmake --build "$LLVM_DIR" --parallel 4 2>&1 | tee "$LLVM_DIR/build.log"
  ctest --test-dir "$LLVM_DIR" --output-on-failure 2>&1 | tee "$LLVM_DIR/ctest.log"
  {
    printf '%s\n' "inputs: $LLVM_DIR/configure.log $LLVM_DIR/build.log $LLVM_DIR/ctest.log"
    if rg -n -e 'AddressSanitizer|UndefinedBehaviorSanitizer|runtime error' \
        "$LLVM_DIR/configure.log" "$LLVM_DIR/build.log" "$LLVM_DIR/ctest.log"; then
      echo 'unexpected sanitizer diagnostic match' >&2
      exit 1
    else
      scan_rc=$?
      printf 'rg exit status: %s (expected no-match)\n' "$scan_rc"
      test "$scan_rc" -eq 1
    fi
  } > "$LLVM_DIR/sanitizer-scan-final.log"

  IMPLOT_DIR="$(mktemp -d /tmp/fincore-phase15-implot.XXXXXX)"
  cmake -S . -B "$IMPLOT_DIR" -DCMAKE_BUILD_TYPE=Debug -DFINCORE_BUILD_IMPLOT=ON \
    -DFINCORE_BUILD_IMPLOT_DEMO=ON 2>&1 | tee "$IMPLOT_DIR/configure.log"
  cmake --build "$IMPLOT_DIR" --parallel 4 2>&1 | tee "$IMPLOT_DIR/build.log"
  ctest --test-dir "$IMPLOT_DIR" --output-on-failure 2>&1 | tee "$IMPLOT_DIR/ctest.log"
  test -x "$IMPLOT_DIR/examples/fincore_implot_demo"
  rg -n 'ImPlotAdapterCompile' "$IMPLOT_DIR/ctest.log"
  rg -n 'fincore_implot_demo' "$IMPLOT_DIR/build.log"

  INVALID_IMGUI_DIR="$(mktemp -d /tmp/fincore-phase15-invalid-imgui.XXXXXX)"
  if cmake -S . -B "$INVALID_IMGUI_DIR" -DFINCORE_BUILD_IMPLOT=ON \
      -DFINCORE_IMGUI_SOURCE_DIR=/definitely/not/a/dear-imgui-checkout \
      2>&1 | tee "$INVALID_IMGUI_DIR/configure.log"; then
    echo 'invalid Dear ImGui override unexpectedly configured' >&2
    exit 1
  fi
  rg -U -n 'FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout[[:space:]]+with all required files' \
    "$INVALID_IMGUI_DIR/configure.log"
  ~~~

  Expected: umbrella/full tests pass.  AppleClang, LLVM ASan+UBSan, and ImPlot
  gates retain their exact paths/logs and expected warnings; no sanitizer or
  fincore warning appears.  The ImPlot demo is compiled/linked only, never
  launched.  Invalid ImGui configuration fails with its complete expected
  diagnostic.

- [ ] **Step 5: Independently review and commit only Task 70 paths**

  Review the isolated umbrella surface, native source-policy difference,
  acceptance claims, source/RED/GREEN evidence, fresh-gate logs, exact scope,
  and no-GUI claim.  Stage only the five listed Task 70 paths, commit with
  `docs: accept phase 15 factor regression predictions`, and append the
  ignored report/ledger.  An independent reviewer must report a Spec and
  Task-Quality verdict before completion.

### Task 71: Add non-self-referential Phase 15 traceability

**Files:**

- Modify:
  `docs/acceptance/2026-08-12-phase-15-factor-regression-prediction-acceptance.md`

- [ ] **Step 1: Trace exact prior commits without amending them**

  Record the full Task 69 implementation SHA and direct Task 70 initial
  acceptance SHA beside their distinct path scopes.  If any review correction
  is necessary, distinguish it as a new correction commit.  State that this
  task is documentation-only, does not amend earlier commits, and must not
  state or anticipate its own SHA.

- [ ] **Step 2: Verify and commit exact one-file scope**

  ~~~sh
  ctest --test-dir build -R 'FactorPrediction|AttributionFactorPredictionUmbrella' --output-on-failure
  git diff --check
  git add docs/acceptance/2026-08-12-phase-15-factor-regression-prediction-acceptance.md
  git commit -m "docs: trace phase 15 factor regression prediction acceptance"
  git show --check --stat HEAD
  ~~~

  Expected: focused tests pass; the final commit changes exactly the
  acceptance document, is non-self-referential, and leaves tracked worktree
  state clean.

### Final Phase 15 review

- [ ] Verify the Task 69/70/71 commit chain, exact changed paths, public
  header/umbrella consumption, source oracle, strict model/selector behavior,
  raw-index and typed-empty policy, finite arithmetic, ownership, preserved
  regressions, retained Apple/LLVM/ImPlot/invalid-ImGui evidence, documentation
  accuracy, and non-self-referential trace.
- [ ] Independently rerun a fresh focused and full headless build/CTest.  If a
  Critical, Important, or Minor issue is found, add a new scoped follow-up
  commit and rerun this review; do not amend accepted commits.
