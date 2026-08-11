# fincore C++ Phase 6A Slippage-Adjusted Returns Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (- [ ]) syntax for tracking.

**Goal:** Add a DataFrame-native basis-point slippage adjustment that composes daily transaction volume with daily returns and positions.

**Architecture:** Extend the established transactions module only. The new public function reuses get_txn_vol for raw-transaction validation and aggregation, normalizes return and position timestamps to strict UTC-day series, then implements the Python Series formula over the ordered union of all three input day sets. The existing public umbrella already exports transactions.hpp; acceptance proves that route rather than adding a redundant include.

**Tech Stack:** C++23, CMake, GoogleTest, cloudQuant/DataFrame, local Python fincore oracle through /Users/yunjinqi/opt/anaconda3/bin/conda run -n base python, Homebrew LLVM 22 for ASan+UBSan, and the existing optional local ImGui/ImPlot compile/link boundary.

## Global Constraints

- Extend only the accepted transactions.hpp / transactions.cpp public module; do not add a generic pandas compatibility layer, a new DataFrame/calendar API, or a new CMake target.
- Returns require a named genuine double column and positions select every genuine double holding in insertion order; selected values must match their frame indexes, while non-double metadata is ignored.
- Normalize return, position, and transaction timestamps to UTC midnight. Return and position day inputs must be strictly increasing and unique after normalization; raw transaction rows may be duplicate, intraday, and unordered.
- Reuse get_txn_vol for amount/price validation and daily transaction volume. Do not duplicate or weaken its raw-transaction rules.
- The result has exactly one genuine double column named by returns_column_name and uses the sorted union of return, position, and transaction days.
- Match the Python formula precisely: position sums skip NaN; adjusted_pnl uses pnl.add(-slippage_dollars, fill_value=0) semantics; zero PnL becomes a NaN denominator; final output replaces NaN only with 0.0 and preserves computed infinities.
- Do not validate, clamp, or sanitize slippage_bps; preserve ordinary IEEE behavior for negative, NaN, and infinite input.
- Do not alter existing positions, scalar metrics, DataFrame upstream, ImPlot adapter/dependencies, or fetch/clone/vendor/fabricate dependencies. Do not execute a GUI.

---

### Task 38: Add native basis-point slippage-adjusted returns

**Files:**

- Modify: include/fincore/metrics/transactions.hpp
- Modify: src/metrics/transactions.cpp
- Modify: tests/unit/transactions_test.cpp

**Consumes:** returns_column from the existing returns public header, Phase 5A numeric-position selection semantics, the private UTC-midnight/position helpers in transactions.cpp, and the existing public get_txn_vol operation.

**Produces:**

~~~cpp
dataframe adjust_returns_for_slippage(
    const dataframe& returns,
    const dataframe& positions,
    const dataframe& transactions,
    double slippage_bps,
    std::string_view returns_column_name = returns_column,
    std::string_view amount_column_name = transaction_amount_column,
    std::string_view price_column_name = transaction_price_column);
~~~

- [ ] **Step 1: Write failing public-header tests**

Add tests to transactions_test.cpp, which already includes the public transaction header, for the following exact daily fixture. Returns named returns are [.1, .2, NaN, .3] on Jan 2, 3, 5, and 6. Position rows on Jan 2, 4, and 5 have double asset/cash [100,900], [200,800], and [300,700], plus an int metadata field. Raw intraday transactions on Jan 2, 3, 4, and 5 each have abs(amount) * price == 100. At 10.0 bps, assert the UTC-midnight ascending index Jan 2--6, exactly one double returns column, and values [.0999, 0, 0, 0, 0]:

~~~cpp
const auto adjusted = fincore::adjust_returns_for_slippage(
    returns, positions, transactions, 10.0);
EXPECT_EQ(adjusted.get_index(), expected_days);
expect_double_columns(adjusted, {"returns"});
expect_values(adjusted.get_column<double>("returns"),
              {.0999, 0.0, 0.0, 0.0, 0.0});
~~~

Add public tests proving custom return/amount/price names preserve the custom return output name; a typed-empty return/position/transaction triple produces an empty typed return column; and an empty return frame with valid nonempty position/transaction days produces sorted union-day zeros.

Add arithmetic boundary tests that assert: zero PnL produces 0.0 despite a finite transaction penalty; NaN raw transaction amount leaves a valid return unpenalized; slippage_bps=NaN leaves a finite valid return unpenalized via fill_value=0; and a finite nonzero return/PnL paired with an infinite daily transaction volume produces -infinity, not zero.

Add input_error cases for empty/missing/wrong-typed/malformed return column, no genuine-double or malformed position columns, duplicate or out-of-order normalized return/position days, and every invalid amount/price condition already required by get_txn_vol. Do not call private helpers or add tests for unimplemented liquidity/mapping functionality.

- [ ] **Step 2: Run the RED test**

~~~sh
cmake --build build --parallel 4
ctest --test-dir build -R Transactions --output-on-failure
~~~

Expected: compilation fails only because adjust_returns_for_slippage is not declared by the accepted public transaction header.

- [ ] **Step 3: Implement the smallest native operation**

In transactions.hpp, include the public returns header and declare the signature above. In transactions.cpp, retain every helper private and:

1. obtain return values with require_double_column, normalize each return timestamp with the existing utc_midnight, and reject duplicate/nonascending normalized return days;
2. reuse numeric_position_columns plus strict normalized position days and create daily portfolio values with sum_skipping_nan;
3. call get_txn_vol(transactions, amount_column_name, price_column_name), extract its txn_volume, and represent its existing sorted daily values by day;
4. build an ordered std::map timestamp union of return, position, and transaction days; compute pnl only where return and position values both exist; implement the exact NaN-aware two-operand fill_value=0 behavior for adjusted PnL; replace only an exact zero PnL with NaN before division;
5. calculate return * adjusted_pnl / pnl_safe, replacing only a final NaN with 0.0, and return make_series with returns_column_name.

Do not add an isfinite final cleanup, a cash requirement, a non-double position inclusion, an implicit date forward-fill, or a slippage_bps validation.

- [ ] **Step 4: Run focused and regression verification**

~~~sh
cmake --build build --parallel 4
ctest --test-dir build -R 'Transactions|Positions|Returns|Dataframe' --output-on-failure
git diff --check
~~~

Expected: all selected tests pass and diff check has no output.

- [ ] **Step 5: Commit Task 38**

~~~sh
git add include/fincore/metrics/transactions.hpp \
        src/metrics/transactions.cpp \
        tests/unit/transactions_test.cpp
git commit -m "feat: add native slippage-adjusted returns"
~~~

### Task 39: Prove umbrella consumption and accept the Phase 6A surface

**Files:**

- Modify: tests/unit/metrics_umbrella_test.cpp
- Modify: README.md
- Create: docs/acceptance/2026-08-11-phase-6a-slippage-adjustment-acceptance.md

**Consumes:** Task 38 public transaction header already re-exported by <fincore/metrics.hpp> and the existing public UTC calendar helper.

**Produces:** public-only consumer coverage and evidence-backed acceptance material for the bounded slippage-adjusted return surface.

- [ ] **Step 1: Add an umbrella-only consumer**

In metrics_umbrella_test.cpp, include no transaction-specific header. Use only the existing <fincore/metrics.hpp> plus <fincore/calendar.hpp> to construct the Jan 2--6 fixture, call:

~~~cpp
const auto adjusted = fincore::adjust_returns_for_slippage(
    returns, positions, transactions, 10.0);
~~~

Assert the public result's one-column schema, UTC-midnight index, and [.0999, 0, 0, 0, 0] values. Do not remove accepted headers to manufacture a new RED: Task 38's missing-declaration compilation failure is the genuine public API RED evidence.

- [ ] **Step 2: Prepare evidence-first acceptance material and run fresh gates**

Before the fresh builds, create an unchecked acceptance checklist covering the Task 38 RED, public-only umbrella use, Python-oracle fixture, AppleClang core, Homebrew LLVM ASan+UBSan, positive default ImGui/ImPlot/demo compile boundary, invalid ImGui negative configure, path audit, warnings, no GUI execution, and deferred scope. Then run each fresh configuration in its own mktemp -d root, setting the existing local GoogleTest source and disconnect policy:

~~~sh
phase6a_gtest_source="$PWD/build/_deps/googletest-src"

phase6a_apple_root="$(mktemp -d /tmp/fincore-task39-apple.XXXXXX)"
cmake -S . -B "$phase6a_apple_root/build" -G Ninja \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$phase6a_gtest_source" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build "$phase6a_apple_root/build" --parallel 4
ctest --test-dir "$phase6a_apple_root/build" --output-on-failure

phase6a_llvm_root="$(mktemp -d /tmp/fincore-task39-llvm.XXXXXX)"
cmake -S . -B "$phase6a_llvm_root/build" -G Ninja \
  -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm/bin/clang \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF \
  -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$phase6a_gtest_source" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build "$phase6a_llvm_root/build" --parallel 4
ctest --test-dir "$phase6a_llvm_root/build" --output-on-failure

phase6a_implot_root="$(mktemp -d /tmp/fincore-task39-implot.XXXXXX)"
cmake -S . -B "$phase6a_implot_root/build" -G Ninja \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_BUILD_IMPLOT_DEMO=ON \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$phase6a_gtest_source" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build "$phase6a_implot_root/build" --parallel 4
ctest --test-dir "$phase6a_implot_root/build" --output-on-failure --parallel 4

phase6a_invalid_root="$(mktemp -d /tmp/fincore-task39-invalid-imgui.XXXXXX)"
cmake -S . -B "$phase6a_invalid_root/build" -G Ninja \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_IMGUI_SOURCE_DIR="$phase6a_invalid_root/not-a-dear-imgui-checkout" \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$phase6a_gtest_source" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
~~~

Record only completed commands: compiler versions, exact CTest totals, warnings, sanitizer diagnostics, ImPlotAdapterCompile registration/demo link, and the expected invalid-path diagnostic. Update README with the new API and the return/position/transaction UTC-union, NaN-only final-fill, and infinity-preservation contract. Narrow deferred scope from general basis-point slippage to future market-impact/liquidity models without claiming project completion.

- [ ] **Step 3: Commit Task 39 acceptance evidence**

~~~sh
git add tests/unit/metrics_umbrella_test.cpp README.md \
        docs/acceptance/2026-08-11-phase-6a-slippage-adjustment-acceptance.md
git commit -m "docs: accept phase 6A slippage adjustment"
~~~

The content records Task 38's completed SHA but does not self-reference this initial acceptance commit.

### Task 40: Record the immutable Task 39 acceptance trace

**Files:**

- Modify: docs/acceptance/2026-08-11-phase-6a-slippage-adjustment-acceptance.md

**Consumes:** the completed Task 39 acceptance commit.

**Produces:** a non-self-referential document trace to Task 39's immutable initial acceptance evidence commit.

- [ ] **Step 1: Verify and record the initial acceptance SHA**

Confirm the immediate parent and obtain the full SHA:

~~~sh
git log -1 --format='%H %s'
~~~

Add a concise traceability note beside the Task 38 SHA. It must name the full Task 39 initial acceptance SHA, state that the new commit is docs-only, and state that it neither self-references nor amends Task 39.

- [ ] **Step 2: Verify that only traceability changed**

~~~sh
git diff --check
git diff -- docs/acceptance/2026-08-11-phase-6a-slippage-adjustment-acceptance.md
~~~

Expected: only the traceability note changes and git diff --check has no output.

- [ ] **Step 3: Commit Task 40**

~~~sh
git add docs/acceptance/2026-08-11-phase-6a-slippage-adjustment-acceptance.md
git commit -m "docs: trace phase 6A slippage acceptance"
~~~

## Plan self-review

- [x] Task 38 has one public API, exact date-union and arithmetic semantics, typed-empty/custom-name behavior, Python oracle values, structural errors, a genuine public-header RED, focused regression verification, and a scoped implementation commit.
- [x] Task 39 proves the already-exported umbrella without a redundant header change, records only completed fresh core/sanitizer/ImPlot/negative-gate evidence, documents the bounded surface, and avoids a GUI claim.
- [x] Task 40 records the immutable initial acceptance SHA without a self-reference or amend.
- [x] The plan has no placeholder task, CMake/dataframe/calendar expansion, hidden date forward-fill, isfinite cleanup, Python dependency fetch, or unplanned market-impact/liquidity implementation.

## Deferred after this plan

Transaction mapping and symbols/SIDs; daily bar-data joins; market-impact, liquidity, and liquidation models; round trips; position grouping/factor exposure; performance attribution; bootstrap/distribution and Bayesian statistics; advanced risk models; optimization; simulation; reports/tearsheets; a full native facade; packaging/release; and interactive ImPlot visual smoke testing remain separate work. Completing this plan is not full fincore parity or project completion.
