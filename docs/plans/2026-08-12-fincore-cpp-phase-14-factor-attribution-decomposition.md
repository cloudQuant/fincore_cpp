# fincore C++ Phase 14 Factor Attribution Decomposition Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> `superpowers:subagent-driven-development` to execute this plan task by task,
> with a fresh implementer and independent reviewer for each implementation or
> acceptance task.

**Goal:** Add the deterministic, DataFrame-native attribution-decomposition
slice of Python `fincore.attribution.fama_french.FamaFrenchModel`, while
reusing the accepted public factor-regression leaf for the only fit.

**Architecture:** A new `factor_attribution` leaf validates/owns its string
selectors, invokes `fit_factor_regression` exactly once, calculates each raw
factor's arithmetic-mean contribution, and materializes an ordinal-indexed
string/double summary DataFrame.  The existing attribution umbrella exports it
only after a leaf-isolated consumer proves the public surface.  No stateful
model, standard-factor receiver, provider, or duplicate QR solver is added.

**Tech Stack:** C++23, local cloudQuant/DataFrame, CMake, GoogleTest, local
Python oracle through `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base
python`, Homebrew LLVM 22 ASan+UBSan, and optional local Dear ImGui/ImPlot
compile/link verification.

**Baseline:** Accepted Phase 13 ends at
`63ea36c339bb3f9499df51d090df11f29d04b58f` with 266 headless CTest cases.
Do not amend prior acceptance, trace, boundary, or test-hardening commits.

## Global Constraints

- Use only `fincore::dataframe`, the accepted public
  `fit_factor_regression` / `factor_regression_result` API, and the C++
  standard library.  Do not add Eigen, BLAS/LAPACK, NumPy/pandas, Python
  embedding, provider/cache, a model object, or a numerical dependency.
- Add only this factor-attribution leaf and its public umbrella export.  Do
  not modify accepted Phase 12 factor-regression source/header/test, Phase 13
  rolling-exposure source/header/test, Brinson/performance-attribution
  behavior, generic DataFrame helpers, metrics/calendar behavior, third-party
  configuration, or ImPlot source.
- The explicit factor-name span is nonempty; every name is nonempty, has no
  embedded NUL, is distinct as a full string, and is not the reserved
  `factor_regression_alpha_name` (`alpha`).  The return selector has no
  embedded NUL.  Own all selector strings before any DataFrame C-string call
  or public-factor-regression delegation; a bounded non-NUL-terminated view is
  valid, while embedded-NUL selectors throw `fincore::input_error`.
- `risk_free_rate` must be finite.  The operation must invoke public
  `fit_factor_regression` exactly once using the owned selector values.  That
  delegate remains the authority for strict selected-column type/length and
  finite-value checks, duplicate-free exact raw-index matching, at-least
  `K + 1` observations, and full-rank QR behavior.  Do not align, sort, drop,
  resample, deduplicate, or positional-zip inputs.
- The output has synthetic timestamp index `0..K+3`, exactly one genuine
  `std::string` `component` column followed by one genuine `double`
  `contribution` column, and no other columns.  Rows are exactly `alpha`, then
  caller-ordered `<factor>_attribution` labels, then `specific_return`,
  `common_return`, and `unexplained`.  Do not return a dynamic map/dict or
  infer factor names.
- The Python-source semantics are literal: `specific_return` means the sum of
  factor contributions; `common_return` means arithmetic return mean minus the
  supplied risk-free rate; `unexplained` means
  `common_return - alpha - specific_return`.  It is not a residual or
  idiosyncratic-risk feature.
- Aggregate selected raw values in supplied order.  Reject a non-finite
  mean/product/sum/remainder intermediate as `input_error`; never publish
  accidental nonfinite output for an otherwise finite request.  Output must
  own all values and strings after source frame/name storage mutation or
  destruction.  Empty inputs are invalid under the delegated `K + 1`
  precondition, not typed-empty results.
- All Python commands must use
  `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`; never use bare
  `python` or `python3`.
- Exclude Python's stateful model receiver, fixed 3/5/momentum factor sets,
  provider/fetch/cache, prediction, rolling exposures, Newey-West/statistical
  outputs, p-values, residual/idiosyncratic-risk helpers, Brinson or position
  changes, reports/tearsheets, packages/releases, full Python dict parity, and
  GUI runtime.

---

### Task 66: Add the native factor-attribution decomposition leaf

**Files:**

- Create: `include/fincore/attribution/factor_attribution.hpp`
- Create: `src/attribution/factor_attribution.cpp`
- Create: `tests/unit/factor_attribution_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Consumes:** `fincore::dataframe`, `timestamp`, `require_double_column`,
`input_error`, `returns_column`, `factor_regression_alpha_name`, and public
`fit_factor_regression` / `factor_regression_result`.

**Produces:**

~~~cpp
inline constexpr std::string_view factor_attribution_component_column{
    "component"};
inline constexpr std::string_view factor_attribution_contribution_column{
    "contribution"};

dataframe factor_attribution_decomposition(
    const dataframe& returns,
    const dataframe& factor_returns,
    std::span<const std::string_view> factor_column_names,
    double risk_free_rate = 0.0,
    std::string_view returns_column_name = returns_column);
~~~

- [ ] **Step 1: Record a source-derived oracle and create a public-header RED test**

  Create `tests/unit/factor_attribution_test.cpp`, include only
  `<fincore/attribution/factor_attribution.hpp>` among fincore headers, and
  register it in `tests/CMakeLists.txt`.  Do not create the public header or
  register the source before RED is captured.

  Run this exact local-source oracle and save its complete output in the
  ignored Task 66 report:

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
  for risk_free_rate in (0.0, .0002):
      result = FamaFrenchModel("3factor", risk_free_rate=risk_free_rate).attribution_decomposition(
          returns, factors)
      print("risk_free_rate", repr(risk_free_rate))
      for name, value in result.items():
          print(name, repr(value))
  '
  ~~~

  Assert the `.0002` finite source goldens with `1e-12` tolerance:

  ~~~text
  alpha             = 0.0005694444444444665
  MKT_attribution   = 0.010526060999251869
  SMB_attribution   = -0.0031142789575150653
  HML_attribution   = 0.0010187735138187518
  specific_return   = 0.008430555555555556
  common_return     = 0.0088
  unexplained       = -0.00020000000000002134
  ~~~

  Run:

  ~~~sh
  cmake --build build --parallel 4
  ~~~

  Expected: compilation fails only because the public factor-attribution
  header/API is absent.  Do not treat an old executable or no-tests result as
  RED evidence.

- [ ] **Step 2: Implement strict owned delegation and the typed result**

  Create the header with all public declarations needed for `returns_column`;
  a consumer must not include a metric/private header.  In the source, keep
  selector ownership/validation, finite arithmetic, and DataFrame materializer
  helpers private.

  Before delegation, copy and validate the selector views, explicitly reject
  embedded NUL, validate finite risk-free rate, and build a span of views into
  owned factor strings.  Invoke public `fit_factor_regression` **once**.  After
  its strict validation succeeds, read the selected return/factor spans through
  the owned names.  Use the coefficient table rows one through `K` in supplied
  order to calculate each factor mean/product; compute the source-defined
  factor sum, return mean minus risk-free rate, and remainder.  Check every
  numeric aggregate before output construction.

  Load ordinal index `{0, ..., K + 3}`, string component labels, and double
  contributions with `dont_pad_with_nans`.  Register only
  `src/attribution/factor_attribution.cpp` in the library source list.

- [ ] **Step 3: Add boundary regressions and run GREEN**

  Extend the same leaf-isolated public-header test to prove:

  - source-oracle values, exactly ordered ordinal index/string/double schema,
    no extra total columns, and no output timestamp from the input leaks into
    the ordinal table;
  - requested-factor order changes the factor-attribution rows only; every
    contribution exactly matches a direct public `fit_factor_regression` beta
    times the independently calculated selected raw factor mean; and alpha
    matches the direct public fit;
  - changing only finite `risk_free_rate` changes only `common_return` and
    `unexplained` by the same negative amount; the literal
    `specific_return == sum(factor rows)` and source `unexplained` identity
    hold;
  - selected missing/wrong-type/short return or factor columns; empty,
    duplicate, reserved-alpha, embedded-NUL factor names; embedded-NUL return
    selector; bounded non-NUL-terminated return selector; duplicated or
    mismatched indexes; fewer-than-`K + 1` rows; selected NaN/`+inf`/`-inf`;
    nonfinite risk-free rate; and rank-deficient inputs all throw
    `fincore::input_error`;
  - unrelated malformed metadata has no effect; and output strings/vectors
    remain unchanged after input/name-storage mutation and destruction.

  Run:

  ~~~sh
  cmake --build build --parallel 4
  ctest --test-dir build -R 'FactorAttribution|FactorRegression|FactorExposures|Brinson|Dataframe|Returns|PerfAttrib' --output-on-failure
  ctest --test-dir build --output-on-failure
  git diff --check
  ~~~

  Expected: oracle/edge tests and prior attribution/DataFrame contracts pass;
  full CTest is green and `git diff --check` has no output.

- [ ] **Step 4: Review and commit only Task 66 paths**

  Review the single public-fit delegation, selector ownership, strict errors,
  literal source naming, aggregation arithmetic, schema/order, ownership, and
  absence of provider/model surface.  Stage only:

  ~~~sh
  git add CMakeLists.txt tests/CMakeLists.txt \
      include/fincore/attribution/factor_attribution.hpp \
      src/attribution/factor_attribution.cpp \
      tests/unit/factor_attribution_test.cpp
  git commit -m "feat: add factor attribution decomposition"
  ~~~

  Expected: one exact Task 66 implementation commit and an ignored report with
  RED, complete source oracle, GREEN/full evidence, and scope audit.

### Task 67: Export the leaf and accept Phase 14

**Files:**

- Modify: `include/fincore/attribution.hpp`
- Create: `tests/unit/attribution_factor_attribution_umbrella_test.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `README.md`
- Create: `docs/acceptance/2026-08-12-phase-14-factor-attribution-decomposition-acceptance.md`

**Consumes:** Task 66 public API and established retained fresh-gate practice.

- [ ] **Step 1: Add an attribution-umbrella-only RED consumer**

  Create `tests/unit/attribution_factor_attribution_umbrella_test.cpp`, include
  only `<fincore/attribution.hpp>` among fincore headers, and register it.  It
  must create a minimal full-rank one-factor frame, call
  `factor_attribution_decomposition`, and inspect the exact string/double
  output schema and source-defined component identities.  It must not include
  the factor-attribution/factor-regression/exposure leaf, metrics, or source
  headers.

- [ ] **Step 2: Run umbrella RED**

  ~~~sh
  cmake --build build --parallel 4
  ~~~

  Expected: compilation fails solely because the existing umbrella does not
  export the new leaf; a stale executable is not evidence.

- [ ] **Step 3: Export and document the bounded attribution surface**

  Add exactly the factor-attribution include to
  `include/fincore/attribution.hpp`.  Update README and create the acceptance
  record with the function, ordered two-column result, literal
  `specific_return` meaning, risk-free locality, strict delegated contracts,
  Task 66 full SHA, Python oracle/RED/GREEN evidence, and exclusions.  State
  clearly that it is a one-shot calculation over supplied factors—not a full
  Fama-French provider/model or GUI runtime claim.

- [ ] **Step 4: Run umbrella GREEN plus retained fresh gates**

  First run the configured focused/full tests.  Then retain every fresh
  command's terminal output in non-overlapping directories with `tee`.  Run
  scan commands only after CTest completes.  If LLVM configuration fails only
  because of transient FetchContent/network infrastructure, retain that
  failure as environmental evidence and use one fresh retry directory for the
  acceptance proof; never overwrite or silently reuse it.  Do not launch a
  GUI.

  ~~~sh
  set -euo pipefail

  APPLE_DIR="$(mktemp -d /tmp/fincore-phase14-apple.XXXXXX)"
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

  LLVM_DIR="$(mktemp -d /tmp/fincore-phase14-llvm.XXXXXX)"
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
  rg -n 'warning:' "$LLVM_DIR/configure.log" "$LLVM_DIR/build.log" "$LLVM_DIR/ctest.log" \
    > "$LLVM_DIR/warnings-scan-final.log" || true

  IMPLOT_DIR="$(mktemp -d /tmp/fincore-phase14-implot.XXXXXX)"
  cmake -S . -B "$IMPLOT_DIR" -DCMAKE_BUILD_TYPE=Debug \
    -DFINCORE_BUILD_IMPLOT=ON -DFINCORE_BUILD_IMPLOT_DEMO=ON \
    2>&1 | tee "$IMPLOT_DIR/configure.log"
  cmake --build "$IMPLOT_DIR" --parallel 4 2>&1 | tee "$IMPLOT_DIR/build.log"
  ctest --test-dir "$IMPLOT_DIR" --output-on-failure 2>&1 | tee "$IMPLOT_DIR/ctest.log"
  rg -n 'warning:' "$IMPLOT_DIR/configure.log" "$IMPLOT_DIR/build.log" "$IMPLOT_DIR/ctest.log" \
    > "$IMPLOT_DIR/warnings-scan-final.log" || true

  INVALID_DIR="$(mktemp -d /tmp/fincore-phase14-invalid-imgui.XXXXXX)"
  if cmake -S . -B "$INVALID_DIR" -DFINCORE_BUILD_IMPLOT=ON \
      -DFINCORE_IMGUI_SOURCE_DIR=/definitely-invalid-imgui \
      2>&1 | tee "$INVALID_DIR/configure.log"; then
    echo 'expected invalid ImGui configure to fail' >&2
    exit 1
  fi
  rg -n -U 'FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required\s+files' \
    "$INVALID_DIR/configure.log"
  ~~~

  Record actual paths, compiler versions, statuses, CTest counts, scan timing,
  warning matches/no-match results, any retained/retried network failure, and
  complete invalid-ImGui diagnostic in the ignored Task 67 report and the
  acceptance document.  ImPlot evidence is compile/link only; no GUI
  executable may run.

- [ ] **Step 5: Commit only Task 67 tracked paths**

  Review consumer isolation, public output use, documentation truthfulness,
  retained-gate chronology, no-GUI claim, and exact scope.  Stage only:

  ~~~sh
  git add include/fincore/attribution.hpp tests/CMakeLists.txt \
      tests/unit/attribution_factor_attribution_umbrella_test.cpp README.md \
      docs/acceptance/2026-08-12-phase-14-factor-attribution-decomposition-acceptance.md
  git commit -m "docs: accept phase 14 factor attribution decomposition"
  ~~~

  Expected: one exact acceptance commit, ignored Task 67 report, and clean
  tracked worktree.

### Task 68: Add Phase 14 non-self-referential traceability

**Files:**

- Modify: `docs/acceptance/2026-08-12-phase-14-factor-attribution-decomposition-acceptance.md`

**Consumes:** Task 66 implementation SHA and Task 67 direct initial acceptance
SHA.

- [ ] **Step 1: Audit parents and scopes before editing**

  Confirm the direct-parent relation and complete Task 66/67 SHAs, their exact
  path scopes, and clean tracked state.  Do not amend either prior commit.

- [ ] **Step 2: Append only a trace record**

  Add a brief trace section that lists the complete Task 66 implementation SHA
  and Task 67 initial acceptance SHA, says Task 68 is documentation-only and
  does not amend prior commits, and never states or anticipates its own SHA.
  Do not alter source/test/README/numerical/gate claims.

- [ ] **Step 3: Verify and one-file commit**

  ~~~sh
  ctest --test-dir build -R 'FactorAttribution|AttributionFactorAttributionUmbrella' --output-on-failure
  git diff --check
  git add docs/acceptance/2026-08-12-phase-14-factor-attribution-decomposition-acceptance.md
  git diff --cached --check
  git commit -m "docs: trace phase 14 factor attribution decomposition acceptance"
  git show --check --stat HEAD
  ~~~

  Expected: focused tests pass and exactly one tracked document changes.  Write
  an ignored Task 68 report with parent/SHA/scope evidence.

## Final Phase Review

After Task 68, use an independent, read-only reviewer who did not implement
the last task.  Verify the design/plan/API line by line; source-defined
component meaning; one public-fit delegation; owned bounded/NUL selector
behavior; strict type/index/finite/rank errors; arithmetic/schema/order and
ownership; Python source oracle; old API nonregression; CMake/test
registration; commit scopes/non-self-referential trace; and retained fresh
gates.  Independently run focused/full builds where useful.  Return separate
Spec and Task-Quality verdicts with Critical/Important/Minor findings; never
silently repair code.

## Completion Criteria

- [ ] The public native API returns the exact ordered string/double component
  DataFrame and delegates its only regression to the accepted Phase 12 leaf.
- [ ] Literal Python factor-contribution, risk-free, specific/common, and
  unexplained semantics plus strict boundaries/ownership are proven.
- [ ] Umbrella-only consumer, fresh Apple/LLVM/ImPlot/invalid-ImGui evidence,
  and accurate exclusions are accepted without launching a GUI.
- [ ] Implementation, initial acceptance, and trace commits have exact scopes
  and complete non-self-referential provenance.
- [ ] Final independent review returns Spec PASS and Task-Quality PASS with no
  unresolved Critical/Important/Minor finding.
