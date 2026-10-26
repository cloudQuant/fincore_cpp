# fincore C++ Phase 5B Transaction Metrics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (- [ ]) syntax for tracking.

**Goal:** Add DataFrame-native daily transaction-volume aggregation and portfolio turnover metrics with explicit UTC-day and position-index contracts.

**Architecture:** A focused transactions module owns raw amount/price validation, UTC-midnight grouping, daily aggregation, daily-position validation, and ordered result construction. It uses Phase 5A's cash naming and all-genuine-double positions convention without broadening dataframe.hpp or implementing transaction symbols, market data, or pandas MultiIndex behavior.

**Tech Stack:** C++23, CMake, GoogleTest, cloudQuant/DataFrame, local Python fincore oracle via /Users/yunjinqi/opt/anaconda3/bin/conda run -n base python, Homebrew LLVM 22 for ASan+UBSan, local ImGui/ImPlot only for the existing compile/link gate.

## Global Constraints

- Use the timestamp type as UTC nanoseconds; normalize transaction and position dates through public calendar facilities without adding a generic DataFrame helper.
- Raw transactions may have duplicate and intraday timestamps; turnover positions must have strictly increasing, unique normalized UTC days.
- Relevant transaction fields are distinct genuine double amount and price columns, each index-length matched; ignore unrelated fields.
- Positions select every genuine double column in insertion order and validate every selected column; no numeric positions is input_error.
- actual_gross_book requires genuine named double cash; portfolio_value does not. Do not weaken this distinction.
- Preserve local pandas arithmetic: row/group sums skip NaN, signed infinity remains ordinary until final turnover sanitation, and final nonfinite turnover is zero.
- Do not modify dataframe.hpp, DataFrame upstream, scalar metrics, positions.cpp, rolling code, ImPlot adapter, or dependencies. Do not fetch, clone, vendor, fabricate, or execute a GUI.

---

### Task 35: Add native daily transaction aggregation and turnover

**Files:**

- Create: include/fincore/metrics/transactions.hpp
- Create: src/metrics/transactions.cpp
- Create: tests/unit/transactions_test.cpp
- Modify: CMakeLists.txt
- Modify: tests/CMakeLists.txt

**Interfaces:**

~~~cpp
namespace fincore {

inline constexpr std::string_view transaction_amount_column{"amount"};
inline constexpr std::string_view transaction_price_column{"price"};

enum class turnover_denominator {
    actual_gross_book,
    portfolio_value,
};

dataframe get_txn_vol(
    const dataframe& transactions,
    std::string_view amount_column_name = transaction_amount_column,
    std::string_view price_column_name = transaction_price_column);

dataframe get_turnover(
    const dataframe& positions,
    const dataframe& transactions,
    turnover_denominator denominator = turnover_denominator::actual_gross_book,
    std::string_view cash_column_name = cash_column,
    std::string_view amount_column_name = transaction_amount_column,
    std::string_view price_column_name = transaction_price_column);

}  // namespace fincore
~~~

**Consumes:** dataframe, timestamp, input_error, utc_timestamp, and cash_column from existing public headers.

**Produces:** daily transaction DataFrames with exact columns txn_volume, then txn_shares, and turnover DataFrames with exact column turnover.

- [ ] **Step 1: Write failing public-header tests**

Create transactions_test.cpp that includes only fincore/error.hpp, fincore/calendar.hpp, and fincore/metrics/transactions.hpp. Build raw transactions with duplicate intraday UTC timestamps on Jan 2, a NaN record on Jan 3, and an infinite-price record on Jan 5. Assert the Jan 2/3/5 midnight index and the exact oracle aggregation:

~~~cpp
EXPECT_EQ(daily.get_index(), expected_days);
expect_double_columns(daily, {"txn_volume", "txn_shares"});
expect_values(daily.get_column<double>("txn_volume"), {90.0, 0.0, inf});
expect_values(daily.get_column<double>("txn_shares"), {14.0, 0.0, 3.0});
~~~

Build daily positions asset=[100,-50,20], cash=[900,1050,980] on Jan 2, 3, and 4. Assert the ascending Jan 2--5 output union, actual-gross-book turnover [1.8,0,0,0], and portfolio-value turnover [.09,0,0,0]. Prove the first AGB denominator is half the first gross book. Include raw transaction input order independent daily sorting, custom amount/price/cash names, typed empty transaction and turnover outputs, non-double metadata exclusion, NaN/infinity sanitation, absent/wrong/malformed required fields, equal field names, no numeric position fields, missing/wrong cash for AGB, valid cash-less portfolio-value turnover, duplicate/out-of-order normalized position days, and an unknown enum cast. Do not test private helpers.

- [ ] **Step 2: Run the RED test**

~~~sh
cmake --build build --parallel 4
ctest --test-dir build -R Transactions --output-on-failure
~~~

Expected: compilation fails because fincore/metrics/transactions.hpp and both public declarations are absent.

- [ ] **Step 3: Implement the smallest native transaction module**

Register transactions.cpp in the fincore target and transactions_test.cpp in fincore_tests. In transactions.cpp implement private helpers that:

1. validate distinct named double amount/price columns with require_double_column;
2. convert each timestamp to UTC midnight using existing calendar APIs;
3. aggregate raw rows in an ordered std::map<timestamp, totals> while skipping raw NaN contributions and preserving ordinary IEEE arithmetic for signed infinity;
4. enumerate and index-length validate every genuine double position column;
5. validate strict ascending unique normalized position days;
6. compute AGB denominators as first gross/2 followed by (previous_gross + current_gross)/2, or portfolio-value denominators as all-numeric row sums;
7. emit the sorted union of position and transaction days and replace every nonfinite final turnover with 0.0.

Keep all helpers private. Do not add calendar.hpp or dataframe.hpp APIs, add an implicit cash fallback, or copy unrelated slippage/liquidity logic.

- [ ] **Step 4: Run focused and regression verification**

~~~sh
cmake --build build --parallel 4
ctest --test-dir build -R 'Transactions|Positions|Dataframe' --output-on-failure
git diff --check
~~~

Expected: the focused suite passes and diff check has no output.

- [ ] **Step 5: Commit Task 35**

~~~sh
git add CMakeLists.txt tests/CMakeLists.txt \
        include/fincore/metrics/transactions.hpp \
        src/metrics/transactions.cpp tests/unit/transactions_test.cpp
git commit -m "feat: add native transaction turnover metrics"
~~~

### Task 36: Publish and accept the Phase 5B transaction surface

**Files:**

- Modify: include/fincore/metrics.hpp
- Modify: tests/unit/metrics_umbrella_test.cpp
- Modify: README.md
- Create: docs/acceptance/2026-08-11-phase-5b-transaction-metrics-acceptance.md

**Consumes:** Task 35 public header and public calendar helper only.

**Produces:** umbrella availability and evidence-backed acceptance material for the bounded transaction aggregation/turnover surface.

- [ ] **Step 1: Add a public-only umbrella consumer**

Include only <fincore/metrics.hpp> and the existing allowed public calendar header in metrics_umbrella_test.cpp. Construct raw duplicate/intraday transactions and daily positions, call get_txn_vol and both turnover enum variants, and assert public result schemas, daily index normalization, representative values, and the transaction-only union day. Do not include transactions.hpp directly or remove accepted headers to fabricate RED.

- [ ] **Step 2: Prepare the acceptance checklist and run fresh gates**

Before tests, create an unchecked acceptance checklist covering public-only consumer behavior, Task 35 RED evidence, AppleClang core, LLVM sanitizer, positive default ImGui/ImPlot/demo compile boundary, invalid ImGui negative configure, owned path audit, accurate warnings, no GUI execution, and deferred scope. Then use separate mktemp -d roots to run:

~~~sh
phase5b_apple_root="$(mktemp -d /tmp/fincore-task36-apple.XXXXXX)"
cmake -S . -B "$phase5b_apple_root/build" -G Ninja \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF
cmake --build "$phase5b_apple_root/build" --parallel 4
ctest --test-dir "$phase5b_apple_root/build" --output-on-failure

phase5b_llvm_root="$(mktemp -d /tmp/fincore-task36-llvm.XXXXXX)"
cmake -S . -B "$phase5b_llvm_root/build" -G Ninja \
  -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm/bin/clang \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF \
  -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON
cmake --build "$phase5b_llvm_root/build" --parallel 4
ctest --test-dir "$phase5b_llvm_root/build" --output-on-failure

phase5b_implot_root="$(mktemp -d /tmp/fincore-task36-implot.XXXXXX)"
cmake -S . -B "$phase5b_implot_root/build" -G Ninja \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_BUILD_IMPLOT_DEMO=ON
cmake --build "$phase5b_implot_root/build" --parallel 4
ctest --test-dir "$phase5b_implot_root/build" --output-on-failure

phase5b_invalid_root="$(mktemp -d /tmp/fincore-task36-invalid-imgui.XXXXXX)"
cmake -S . -B "$phase5b_invalid_root/build" -G Ninja \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_IMGUI_SOURCE_DIR="$phase5b_invalid_root/not-a-dear-imgui-checkout"
~~~

Record only completed commands: compiler versions, exact CTest totals, warnings, sanitizer diagnostics, ImPlotAdapterCompile registration/demo linking, and the expected invalid-path diagnostic. Do not launch GUI or add a claim of visual validation. Document raw transaction semantics, UTC day normalization, denominator modes and cash distinction, final nonfinite turnover sanitation, and deferred slippage/liquidity/symbol/SID work.

- [ ] **Step 3: Commit initial Task 36 acceptance evidence**

~~~sh
git add include/fincore/metrics.hpp tests/unit/metrics_umbrella_test.cpp \
        README.md \
        docs/acceptance/2026-08-11-phase-5b-transaction-metrics-acceptance.md
git commit -m "docs: accept phase 5B transaction metrics"
~~~

The content records Task 35's completed SHA but does not self-reference this initial acceptance commit.

### Task 37: Record the immutable Task 36 acceptance trace

**Files:**

- Modify: docs/acceptance/2026-08-11-phase-5b-transaction-metrics-acceptance.md

**Consumes:** the completed Task 36 acceptance commit.

**Produces:** a non-self-referential document trace to Task 36's immutable commit SHA.

- [ ] **Step 1: Verify and record the completed initial acceptance SHA**

Confirm the immediate parent is the initial acceptance commit and obtain its full identifier:

~~~sh
git log -1 --format='%H %s'
~~~

Add a concise traceability note beside the Task 35 SHA that records that full Task 36 SHA as the initial acceptance evidence commit and states that the new docs-only trace commit intentionally does not self-reference or amend it.

- [ ] **Step 2: Validate the documentation-only change**

~~~sh
git diff --check
git diff -- docs/acceptance/2026-08-11-phase-5b-transaction-metrics-acceptance.md
~~~

Expected: only the traceability note changes and diff check has no output.

- [ ] **Step 3: Commit the traceability update**

~~~sh
git add docs/acceptance/2026-08-11-phase-5b-transaction-metrics-acceptance.md
git commit -m "docs: trace phase 5B transaction acceptance"
~~~

## Plan self-review

- [x] Task 35 covers all new APIs, output schemas, time normalization, validation, NaN/infinity behavior, denominator rules, CMake registration, public RED/GREEN, and a scoped implementation commit.
- [x] Task 36 covers the public umbrella, actual fresh core/sanitizer/ImPlot evidence, negative dependency gate, exact claims, deferred scope, and the initial acceptance commit.
- [x] Task 37 prevents self-reference while recording the immutable acceptance commit in the tracked evidence document.
- [x] The plan has no placeholder task, unplanned DataFrame helper expansion, string/SID/MultiIndex implementation, dependency fetch, or GUI execution.

## Deferred after this plan

Transaction row mapping and symbols/SIDs; daily bar-data joins; liquidity and liquidation metrics; quadratic and basis-point slippage adjustments; round trips; position grouping/factor exposure; performance attribution; bootstrap/distribution and Bayesian statistics; advanced risk models; optimization; simulation; reports/tearsheets; a full native facade; packaging/release; and interactive ImPlot visual smoke testing remain separate work. Completing this plan is not full fincore parity or project completion.
