# fincore C++ Phase 8 Round-Trip Core Implementation Plan

> **For agentic workers:** Use the execution workflow task-by-task. Steps use checkbox syntax for progress tracking.

**Goal:** Add DataFrame-native execution consolidation and FIFO round-trip extraction for the deterministic core of Python "fincore.metrics.round_trips".

**Architecture:** Create one focused "round_trips" metric module. It validates mixed string/double transaction columns locally, stable-sorts each symbol's executions, consolidates direction/time blocks by signed VWAP, then uses the same private row model for FIFO matching. The native result uses mixed cloudQuant DataFrame columns and an ordinal index in place of pandas' RangeIndex.

**Tech Stack:** C++23, cloudQuant/DataFrame, CMake, GoogleTest, the local Python oracle through "/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python", Homebrew LLVM 22 ASan+UBSan, and the optional local ImGui/ImPlot compile/link gate.

## Global constraints

- Use the accepted DataFrame alias and local dependency only. Do not add pandas, a Python runtime, MultiIndex, date/time, logging, or warning dependencies.
- Add a new round-trip metric module; do not modify dataframe.hpp, calendar.hpp, positions, transactions, returns, existing numeric semantics, ImPlot, or dependency settings.
- Required input columns are named nonempty/distinct genuine string symbol, double amount, and double price columns whose lengths match the index. Ignore unrelated columns.
- Group raw timestamps, not calendar days. Sort per symbol by timestamp and retain raw order for equal timestamps. Start a block when "amount > 0" changes or "gap > max_delta"; an exact equal gap stays in the block.
- Preserve ordinary IEEE NaN/infinity arithmetic. An exact zero signed amount sum produces NaN VWAP. Do not filter, clamp, or invent a warning channel.
- Consolidation output preserves requested field names and emits them in Python-equivalent amount, symbol, price order. Its output order is ascending symbol followed by that symbol's blocks.
- Extraction uses FIFO lots per symbol, one result row per closing block that consumes one or more open lots, preserving partial fills and crossing residuals.
- The extracted columns/order are pnl double, open_dt timestamp, close_dt timestamp, long bool, rt_returns double, symbol string, duration timestamp (nanoseconds), with an ordinal {0,1,...} index. Empty valid input/results retain that full typed schema.
- Skip a negative consolidated price without a warning channel, matching Python's warning-and-skip behavior.
- Do not implement portfolio-value-normalized return joins, synthetic closing transactions, sector mappings, dynamic summary tables, attribution, reports, tearsheets, or a GUI.
- All Python oracle commands must use "/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python"; never bare "python" or "python3".

### Task 47: Add typed transaction consolidation and FIFO extraction

**Files:**

- Create: include/fincore/metrics/round_trips.hpp
- Create: src/metrics/round_trips.cpp
- Create: tests/unit/round_trips_test.cpp
- Modify: CMakeLists.txt
- Modify: tests/CMakeLists.txt

**Consumes:** dataframe, timestamp, input_error, and accepted transaction amount/price defaults.

**Produces:**

~~~cpp
inline constexpr std::string_view transaction_symbol_column{"symbol"};
inline constexpr std::string_view round_trip_open_datetime_column{"open_dt"};
inline constexpr std::string_view round_trip_close_datetime_column{"close_dt"};
inline constexpr std::string_view round_trip_duration_column{"duration"};
inline constexpr std::string_view round_trip_pnl_column{"pnl"};
inline constexpr std::string_view round_trip_return_column{"rt_returns"};
inline constexpr std::string_view round_trip_long_column{"long"};

inline constexpr std::chrono::nanoseconds default_round_trip_max_delta{
    std::chrono::hours{8}};

dataframe groupby_consecutive_transactions(
    const dataframe& transactions,
    std::chrono::nanoseconds max_delta = default_round_trip_max_delta,
    std::string_view symbol_column_name = transaction_symbol_column,
    std::string_view amount_column_name = transaction_amount_column,
    std::string_view price_column_name = transaction_price_column);

dataframe extract_round_trips(
    const dataframe& transactions,
    std::chrono::nanoseconds max_delta = default_round_trip_max_delta,
    std::string_view symbol_column_name = transaction_symbol_column,
    std::string_view amount_column_name = transaction_amount_column,
    std::string_view price_column_name = transaction_price_column);
~~~

- [ ] **Step 1: Record the Python oracle and add public-header RED tests**

Create the new unit test and list it in tests/CMakeLists.txt. It includes only the new public round_trips header among fincore headers. Before production edits, use the required Anaconda command to record the Python results in the ignored phase evidence ledger.

Use an interleaved UTC fixture:

~~~text
2024-01-02 09:00 A  +2 @ 10
2024-01-02 11:00 A  +2 @ 20
2024-01-02 13:00 A  -3 @ 25
2024-01-02 15:00 A  -1 @  5
2024-01-02 10:00 B  -2 @ 30
2024-01-02 12:00 B  +2 @ 20
~~~

The default consolidated result is:

~~~text
index  = [A@09:00, A@13:00, B@10:00, B@12:00]
amount = [4, -4, -2, 2]
symbol = [A, A, B, B]
price  = [15, 20, 30, 20]
~~~

The extracted result is:

~~~text
index      = [0, 1]
pnl        = [20, 20]
open_dt    = [A@09:00, B@10:00]
close_dt   = [A@13:00, B@12:00]
long       = [true, false]
rt_returns = [1/3, 1/3]
symbol     = [A, B]
duration   = [4h, 2h]
~~~

Add public tests for all required boundaries:

- an exact eight-hour same-direction gap consolidates; a gap one nanosecond greater splits; exact timestamp ties retain raw input order;
- the three-day crossing "+2 @10, -4 @15, +3 @20" emits pnl [10, -10], long [true, false], and returns [.5, -1/3]; a partial-fill fixture emits one row per close;
- zero-invested output returns zero; a negative price emits no completed event; one-direction-only input yields typed-empty extracted schema;
- typed-empty valid inputs, custom field names, zero-summed VWAP, and ordinary NaN/infinity propagation;
- missing/wrong-type/malformed columns and empty or duplicate field names throw input_error; unrelated metadata is ignored.

Assert all output names, order, index, and types explicitly. Retrieve string/bool/timestamp columns through their concrete native types; do not call private helpers or test deferred functions.

- [ ] **Step 2: Run the RED test**

~~~sh
cmake --build build --parallel 4
ctest --test-dir build -R RoundTrips --output-on-failure
~~~

Expected: compilation fails only because the public round_trips header and declarations do not exist; no focused test binary is generated.

- [ ] **Step 3: Implement the smallest native core**

In the header include chrono, string_view, and the existing transactions header, then declare the exact public surface above.

In the source, keep all helpers private:

1. Validate names before accessing any columns. Build a local genuine-string validator using DataFrame type metadata and get_column<std::string>(); use require_double_column for amount/price.
2. Form raw rows with symbol, amount, price, timestamp, and raw position. Stable-sort by symbol, timestamp, and raw position. Compare timestamp gaps without signed overflow and apply the strict comparison.
3. Aggregate blocks while preserving their first timestamp/symbol, summing signed amount, and calculating signed VWAP. Load amount/symbol/price with dont_pad_with_nans.
4. Reuse the blocks for FIFO matching. A queued lot contains signed price, absolute residual quantity, and opening timestamp. An opposite block consumes queue-front lots, accumulates documented pnl and invested amounts, and appends a crossing residual as a new lot.
5. Load ordinal index and all seven result columns in the documented exact order/types. Compute nanosecond duration and do not sanitize regular floating arithmetic. Silently skip negative consolidated price.

Add the source to the fincore library only after the RED observation. Do not alter shared helpers or accepted metric modules.

- [ ] **Step 4: Run focused and regression verification**

~~~sh
cmake --build build --parallel 4
ctest --test-dir build -R 'RoundTrips|Transactions|Positions|Dataframe' --output-on-failure
ctest --test-dir build --output-on-failure
git diff --check
~~~

Expected: focused/full headless CTest pass and diff check is silent.

- [ ] **Step 5: Commit Task 47**

~~~sh
git add CMakeLists.txt tests/CMakeLists.txt \
        include/fincore/metrics/round_trips.hpp \
        src/metrics/round_trips.cpp tests/unit/round_trips_test.cpp
git commit -m "feat: add native round-trip extraction"
~~~

### Task 48: Prove public façade consumption and accept Phase 8

**Files:**

- Modify: include/fincore/metrics.hpp
- Modify: tests/unit/metrics_umbrella_test.cpp
- Modify: README.md
- Create: docs/acceptance/2026-08-11-phase-8-round-trip-core-acceptance.md

**Consumes:** Task 47 implementation and its full SHA.

- [ ] **Step 1: Add a public-only umbrella consumer**

Use only metrics.hpp and calendar.hpp in the existing umbrella test, never the direct round_trips header. Create a long/short fixture and call both public APIs. Assert default eight-hour behavior, expected ordinal index, exact first FIFO result, and concrete string/bool/timestamp column types. Add only round_trips.hpp to the public metric umbrella.

- [ ] **Step 2: Create acceptance checklist and run fresh gates**

Before verification, write an unchecked acceptance checklist covering: Task 47 RED, Python oracle, focused/full test results, umbrella-only consumer, source/path audit, AppleClang headless, Homebrew LLVM 22 ASan+UBSan, default ImGui/ImPlot/demo compile/link, invalid ImGui negative configure, warnings/sanitizer evidence, no GUI execution, and deferred scope.

Run all fresh configurations in separate mktemp roots using the existing local GoogleTest source and FETCHCONTENT_FULLY_DISCONNECTED=ON:

~~~sh
phase8_gtest_source="$PWD/build/_deps/googletest-src"

phase8_apple_root="$(mktemp -d /tmp/fincore-task48-apple.XXXXXX)"
cmake -S . -B "$phase8_apple_root/build" -G Ninja \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$phase8_gtest_source" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build "$phase8_apple_root/build" --parallel 4
ctest --test-dir "$phase8_apple_root/build" --output-on-failure

phase8_llvm_root="$(mktemp -d /tmp/fincore-task48-llvm.XXXXXX)"
cmake -S . -B "$phase8_llvm_root/build" -G Ninja \
  -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm/bin/clang \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF \
  -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$phase8_gtest_source" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build "$phase8_llvm_root/build" --parallel 4
ctest --test-dir "$phase8_llvm_root/build" --output-on-failure

phase8_implot_root="$(mktemp -d /tmp/fincore-task48-implot.XXXXXX)"
cmake -S . -B "$phase8_implot_root/build" -G Ninja \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_BUILD_IMPLOT_DEMO=ON \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$phase8_gtest_source" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build "$phase8_implot_root/build" --parallel 4
ctest --test-dir "$phase8_implot_root/build" --output-on-failure --parallel 4

phase8_invalid_root="$(mktemp -d /tmp/fincore-task48-invalid-imgui.XXXXXX)"
cmake -S . -B "$phase8_invalid_root/build" -G Ninja \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_IMGUI_SOURCE_DIR="$phase8_invalid_root/not-a-dear-imgui-checkout" \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$phase8_gtest_source" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
~~~

Record actual completed commands, compiler versions, test counts, warnings, sanitizer diagnostics, demo/ImPlotAdapterCompile link evidence, and the expected invalid-path diagnostic. Update README with only accepted core behavior and deferred scope; do not claim GUI execution, full parity, or release readiness.

- [ ] **Step 3: Commit Task 48**

~~~sh
git add include/fincore/metrics.hpp tests/unit/metrics_umbrella_test.cpp \
        README.md docs/acceptance/2026-08-11-phase-8-round-trip-core-acceptance.md
git commit -m "docs: accept phase 8 round-trip core"
~~~

Record Task 47's full SHA but do not self-reference the initial Task 48 acceptance commit.

### Task 49: Record immutable Task 48 acceptance trace

**Files:**

- Modify: docs/acceptance/2026-08-11-phase-8-round-trip-core-acceptance.md

**Consumes:** the completed Task 48 initial acceptance commit.

- [ ] **Step 1: Add the initial acceptance SHA**

Confirm the immediate parent using "git log -1 --format='%H %s'". Record that full SHA beside Task 47's trace. State that this follow-up is documentation-only, does not amend Task 48, and does not self-reference.

- [ ] **Step 2: Verify scope and commit**

~~~sh
git diff --check
git diff -- docs/acceptance/2026-08-11-phase-8-round-trip-core-acceptance.md
git add docs/acceptance/2026-08-11-phase-8-round-trip-core-acceptance.md
git commit -m "docs: trace phase 8 round-trip core acceptance"
~~~

## Plan self-review

- [x] The plan uses local DataFrame mixed-column support instead of an adapter or generic data model.
- [x] Schemas, ordering, VWAP, FIFO, crossing, typed-empty, and IEEE boundaries are explicit and testable.
- [x] Existing modules remain out of scope, preventing a refactor spillover.
- [x] Acceptance requires independent compiler/sanitizer/optional-visualization evidence without a GUI run.

## Deferred after this plan

Portfolio-value-normalized round-trip returns; synthetic closing transactions; sector mappings and native round-trip summary tables; transaction/SID and market-data joins; performance attribution; Bayesian/bootstrap and advanced-risk analytics; optimization; simulation; reports/tearsheets; a dynamic Python façade; packaging/release; and interactive ImPlot visual smoke testing remain separate work. Completing this plan is not whole-project fincore parity or project completion.
