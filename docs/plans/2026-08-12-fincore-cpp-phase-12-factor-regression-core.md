# fincore C++ Phase 12 Factor Regression Core Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> `superpowers:subagent-driven-development` to execute this plan task by task,
> with a fresh implementer and independent reviewer for each implementation or
> acceptance task.

**Goal:** Add a deterministic, DataFrame-native ordinary-least-squares factor
regression leaf modeled on the fitting core of Python
`fincore.attribution.fama_french.FamaFrenchModel`, without claiming or
embedding a full Fama-French provider/model surface.

**Architecture:** A new attribution leaf accepts an explicitly selected return
column, an explicitly ordered nonempty list of selected factor-return columns,
and frames with exactly matching unique timestamp indexes.  It validates the
complete solve input, uses a private Householder-QR least-squares solver with
an intercept, and returns value-owning typed DataFrames for coefficients,
fitted returns, and residuals.  The existing attribution umbrella exports the
leaf after its public API is independently proven.

**Tech Stack:** C++23, local cloudQuant/DataFrame, CMake, GoogleTest, local
Python oracle through `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base
python`, Homebrew LLVM 22 ASan+UBSan, and optional local Dear ImGui/ImPlot
compile/link verification.

## Global Constraints

- Use only `fincore::dataframe` (`hmdf::StdDataFrame<fincore::timestamp>`),
  the accepted public DataFrame boundary, and the C++ standard library.  Do
  not add Eigen, BLAS/LAPACK, NumPy/pandas/Python embedding, a provider,
  filesystem cache, or a math dependency.
- This phase creates only the factor-regression attribution leaf and exports
  it from the existing attribution umbrella.  Do not change accepted metrics,
  Brinson behavior, generic DataFrame helpers, calendar behavior, third-party
  dependency configuration, or ImPlot source.
- The API has no implicit standard factor model.  Callers supply a nonempty,
  ordered factor-name span.  Empty names, duplicate names, and the reserved
  coefficient label `alpha` are invalid.  Output coefficient order is exactly
  `alpha` followed by that caller order.
- Every selected return/factor column must exist, be a genuine `double`, and
  have exactly the owning frame's index length.  Ignore unrelated columns and
  unrelated malformed metadata.  Do not infer factor columns from DataFrame
  metadata.
- Both supplied timestamp indexes must be duplicate-free and exactly equal in
  length, value, and raw order.  The operation does not positional-zip,
  align-on-intersection, sort, resample, deduplicate, or drop observations.
- All selected solve values must be finite.  NaN, `+inf`, and `-inf` cause
  `fincore::input_error`; they must never silently propagate into the solver.
  Require at least `K + 1` observations for `K` selected factors.  Exact fits
  are valid.  A rank-deficient intercept-plus-factor design is invalid and
  throws `input_error`, rather than returning an arbitrary minimum-norm
  solution.
- Implement the solver privately with Householder QR.  Preserve the original
  design values for fitted/residual calculation, choose its rank tolerance
  relative to the finite input matrix scale, and reject nonfinite intermediate
  results.  Do not copy the fixed 3-by-3 normal-equation helper from timing or
  refactor it out of its accepted scope.
- The coefficient result has ordinal timestamp index `0..K`, one genuine
  `std::string` `factor` column, and one genuine `double` `coefficient`
  column.  Fitted and residual outputs preserve the raw return index and each
  contain exactly one genuine-double column: `fitted_returns` or `residuals`.
  Results own their values after the source frames are changed or destroyed.
- `r_squared` is `1 - SSE/SST` when `SST > 0`; an exactly constant selected
  return vector reports exactly `0.0` as explicit native policy.  This avoids
  Python's floating mean-rounding artifact for some otherwise constant decimal
  values.  `alpha` repeats coefficient row zero for direct C++ use.
- Exclude Newey-West/standard errors/p-values, weighting/GLS, risk-free
  adjustment, standard Fama-French enum/factor lists, provider fetching or
  caching, stateful model objects, prediction, rolling exposures, attribution
  decomposition, idiosyncratic-risk helpers, reports, release work, full
  Python parity, and GUI runtime behavior.
- All Python commands must use
  `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`; never use bare
  `python` or `python3`.

---

### Task 60: Add the strict native factor-regression leaf

**Files:**

- Create: `include/fincore/attribution/factor_regression.hpp`
- Create: `src/attribution/factor_regression.cpp`
- Create: `tests/unit/factor_regression_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Consumes:** `fincore::dataframe`, `timestamp`, `require_double_column`,
`input_error`, `returns_column`, and the accepted DataFrame ownership/type
boundary.

**Produces:**

~~~cpp
inline constexpr std::string_view factor_regression_factor_column{"factor"};
inline constexpr std::string_view factor_regression_coefficient_column{
    "coefficient"};
inline constexpr std::string_view factor_regression_fitted_column{
    "fitted_returns"};
inline constexpr std::string_view factor_regression_residual_column{
    "residuals"};
inline constexpr std::string_view factor_regression_alpha_name{"alpha"};

struct factor_regression_result {
    double alpha;
    double r_squared;
    dataframe coefficients;
    dataframe fitted_returns;
    dataframe residuals;
};

factor_regression_result fit_factor_regression(
    const dataframe& returns,
    const dataframe& factor_returns,
    std::span<const std::string_view> factor_column_names,
    std::string_view returns_column_name = returns_column);
~~~

- [ ] **Step 1: Record the source-derived Python oracle and create a public-header RED test**

  Create `tests/unit/factor_regression_test.cpp`, with
  `<fincore/attribution/factor_regression.hpp>` as its only fincore header, and
  register it in `tests/CMakeLists.txt`.  Do not create the public header or
  register the source before capturing RED.

  Run the actual Python Fama-French fitting core and save the complete output
  in the ignored Task 60 report:

  ~~~sh
  cd /Users/yunjinqi/Documents/new_projects/fincore
  /Users/yunjinqi/opt/anaconda3/bin/conda run -n base python -c '
  import numpy as np
  import pandas as pd
  from fincore.attribution.fama_french import FamaFrenchModel

  index = pd.date_range("2024-01-01", periods=8, freq="D")
  factor_data = pd.DataFrame({
      "MKT": [.01, -.02, .03, .00, .015, -.01, .025, .005],
      "SMB": [.02, .01, -.01, .03, .00, .015, -.02, .01],
      "HML": [.00, .02, .01, -.01, .025, -.005, .015, -.02],
  }, index=index)
  residual = np.array([.0005, -.001, .0, .0015, -.0005, .00025, -.00075, .00025])
  values = (.001 + 1.5 * factor_data["MKT"] - .5 * factor_data["SMB"] +
            .25 * factor_data["HML"] + residual)
  result = FamaFrenchModel("3factor").fit(
      pd.Series(values, index=index), factor_data, newey_west_lags=0)
  # The source annotation is TypedDict and fit() returns this mapping.
  print("alpha", repr(result["alpha"]))
  print("betas", {name: repr(value) for name, value in result["betas"].items()})
  print("r_squared", repr(result["r_squared"]))
  print("residuals", [repr(value) for value in result["residuals"].tolist()])
  '
  ~~~

  The native oracle fixture uses the same eight raw timestamps and selected
  columns, plus unrelated malformed metadata.  Assert these finite goldens
  with `1e-12` tolerance:

  ~~~text
  alpha = 0.0005694444444444665
  MKT   = 1.5310634180729992
  SMB   = -0.45298603018400957
  HML   = 0.23286251744428613
  R²    = 0.9999467109765429
  residuals = [-0.0003203580214942679, -0.00007556613003010387,
                0.00014016753708259722, 0.00034876163551868407,
                -0.00010695865164660895, 0.00020029277626710043,
                0.00010131173838608121, -0.00028765088408364426]
  ~~~

- [ ] **Step 2: Run RED before production edits**

  Run:

  ~~~sh
  cmake --build build --parallel 4
  ctest --test-dir build -R FactorRegression --output-on-failure
  ~~~

  Expected: compilation fails only because the public factor-regression header
  is absent.  A stale test executable or a no-tests result is not RED evidence.

- [ ] **Step 3: Implement strict validation, private QR, and typed outputs**

  Create the public header with exactly the API above and include every public
  declaration needed for its default `returns_column`; consumers must not have
  to include a metrics or private source header.  In the source, keep name,
  index, finite-value, QR, and DataFrame-construction helpers private.

  Validate owned names before reading selected columns.  Validate each frame's
  index has no duplicate timestamps and the two full index vectors compare
  equal.  Use `require_double_column` for the selected return and every
  selected factor.  Check selected values with `std::isfinite` before any
  solve.  Treat `N < K + 1` and a rank-deficient QR diagonal as `input_error`.

  Construct an `N x (K + 1)` local design matrix with an intercept-one first
  column.  Apply a stable private Householder QR transformation to that matrix
  and the response vector, with a scale-relative rank tolerance; then
  back-substitute.  Retain an original design copy to calculate fitted values
  and residuals in raw input order.  Calculate `SSE`, return `0.0` for a zero
  `SST`, otherwise calculate `1.0 - SSE / SST`.

  Build the three DataFrames with value-owning vectors only.  The coefficients
  DataFrame has timestamp index `{0, 1, ..., K}`, string labels `alpha` then
  supplied names, then the paired double coefficients.  The other two frames
  use exactly the supplied return index and one output double column each.
  Register only `src/attribution/factor_regression.cpp` in the library source
  list.

- [ ] **Step 4: Add boundary regressions and run GREEN**

  Extend the same public-header test to prove:

  - all scalar/table oracle goldens, exactly ordered string/double coefficient
    schema, index retention, fitted-plus-residual identity, and `alpha ==`
    coefficient row zero;
  - supplied factor order controls coefficient rows; a permutation produces
    the mathematically corresponding permuted coefficient rows;
  - an exact `N == K + 1` fit is accepted, and a constant-return full-rank
    fixture reports exactly `r_squared == 0.0`;
  - selected empty/missing/wrong-type/short columns; empty/duplicate/reserved
    factor names; duplicated or mismatched timestamps; fewer than `K + 1`
    rows; selected NaN, `+inf`, and `-inf`; and rank-deficient factors each
    throw `fincore::input_error`;
  - unrelated malformed metadata has no effect; and outputs remain unchanged
    after mutating or destroying source fixtures.

  Run:

  ~~~sh
  cmake --build build --parallel 4
  ctest --test-dir build -R 'FactorRegression|Brinson|Dataframe|Returns|PerfAttrib' --output-on-failure
  ctest --test-dir build --output-on-failure
  git diff --check
  ~~~

  Expected: oracle/edge tests pass, prior attribution and DataFrame contracts
  remain green, full CTest is green, and `git diff --check` has no output.

- [ ] **Step 5: Review and commit only Task 60 paths**

  Review strict index policy, name ownership/order, rejection before QR, rank
  behavior, output schema/order, result ownership, and that no provider or
  statistical-inference surface slipped in.  Stage only:

  ~~~sh
  git add CMakeLists.txt tests/CMakeLists.txt \
      include/fincore/attribution/factor_regression.hpp \
      src/attribution/factor_regression.cpp \
      tests/unit/factor_regression_test.cpp
  git commit -m "feat: add native factor regression"
  ~~~

  Expected: one task-scoped implementation commit plus an ignored Task 60
  report containing RED, exact Anaconda oracle, GREEN/full evidence, and a
  scope audit.

### Task 61: Export the attribution leaf and accept Phase 12

**Files:**

- Modify: `include/fincore/attribution.hpp`
- Create: `tests/unit/attribution_factor_regression_umbrella_test.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `README.md`
- Create: `docs/acceptance/2026-08-12-phase-12-factor-regression-core-acceptance.md`

**Consumes:** Task 60's public API, accepted attribution umbrella, and the
retained fresh-gate convention.

- [ ] **Step 1: Add an attribution-umbrella-only RED consumer**

  Create `tests/unit/attribution_factor_regression_umbrella_test.cpp`, include
  only `<fincore/attribution.hpp>` among fincore headers, and register it.  It
  should build a minimal valid full-rank one-factor frame, call
  `fit_factor_regression`, and consume the scalar, string coefficient table,
  fitted frame, and residual frame.  It must not include the leaf, Brinson,
  metrics, or private source headers.

- [ ] **Step 2: Run umbrella RED**

  Run:

  ~~~sh
  cmake --build build --parallel 4
  ctest --test-dir build -R AttributionFactorRegressionUmbrella --output-on-failure
  ~~~

  Expected: the public consumer fails solely because the existing attribution
  umbrella does not export the leaf.  A stale test executable is not evidence.

- [ ] **Step 3: Export and document the bounded native surface**

  Add exactly the new leaf include to `include/fincore/attribution.hpp` next
  to Brinson.  Update README and create the acceptance record with the exact
  API/table schemas, strict same-index and finite/rank policy, Task 60 full
  SHA, Python finite oracle, RED/GREEN evidence, and exclusions.  Be explicit
  that this is a no-state generic factor-regression core, not a full
  Fama-French/provider/p-value/Newey-West/prediction/rolling/decomposition
  implementation, and that any ImPlot proof is compile/link only.

- [ ] **Step 4: Run umbrella GREEN plus retained fresh acceptance gates**

  First run focused and full configured tests.  Then create four new,
  non-overlapping temporary directories.  Retain every configure/build/CTest
  command's terminal log with `tee`; execute scanner commands only after CTest
  completes so their mtime/evidence cannot precede the test log.  Do not delete
  any existing temporary directory and do not launch a GUI.

  ~~~sh
  set -euo pipefail

  APPLE_DIR="$(mktemp -d /tmp/fincore-phase12-apple.XXXXXX)"
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

  LLVM_DIR="$(mktemp -d /tmp/fincore-phase12-llvm.XXXXXX)"
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

  IMPLOT_DIR="$(mktemp -d /tmp/fincore-phase12-implot.XXXXXX)"
  cmake -S . -B "$IMPLOT_DIR" -DCMAKE_BUILD_TYPE=Debug \
    -DFINCORE_BUILD_IMPLOT=ON -DFINCORE_BUILD_IMPLOT_DEMO=ON \
    2>&1 | tee "$IMPLOT_DIR/configure.log"
  cmake --build "$IMPLOT_DIR" --parallel 4 2>&1 | tee "$IMPLOT_DIR/build.log"
  ctest --test-dir "$IMPLOT_DIR" --output-on-failure 2>&1 | tee "$IMPLOT_DIR/ctest.log"
  rg -n 'warning:' "$IMPLOT_DIR/configure.log" "$IMPLOT_DIR/build.log" "$IMPLOT_DIR/ctest.log" \
    > "$IMPLOT_DIR/warnings-scan-final.log" || true

  INVALID_DIR="$(mktemp -d /tmp/fincore-phase12-invalid-imgui.XXXXXX)"
  if cmake -S . -B "$INVALID_DIR" -DFINCORE_BUILD_IMPLOT=ON \
      -DFINCORE_IMGUI_SOURCE_DIR=/definitely-invalid-imgui \
      2>&1 | tee "$INVALID_DIR/configure.log"; then
    echo 'expected invalid ImGui configure to fail' >&2
    exit 1
  fi
  # CMake may wrap "required files" across physical output lines.
  rg -n -U 'FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required\s+files' \
    "$INVALID_DIR/configure.log"
  ~~~

  Record actual paths, exit statuses, compiler versions, test counts, exact
  warnings, no-match sanitizer scan, invalid-ImGui diagnostic, and the
  compile/link-only/no-GUI boundary in both the ignored Task 61 report and the
  acceptance record.  The invalid configuration must exit nonzero and show the
  complete Dear-ImGui diagnostic, allowing CMake's physical line wrapping in
  the retained-log scanner.

- [ ] **Step 5: Commit only Task 61 tracked paths**

  Review the umbrella consumer's isolation, README/acceptance claims against
  retained logs, warning/sanitizer evidence ordering, and the absence of GUI
  execution.  Stage only:

  ~~~sh
  git add include/fincore/attribution.hpp \
      tests/CMakeLists.txt \
      tests/unit/attribution_factor_regression_umbrella_test.cpp \
      README.md \
      docs/acceptance/2026-08-12-phase-12-factor-regression-core-acceptance.md
  git commit -m "docs: accept phase 12 factor regression"
  ~~~

  Expected: one task-scoped acceptance commit, with a clean tracked worktree
  and an ignored Task 61 evidence report.

### Task 62: Add Phase 12 non-self-referential traceability

**Files:**

- Modify: `docs/acceptance/2026-08-12-phase-12-factor-regression-core-acceptance.md`

**Consumes:** Task 60's implementation SHA and Task 61's direct acceptance
commit SHA.

- [ ] **Step 1: Audit the exact parent chain before editing**

  Confirm Task 61 is the direct parent, retrieve the complete Task 60 and
  Task 61 SHAs, verify their owned-path scopes, and confirm the source tree is
  otherwise clean.  Do not amend either previous commit.

- [ ] **Step 2: Append only the trace record**

  Add a short trace section to the acceptance record that lists the complete
  Task 60 implementation SHA and Task 61 initial-acceptance SHA.  State that
  Task 62 is documentation-only, does not amend either prior commit, and does
  not state or anticipate its own SHA.  Do not alter source, tests, README,
  numerical claims, or gate evidence.

- [ ] **Step 3: Focused verification and one-file commit**

  Run:

  ~~~sh
  ctest --test-dir build -R 'FactorRegression|AttributionFactorRegressionUmbrella' --output-on-failure
  git diff --check
  git add docs/acceptance/2026-08-12-phase-12-factor-regression-core-acceptance.md
  git diff --cached --check
  git commit -m "docs: trace phase 12 factor regression acceptance"
  git show --check --stat HEAD
  ~~~

  Expected: focused tests pass, the commit changes exactly the acceptance
  document, and the follow-up stays non-self-referential.  Save an ignored
  Task 62 report with SHA/parent/scope evidence.

## Final Phase Review

After Task 62, use an independent read-only reviewer who did not implement the
last task.  The reviewer must check the design/plan/API line by line; raw index
and finite/rank semantics; Householder QR vs prohibited normal-equation reuse;
the exact Python oracle; string/double output schemas; output ownership;
old-API non-regression; CMake/test registration; all three commit scopes and
trace non-self-reference; retained Apple/LLVM/ImPlot/invalid-ImGui evidence;
and a fresh focused/full build when useful.  It must return separate Spec and
Task-Quality verdicts, list any Critical/Important/Minor finding with concrete
evidence, and request a bounded follow-up rather than silently fixing code.

## Completion Criteria

- [ ] The strict public leaf matches the documented C++ API and owns the three
  typed DataFrame outputs.
- [ ] The actual source-derived Python 3-factor finite oracle, public RED,
  GREEN/full CTest, and boundary contracts are recorded.
- [ ] The attribution umbrella is proven by a leaf-isolated public consumer.
- [ ] Fresh AppleClang, LLVM ASan+UBSan, optional ImPlot compile/link-only, and
  expected invalid-ImGui gates are retained and accurately documented.
- [ ] Implementation, initial acceptance, and trace commits have exact scopes;
  traceability is complete and non-self-referential.
- [ ] An independent phase review reports Spec PASS and Task-Quality PASS with
  no unresolved Critical/Important/Minor finding.
