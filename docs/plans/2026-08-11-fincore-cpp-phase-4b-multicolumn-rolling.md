# fincore C++ Phase 4B Multi-column Rolling Metrics Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Complete the remaining Python `metrics.rolling` functions with
DataFrame-native single-, two-, and multi-factor rolling outputs.

**Architecture:** Extend the established `rolling.hpp`/`rolling.cpp`
facade. The endpoint alpha/beta API aligns once and delegates to public
`alpha_beta`. The full-index beta and compact regression APIs share a private
fixed-width pair-window calculation, because Python rolling covariance requires
all values in a window while scalar beta intentionally has different NaN
filtering. Multi-factor callers explicitly name selected factor columns and
receive a real multi-column cloudQuant/DataFrame result.

**Tech Stack:** C++23, CMake, GoogleTest, cloudQuant/DataFrame, local Python
fincore oracle through `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base
python`, and Homebrew LLVM for ASan+UBSan verification.

---

## Contract shared by every task

- Keep `fincore::dataframe` as `hmdf::StdDataFrame<fincore::timestamp>`.
  Load ordinary double result columns; do not invent a Series wrapper or a
  string-indexed table.
- Pair inputs with `align_on_common_index`, retaining common timestamps in
  the left/returns supplied order. Do not emulate Python's outer-shaped
  Series/Series `aligned_series` artifact or modify the helper.
- `roll_alpha_beta` returns endpoint-indexed `alpha`/`beta` columns and
  uses one public `alpha_beta` delegate per complete window. Resolve its
  requested annualization exactly once.
- `rolling_beta` returns an aligned full index with leading/invalid NaN
  windows. Its one-factor overload has the requested returns column name; its
  ordered explicit-factor overload has exactly the requested factor column
  names. No automatic scan of arbitrary DataFrame columns is allowed.
- `rolling_regression` returns compact valid endpoints in `alpha`/`beta`
  columns. Its alpha is the non-annualized intercept.
- Direct rolling beta/regression math is limited to local fixed-width
  covariance/variance behavior. Every non-finite pair makes that window NaN;
  zero factor variance is NaN. Do not change scalar beta, global filters,
  `rolling_engine`, ImPlot, or other metrics.
- Named columns must be nonempty genuine `double` vectors matching their
  frame index. Window zero, an empty/duplicate factor list, or structural
  violations throw `input_error`. Empty valid outputs retain their declared
  double column shape.

### Task 31: Add DataFrame-native remaining rolling metrics

**Files:**

- Modify: `include/fincore/metrics/rolling.hpp`
- Modify: `src/metrics/rolling.cpp`
- Modify: `tests/unit/rolling_metrics_test.cpp`

**Step 1: Write failing public-header and behavior tests**

Add tests that include only public fincore metric headers and attempt to call
absent `roll_alpha_beta`, both `rolling_beta` overloads, and
`rolling_regression`. Use the finite six-observation Python oracle in the
design to assert index, exact two-column/factor-column shapes, and all finite
values. Add a two-factor frame built through cloudQuant/DataFrame with ordered
`market` and `inverse` columns.

Cover native common-index left order, custom names, short/no-common result
shapes, zero window, duplicate/empty/missing/wrong-type/malformed selected
factors, constant factors, NaN/infinity windows, and the distinction between
full-index beta and compact regression. For `roll_alpha_beta`, test the
requested annualization against direct public `alpha_beta` windows and ensure
an invalid period has the accepted alpha-style validation behavior.

**Step 2: Run tests to verify the RED state**

Run:

```sh
cmake --build build --parallel 4
ctest --test-dir build -R RollingMetrics --output-on-failure
```

Expected: compilation fails only because the three new public APIs are absent
from `fincore/metrics/rolling.hpp`.

**Step 3: Implement the smallest native facade**

Declare the exact overloads in the design. In `rolling.cpp`, add only local
helpers for loading a two-column output, validating an ordered factor span,
materializing aligned windows, and calculating a finite fixed-width beta plus
non-annualized intercept. Reuse `paired_endpoint_output` only when its
single-column output fits; otherwise align once and load the two columns in
order. Do not add a public generic DataFrame factory or update
`rolling_engine`.

**Step 4: Run focused verification**

Run:

```sh
cmake --build build --parallel 4
ctest --test-dir build -R 'RollingMetrics|AlphaBeta|Ratios|Dataframe' --output-on-failure
git diff --check
```

Expected: all focused tests pass and the diff check has no output.

**Step 5: Commit**

```sh
git add include/fincore/metrics/rolling.hpp src/metrics/rolling.cpp \
        tests/unit/rolling_metrics_test.cpp
git commit -m "feat: add multicolumn rolling metrics"
```

### Task 32: Publish and accept the complete rolling metric module

**Files:**

- Modify: `tests/unit/metrics_umbrella_test.cpp`
- Create: `docs/acceptance/2026-08-11-phase-4b-multicolumn-rolling-acceptance.md`
- Modify: `README.md`

**Step 1: Add a public umbrella consumer regression**

Extend `MetricsUmbrella` through only `<fincore/metrics.hpp>` to construct
a factor DataFrame, call all newly accepted APIs, and assert the single/two/
multi-column names and a representative output index. Do not remove accepted
headers to manufacture a new RED; Task 31 supplies the genuine public API RED.

**Step 2: Publish only accepted behavior**

Document the two-column endpoints, full-index multi-factor beta, compact
regression, explicit factor selection, native alignment difference, nonfinite
window behavior, and the fact that the Python `rolling.py` public metrics are
now covered. Do not claim portfolio/report/optimization parity, a GUI visual
smoke test, or packaging/release readiness. Create the acceptance checklist
before final verification; fill exact commit SHAs, compilers, commands, counts,
and warnings only after they happen.

**Step 3: Run final verification in fresh directories**

Run in separate `mktemp -d` directories:

```sh
cmake -S . -B <fresh-core> -G Ninja -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF
cmake --build <fresh-core> --parallel 4
ctest --test-dir <fresh-core> --output-on-failure

cmake -S . -B <fresh-sanitize> -G Ninja -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF \
  -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++
cmake --build <fresh-sanitize> --parallel 4
ctest --test-dir <fresh-sanitize> --output-on-failure
```

Also reconfirm the default ImPlot build/demo contract and a fresh invalid-ImGui
override. Do not launch a GUI; build/link and headless CTest remain separate
from interactive visual proof.

**Step 4: Commit**

```sh
git add tests/unit/metrics_umbrella_test.cpp README.md \
        docs/acceptance/2026-08-11-phase-4b-multicolumn-rolling-acceptance.md
git commit -m "docs: accept phase 4B multicolumn rolling metrics"
```

## Deferred after this plan

Portfolio positions and transactions; round trips; performance attribution;
bootstrap/distribution statistics; Bayesian and advanced risk models;
optimization; simulation; reports/tearsheets; a full native facade;
packaging/release; and interactive ImPlot visual smoke testing remain separate
work. Completing this plan is not full fincore parity or project completion.
