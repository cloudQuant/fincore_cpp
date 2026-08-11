# fincore C++ Phase 13 Rolling Factor Exposures Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> `superpowers:subagent-driven-development` to execute this plan task by task,
> with a fresh implementer and independent reviewer for each implementation or
> acceptance task.

**Goal:** Add the deterministic, DataFrame-native expanding/fixed rolling
factor-exposure slice of Python
`fincore.attribution.fama_french.FamaFrenchModel.get_factor_exposures`, while
reusing the accepted native factor-regression public leaf for every eligible
window.

**Architecture:** Create an attribution `factor_exposures` leaf that validates
the entire selected return/factor boundary once, fills an output table with the
ordinary insufficient-history `NaN` prefix, and materializes each eligible raw
window only to delegate to `fit_factor_regression`.  It adds no mutable model,
provider, or duplicate QR solver.  The existing attribution umbrella exports
the new leaf after a leaf-isolated public consumer proves it.

**Tech Stack:** C++23, local cloudQuant/DataFrame, CMake, GoogleTest, local
Python oracle through `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base
python`, Homebrew LLVM 22 ASan+UBSan, and optional local Dear ImGui/ImPlot
compile/link verification.

## Global Constraints

- Use only `fincore::dataframe`, the accepted public factor-regression leaf,
  and the C++ standard library.  Do not add Eigen, BLAS/LAPACK, NumPy/pandas,
  Python embedding, a provider/cache, a model object, or a numerical library.
- Add only the factor-exposures attribution leaf and export it from the
  existing attribution umbrella.  Do not modify the accepted Phase 12 factor
  regression source/header/test, Brinson behavior, generic DataFrame helpers,
  metrics/rolling behavior, calendar behavior, third-party configuration, or
  ImPlot source.
- The explicit factor span is nonempty; factor names must be nonempty,
  distinct, and cannot be the reserved `factor_regression_alpha_name` (`alpha`).
  The result contains genuine-double columns in exact insertion order `alpha`,
  then caller-supplied factor names.  Do not infer factor names from metadata.
- Selected return/factor columns must exist, be genuine `double`, have exact
  owning index lengths, and be finite across the whole input.  NaN, `+inf`, and
  `-inf` are input errors even if a value would otherwise occur in an
  insufficient-history prefix.  Ignore unrelated malformed metadata.
- Both raw timestamp indexes must be duplicate-free and exactly equal in
  length, value, and supplied order.  Do not positional-zip, align on common
  timestamps, sort, resample, deduplicate, or otherwise repair inputs.
- `rolling_window == std::nullopt` is expanding history; a positive value is a
  trailing raw-row window.  `0` throws `fincore::input_error`.  A positive
  width below `K + 1` is valid and produces typed output full of `NaN`, because
  no fit is eligible.
- The output always has the complete validated returns index.  Rows with fewer
  than `K + 1` selected observations contain only `NaN`; every eligible row
  must call public `fit_factor_regression` on its materialized selected raw
  slice and map coefficient row zero to `alpha` and the following rows to
  factors in supplied order.  An eligible rank-deficient window throws
  `input_error`; never silently publish a minimum-norm solution or a fit-failure
  `NaN`.
- Preserve result ownership after source frame/name-storage mutation or
  destruction.  The output DataFrame must have a typed-empty schema for valid
  empty frames.
- Exclude Python's stateful model receiver, Newey-West/standard errors/p-values,
  risk-free adjustment, provider/fetch/cache, factor prediction, attribution
  decomposition, idiosyncratic risk, generic rolling metrics, reports/releases,
  full Python parity, and GUI runtime.
- All Python commands must use
  `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`; never use bare
  `python` or `python3`.

---

### Task 63: Add the strict rolling factor-exposures leaf

**Files:**

- Create: `include/fincore/attribution/factor_exposures.hpp`
- Create: `src/attribution/factor_exposures.cpp`
- Create: `tests/unit/factor_exposures_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Consumes:** `fincore::dataframe`, `timestamp`, `require_double_column`,
`input_error`, `returns_column`, `factor_regression_alpha_name`, and public
`fit_factor_regression` / `factor_regression_result`.

**Produces:**

~~~cpp
dataframe rolling_factor_exposures(
    const dataframe& returns,
    const dataframe& factor_returns,
    std::span<const std::string_view> factor_column_names,
    std::optional<std::size_t> rolling_window = std::nullopt,
    std::string_view returns_column_name = returns_column);
~~~

- [ ] **Step 1: Record the source-derived Python oracle and create a public-header RED test**

  Create `tests/unit/factor_exposures_test.cpp`, include only
  `<fincore/attribution/factor_exposures.hpp>` among fincore headers, and
  register it in `tests/CMakeLists.txt`.  Do not create the public header or
  register the source before the RED build.

  Run this exact local-source oracle and record its complete output in the
  ignored Task 63 report:

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
  values = (.001 + 1.5 * factors["MKT"] - .5 * factors["SMB"] +
            .25 * factors["HML"] + residual)
  returns = pd.DataFrame({"returns": values}, index=index)
  model = FamaFrenchModel("3factor")
  for window in (None, 4):
      output = model.get_factor_exposures(returns, factors, rolling_window=window)
      print("window", window)
      for name in output.columns:
          print(name, [repr(float(value)) for value in output[name].tolist()])
  '
  ~~~

  The native test fixture uses the same eight raw timestamps and MKT/SMB/HML
  values plus ignored unrelated malformed metadata.  Assert all three leading
  rows are `NaN` for every output column.  Assert expanding row four and final
  row, and fixed-width-four final row, with `1e-12` tolerance:

  ~~~text
  expanding row 3: alpha=.002333333333333203, MKT=1.483333333333333,
                    SMB=-.5333333333333329, HML=.13333333333333303
  expanding row 7: alpha=.0005694444444444665, MKT=1.5310634180729992,
                    SMB=-.45298603018400957, HML=.23286251744428613
  fixed(4) row 7: alpha=.0008977272727272866, MKT=1.4931818181818164,
                    SMB=-.4850000000000014, HML=.23818181818181863
  ~~~

- [ ] **Step 2: Run RED before production edits**

  Run:

  ~~~sh
  cmake --build build --parallel 4
  ctest --test-dir build -R FactorExposures --output-on-failure
  ~~~

  Expected: compilation fails only because the public leaf header is absent.
  A stale executable or a no-tests response is not valid RED evidence.

- [ ] **Step 3: Implement whole-frame validation, window materialization, and delegation**

  Create the header with exactly the API above, including its own public
  factor-regression dependency and `<optional>`/`<span>` declarations.  Keep
  all checked-name, checked-index, finite, materialization, and schema helpers
  private to the source.

  Before allocating output values, validate names; both duplicate-free exactly
  matching indexes; the selected return and all selected factor columns with
  `require_double_column`; and finiteness across all selected vectors.  Permit
  a valid typed-empty input and positive short window.  Reject `window == 0`
  before any output.

  Allocate one `NaN`-filled vector per output column over the entire return
  index.  For each row, compute the supplied-order expanding/trailing start and
  selected observation count.  If it is below `K + 1`, leave that row `NaN`.
  Otherwise materialize precisely the selected return/factor columns and their
  corresponding raw index slice, invoke public `fit_factor_regression`, and
  copy its scalar alpha and coefficient rows into this row.  Do not reproduce
  QR, use a normal equation, synthesize a fit failure, or call the Python
  oracle from C++.

  Return a fresh DataFrame that owns a copy of the return index, followed by
  `alpha` and factor columns in exact order.  Register only
  `src/attribution/factor_exposures.cpp` in the library source list.

- [ ] **Step 4: Add boundary regressions and run GREEN**

  Extend the same public-header test to prove:

  - exact Python expanding/fixed goldens, complete raw index, exactly ordered
    genuine-double schema, and `NaN` only in insufficient-history rows;
  - every eligible expanding/fixed row equals a separately materialized direct
    public `fit_factor_regression` result on the same raw slice;
  - successful shared nonmonotonic unique raw index order is retained exactly,
    including rowwise alpha/factor correspondence; a valid typed-empty fixture
    retains alpha-plus-factor typed schema; and a short positive width gives an
    all-`NaN` typed output;
  - empty/duplicate/reserved factor names, zero window, absent/wrong-type/short
    selected columns, duplicate/mismatched indexes, selected NaN/+inf/-inf,
    and an eligible rank-deficient window each throw `fincore::input_error`;
  - unrelated malformed metadata does not matter and outputs remain unchanged
    after source values/indexes/factor-name storage are mutated or destroyed.

  Run:

  ~~~sh
  cmake --build build --parallel 4
  ctest --test-dir build -R 'FactorExposures|FactorRegression|Attribution|Dataframe|Returns' --output-on-failure
  ctest --test-dir build --output-on-failure
  git diff --check
  ~~~

  Expected: source goldens, strict contracts, direct-delegation equivalence,
  and all existing CTests pass; diff check has no output.

- [ ] **Step 5: Review and commit only Task 63 paths**

  Review name/index/finite validation ordering, NaN-prefix semantics,
  expansion/trailing endpoint math, direct public-leaf delegation, typed output
  order, ownership, and absence of state/provider/QR duplication.  Stage only:

  ~~~sh
  git add CMakeLists.txt tests/CMakeLists.txt \
      include/fincore/attribution/factor_exposures.hpp \
      src/attribution/factor_exposures.cpp \
      tests/unit/factor_exposures_test.cpp
  git commit -m "feat: add rolling factor exposures"
  ~~~

  Expected: one exact Task 63 implementation commit and an ignored report with
  RED, full source Oracle, GREEN/full CTest, and scope evidence.

### Task 64: Export the factor-exposures leaf and accept Phase 13

**Files:**

- Modify: `include/fincore/attribution.hpp`
- Create: `tests/unit/attribution_factor_exposures_umbrella_test.cpp`
- Modify: `tests/CMakeLists.txt`
- Modify: `README.md`
- Create: `docs/acceptance/2026-08-12-phase-13-rolling-factor-exposures-acceptance.md`

**Consumes:** Task 63 public API and established fresh-gate evidence practice.

- [ ] **Step 1: Add an attribution-umbrella-only RED consumer**

  Create `tests/unit/attribution_factor_exposures_umbrella_test.cpp`, include
  only `<fincore/attribution.hpp>` among fincore headers, and register it.  It
  must build a small one-factor frame, call `rolling_factor_exposures`, and
  inspect the output alpha/factor schema and a fitted endpoint.  It must not
  include the factor-exposures or factor-regression leaf, metrics, or source
  headers.

- [ ] **Step 2: Run umbrella RED**

  Run:

  ~~~sh
  cmake --build build --parallel 4
  ctest --test-dir build -R AttributionFactorExposuresUmbrella --output-on-failure
  ~~~

  Expected: compilation fails solely because the umbrella does not export the
  new leaf; a stale executable is not evidence.

- [ ] **Step 3: Export and document the bounded rolling surface**

  Add exactly the factor-exposures include to `include/fincore/attribution.hpp`.
  Update README and create the acceptance record with the exact function,
  expanding/fixed output semantics, full-index/NaN-prefix schema, strict input
  and rank behavior, Task 63 full SHA, Python finite oracle/RED/GREEN evidence,
  and all exclusions.  State clearly that it is no-state rolling regression
  over supplied data, not a full Fama-French provider/model or GUI runtime
  claim.

- [ ] **Step 4: Run umbrella GREEN plus retained fresh gates**

  First run focused/full configured tests.  Then retain every fresh command's
  terminal output in new, non-overlapping directories with `tee`.  Execute
  scanner commands only after their CTest finishes.  If a fresh LLVM configure
  fails solely due a transient FetchContent/network failure, retain that
  directory/log/status as environmental evidence and use one new,
  non-overlapping retry directory for the acceptance proof; never silently
  discard or reuse the failed directory.  Do not launch a GUI.

  ~~~sh
  set -euo pipefail

  APPLE_DIR="$(mktemp -d /tmp/fincore-phase13-apple.XXXXXX)"
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

  LLVM_DIR="$(mktemp -d /tmp/fincore-phase13-llvm.XXXXXX)"
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

  IMPLOT_DIR="$(mktemp -d /tmp/fincore-phase13-implot.XXXXXX)"
  cmake -S . -B "$IMPLOT_DIR" -DCMAKE_BUILD_TYPE=Debug \
    -DFINCORE_BUILD_IMPLOT=ON -DFINCORE_BUILD_IMPLOT_DEMO=ON \
    2>&1 | tee "$IMPLOT_DIR/configure.log"
  cmake --build "$IMPLOT_DIR" --parallel 4 2>&1 | tee "$IMPLOT_DIR/build.log"
  ctest --test-dir "$IMPLOT_DIR" --output-on-failure 2>&1 | tee "$IMPLOT_DIR/ctest.log"
  rg -n 'warning:' "$IMPLOT_DIR/configure.log" "$IMPLOT_DIR/build.log" "$IMPLOT_DIR/ctest.log" \
    > "$IMPLOT_DIR/warnings-scan-final.log" || true

  INVALID_DIR="$(mktemp -d /tmp/fincore-phase13-invalid-imgui.XXXXXX)"
  if cmake -S . -B "$INVALID_DIR" -DFINCORE_BUILD_IMPLOT=ON \
      -DFINCORE_IMGUI_SOURCE_DIR=/definitely-invalid-imgui \
      2>&1 | tee "$INVALID_DIR/configure.log"; then
    echo 'expected invalid ImGui configure to fail' >&2
    exit 1
  fi
  rg -n -U 'FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required\s+files' \
    "$INVALID_DIR/configure.log"
  ~~~

  Record actual paths, statuses, compiler versions, CTest counts, scan timing,
  warning matches, no-match sanitizer result, any retained/retried transient
  network failure, and complete invalid-ImGui diagnostic in the ignored Task 64
  report and the acceptance document.  ImPlot evidence is compile/link only;
  no GUI executable may run.

- [ ] **Step 5: Commit only Task 64 tracked paths**

  Review consumer isolation, public output use, documentation truthfulness,
  retained-gate chronology, retry disclosure, and no-GUI claim.  Stage only:

  ~~~sh
  git add include/fincore/attribution.hpp tests/CMakeLists.txt \
      tests/unit/attribution_factor_exposures_umbrella_test.cpp README.md \
      docs/acceptance/2026-08-12-phase-13-rolling-factor-exposures-acceptance.md
  git commit -m "docs: accept phase 13 rolling factor exposures"
  ~~~

  Expected: one exact acceptance commit, ignored Task 64 report, and a clean
  tracked worktree.

### Task 65: Add Phase 13 non-self-referential traceability

**Files:**

- Modify: `docs/acceptance/2026-08-12-phase-13-rolling-factor-exposures-acceptance.md`

**Consumes:** Task 63's implementation SHA and Task 64's direct initial
acceptance SHA.

- [ ] **Step 1: Audit parents and scopes before editing**

  Confirm the direct parent/complete Task 63 and Task 64 SHAs, their path
  scopes, and clean tracked state.  Do not amend either prior commit.

- [ ] **Step 2: Append only trace record**

  Add a brief trace section that lists the complete Task 63 implementation SHA
  and Task 64 initial acceptance SHA, says Task 65 is documentation-only and
  does not amend prior commits, and never states or anticipates its own SHA.
  Do not alter source/test/README/numerical/gate claims.

- [ ] **Step 3: Verify and one-file commit**

  Run:

  ~~~sh
  ctest --test-dir build -R 'FactorExposures|AttributionFactorExposuresUmbrella' --output-on-failure
  git diff --check
  git add docs/acceptance/2026-08-12-phase-13-rolling-factor-exposures-acceptance.md
  git diff --cached --check
  git commit -m "docs: trace phase 13 rolling factor exposures acceptance"
  git show --check --stat HEAD
  ~~~

  Expected: focused tests pass and one tracked document changes.  Write an
  ignored Task 65 report with parent/SHA/scope evidence.

## Final Phase Review

After Task 65, use an independent, read-only reviewer who did not implement
the last task.  Verify the design/plan/API line by line; complete strict input
contract; full-index NaN-prefix and expanding/trailing endpoint math; direct
public fit equivalence; rank behavior; schemas/order/ownership; Python source
oracle; old API nonregression; CMake/test registration; all commit scopes and
non-self-referential trace; retained fresh gates and their claims.  Independently
run focused/full builds where useful.  Return separate Spec and Task-Quality
verdicts with Critical/Important/Minor findings; never silently repair code.

## Completion Criteria

- [ ] Public rolling factor exposures uses full raw return index, typed
  alpha-plus-factor columns, ordinary NaN prefix, and public QR delegation.
- [ ] Source-derived Python expanding/fixed finite goldens and all strict
  boundary/ownership contracts are proven.
- [ ] Umbrella-only public consumer, fresh Apple/LLVM/ImPlot/invalid-ImGui
  evidence, and accurate exclusions are accepted.
- [ ] Implementation, initial acceptance, and trace commits have exact scopes
  and complete non-self-referential provenance.
- [ ] Final independent review returns Spec PASS and Task-Quality PASS with no
  unresolved Critical/Important/Minor finding.
