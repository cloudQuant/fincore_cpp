# fincore C++ Phase 5A Position Metrics Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add the DataFrame-native numeric core of Python position metrics:
allocation, long/short exposure, concentration, and gross leverage.

**Architecture:** A new positions.hpp/positions.cpp reads each genuine double
holding column through public cloudQuant/DataFrame APIs, validates its length,
and builds ordered multi-column results. The native long/short result combines
two same-index Python Series into a two-column DataFrame. Cash is optional for
long/short but required for gross leverage and concentration.

**Tech Stack:** C++23, CMake, GoogleTest, cloudQuant/DataFrame, local Python
fincore oracle through /Users/yunjinqi/opt/anaconda3/bin/conda run -n base
python, and Homebrew LLVM for ASan+UBSan verification.

---

## Contract shared by every task

- A positions frame uses its timestamp index and every genuine double column,
  in deterministic insertion order, as numeric holdings. Non-double metadata
  is intentionally ignored; no double columns or malformed doubles are
  input_error.
- get_percent_alloc preserves index and numeric column names/order. Row sums
  skip NaN; allocation results replace signed infinity with NaN.
- get_long_short_pos returns a DataFrame with long and short columns. It
  ignores a matching named cash column when present, permits its absence,
  treats NaN holdings as zero, and otherwise preserves IEEE sums.
- gross_lev requires a genuine named cash double field and returns one
  gross_lev column. get_max_median_position_concentration requires the same
  and returns exactly max_long, median_long, median_short, max_short in that
  order.
- No global finite filter, DataFrame helper/API expansion, scalar metric
  change, transaction/SID schema, rolling-engine change, dependency fetch, or
  ImPlot GUI execution is allowed.

### Task 33: Add native numeric position metrics

**Files:**

- Create: include/fincore/metrics/positions.hpp
- Create: src/metrics/positions.cpp
- Create: tests/unit/positions_test.cpp
- Modify: CMakeLists.txt
- Modify: tests/CMakeLists.txt

**Step 1: Write failing public-header tests**

Use the finite and zero-net-value goldens in the design to call all four
absent APIs through the new public header. Assert every index, ordered schema,
and numerical value, including NaN. Build a real multi-column DataFrame with
load_index/load_column; add a non-double metadata column to prove it is not
accidentally used.

Cover custom/absent cash names, typed empty result shapes, no numeric
positions, wrong-typed/malformed cash and numeric columns, and NaN/infinity
boundaries. Do not test private helpers.

**Step 2: Run the RED test**

~~~sh
cmake --build build --parallel 4
ctest --test-dir build -R Positions --output-on-failure
~~~

Expected: compilation fails because fincore/metrics/positions.hpp and its four
public declarations are absent.

**Step 3: Implement the smallest multi-column facade**

Add the source to the core CMake library and register its tests. Implement
private double-column enumeration/validation and ordered frame construction in
positions.cpp. Use direct row arithmetic matching the design; do not add a
generic factory to dataframe.hpp or expose an unplanned configuration API.

**Step 4: Run focused verification**

~~~sh
cmake --build build --parallel 4
ctest --test-dir build -R 'Positions|Dataframe' --output-on-failure
git diff --check
~~~

Expected: focused CTest passes and the diff check has no output.

**Step 5: Commit**

~~~sh
git add include/fincore/metrics/positions.hpp src/metrics/positions.cpp \
        tests/unit/positions_test.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add native position metrics"
~~~

### Task 34: Publish and accept the Phase 5A position surface

**Files:**

- Modify: include/fincore/metrics.hpp
- Modify: tests/unit/metrics_umbrella_test.cpp
- Create: docs/acceptance/2026-08-11-phase-5a-position-metrics-acceptance.md
- Modify: README.md

**Step 1: Add a public umbrella consumer**

Through only <fincore/metrics.hpp> plus the umbrella test's existing date
helper, build a positions frame and call all four metrics. Assert the native
output column schemas and representative indexes. Task 33 provides the
genuine public-header RED, so do not remove accepted headers to manufacture a
new one.

**Step 2: Publish accepted and deferred boundaries**

Document all-double position discovery, cash rules, the native long/short
two-column representation, allocation/gross infinity replacement, and
concentration behavior. State that top holdings, strings/SID schemas,
sector/style/cap/volume mappings, transactions, liquidation, round trips,
reports, and GUI runtime remain outside the phase. Create an unchecked
acceptance checklist before verification and fill real commit SHAs/evidence
only afterward.

**Step 3: Fresh acceptance verification**

In separate mktemp -d directories run fresh AppleClang headless full CTest
and Homebrew LLVM 22 ASan+UBSan headless full CTest. Also re-run default local
ImGui/ImPlot/demo compile/link/CTest and the expected failing fresh
invalid-ImGui override. Do not launch a GUI. Capture actual compiler versions,
test counts, warnings, and diagnostics.

**Step 4: Commit**

~~~sh
git add include/fincore/metrics.hpp tests/unit/metrics_umbrella_test.cpp \
        README.md docs/acceptance/2026-08-11-phase-5a-position-metrics-acceptance.md
git commit -m "docs: accept phase 5A position metrics"
~~~

## Deferred after this plan

Top-holdings and string/SID schemas; position extraction and stacking;
sector/style/cap/volume mappings; transactions, liquidation and round trips;
performance attribution; bootstrap/distribution statistics; Bayesian and
advanced risk models; optimization; simulation; reports/tearsheets; a full
native facade; packaging/release; and interactive ImPlot visual smoke testing
remain separate work. Completing this plan is not full fincore parity or
project completion.
