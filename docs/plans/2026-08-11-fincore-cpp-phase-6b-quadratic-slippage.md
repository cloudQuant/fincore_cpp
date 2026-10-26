# fincore C++ Phase 6B Quadratic Slippage Penalty Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (- [ ]) syntax for tracking.

**Goal:** Add a DataFrame-native quadratic volume-share slippage penalty for prejoined transaction/bar-volume inputs.

**Architecture:** Extend the accepted transactions module without adding a market-data abstraction. A private row aggregator validates amount/price/bar-volume fields, groups quadratic penalties by UTC day, and a private wealth loop computes the same compounded simulated portfolio-value path as Python cum_returns. The final series follows the validated return-day index only; existing metrics.hpp already exposes transactions.hpp.

**Tech Stack:** C++23, CMake, GoogleTest, cloudQuant/DataFrame, local Python fincore oracle through /Users/yunjinqi/opt/anaconda3/bin/conda run -n base python, Homebrew LLVM 22 for ASan+UBSan, and the existing optional local ImGui/ImPlot compile/link boundary.

## Global Constraints

- Modify only the accepted transactions header/source/test module for implementation. Do not add a market-data class, symbols/SIDs, calendar/dataframe APIs, or CMake target.
- Returns require the named genuine double column and strict ascending unique normalized UTC days. The output uses precisely those normalized return days, not a transaction/return union.
- Transaction-with-bar-data input permits duplicate, unordered, and intraday rows. It requires distinct nonempty genuine double amount, price, and bar-volume columns, all index-length matched; unrelated fields are ignored.
- Keep txn_volume and input bar volume distinct. The public default name for the latter is transaction_bar_volume_column == "volume".
- Define a zero backtest_starting_capital as input_error. Do not finite-validate or clamp simulate_starting_capital or impact.
- Use abs(mult * amount), not mult * abs(amount), then square volume share, multiply impact and simulated dollars, and sum raw daily penalties while skipping only NaN contributions.
- Normalize raw transactions to UTC midnight. Missing or NaN daily penalty becomes zero only after grouping/reindex behavior; signed infinities otherwise remain ordinary IEEE arithmetic.
- For the private wealth path, substitute only NaN returns with zero. An exact zero simulated portfolio value becomes NaN; final adjusted NaN falls back to the original return. Do not globally sanitize infinity.
- Do not modify existing slippage-adjusted-return semantics, positions/scalar/DataFrame upstream, ImPlot adapter/dependencies, or fetch/clone/vendor/fabricate dependencies. Do not execute a GUI.

---

### Task 41: Add native quadratic volume-share slippage penalty

**Files:**

- Modify: include/fincore/metrics/transactions.hpp
- Modify: src/metrics/transactions.cpp
- Modify: tests/unit/transactions_test.cpp

**Consumes:** returns_column and the established return UTC-day validation helper; existing utc_midnight and output helpers in transactions.cpp; public input_error and make_series.

**Produces:**

~~~cpp
inline constexpr std::string_view transaction_bar_volume_column{"volume"};

dataframe apply_slippage_penalty(
    const dataframe& returns,
    const dataframe& transactions_with_bar_data,
    double simulate_starting_capital,
    double backtest_starting_capital,
    double impact = 0.1,
    std::string_view returns_column_name = returns_column,
    std::string_view amount_column_name = transaction_amount_column,
    std::string_view price_column_name = transaction_price_column,
    std::string_view volume_column_name = transaction_bar_volume_column);
~~~

- [ ] **Step 1: Write failing public-header tests**

Add public-header tests in transactions_test.cpp for the exact capacity fixture: returns [1, 1, 1] on Jan 1--3, transactions amount 100000 and price 1 on each day, bar volumes [1000000, 2000000, 3000000], simulated/backtest capital 1000000, impact 1. Assert exact output index, one double returns column, and:

~~~cpp
expect_values(result.get_column<double>("returns"),
              {.9995, .9999375, .9999861111111111});
~~~

Add the required scaling goldens for simulated capital 10000000 with impact 1:
[.95, .99375, .9986111111111111], and impact .1 at equal capitals:
[.99995, .99999375, .9999986111111111].

Add a public unordered duplicate-intraday fixture: return values [.1, .2, NaN,
-1, .3] on Jan 2--6, duplicate Jan 2 raw rows and Jan 3/6 rows, equal 1000
capitals and impact .1. Assert
[.09981818181818182, .1999810606060606, NaN, -1, .3]. The Jan 2 duplicate
penalty must be observable, so a one-row/nonaggregation mutant fails.

Add public tests proving custom return/amount/price/volume names, typed-empty
return output, transaction days outside returns ignored, NaN raw transaction
fields and NaN impact produce original finite returns, an exact-zero portfolio
value falls back to original return, and zero bar volume with finite nonzero
portfolio value produces -infinity.

Add input_error coverage for zero and negative-zero backtest capital; empty,
missing, wrong-typed, malformed, or non-distinct required transaction fields;
empty/missing/wrong-typed/malformed return column; and duplicate/out-of-order
normalized return days. Do not test unimplemented market-data joins, symbols,
or private helpers.

- [ ] **Step 2: Run the RED test**

~~~sh
cmake --build build --parallel 4
ctest --test-dir build -R Transactions --output-on-failure
~~~

Expected: compilation fails because apply_slippage_penalty and
transaction_bar_volume_column are absent from the public transaction header.

- [ ] **Step 3: Implement the smallest native operation**

In transactions.hpp add the constant and signature above. In transactions.cpp
keep all helpers private and implement:

1. a three-column validator that rejects empty or duplicate names and obtains
   amount, price, and volume with require_double_column;
2. an ordered raw penalty map keyed by utc_midnight. For each row compute
   mult = simulated/backtest; shares = abs(mult * amount);
   dollars = price * shares; volume_share = shares/volume;
   penalty = volume_share*volume_share*impact*dollars. Add only a non-NaN
   penalty to its day total;
3. a return-order loop over normalized_return_values. Maintain wealth starting
   at 1.0, multiply it by 1 + (NaN return ? 0 : return), then calculate
   portfolio_value by the ordered operations
   (wealth * backtest_starting_capital) * mult;
4. use zero when the daily penalty is absent or NaN; replace only exact zero
   portfolio value with NaN; calculate adjusted = return - penalty/value; if
   adjusted is NaN, retain the original return; emit the normalized return
   index and caller requested output name.

Do not call get_txn_vol, which has a different two-field daily-volume contract;
do not forward-fill, create output union days, include non-double metadata,
allow zero backtest capital, or use isfinite cleanup.

- [ ] **Step 4: Run focused and regression verification**

~~~sh
cmake --build build --parallel 4
ctest --test-dir build -R 'Transactions|Returns|Dataframe' --output-on-failure
git diff --check
~~~

Expected: selected tests pass and diff check has no output.

- [ ] **Step 5: Commit Task 41**

~~~sh
git add include/fincore/metrics/transactions.hpp \
        src/metrics/transactions.cpp \
        tests/unit/transactions_test.cpp
git commit -m "feat: add quadratic slippage penalty"
~~~

### Task 42: Prove umbrella consumption and accept Phase 6B

**Files:**

- Modify: tests/unit/metrics_umbrella_test.cpp
- Modify: README.md
- Create: docs/acceptance/2026-08-11-phase-6b-quadratic-slippage-acceptance.md

**Consumes:** the Task 41 API re-exported by the existing public metrics umbrella and the public calendar helper.

**Produces:** public-only umbrella coverage plus evidence-backed acceptance for the bounded quadratic penalty surface.

- [ ] **Step 1: Add a public-only umbrella consumer**

Do not include transactions.hpp directly. Use only the current
<fincore/metrics.hpp> and <fincore/calendar.hpp> to create the three-day
capacity fixture and call apply_slippage_penalty. Assert the exact output
schema with both shape().second == 1 and one genuine double column, normalized
return index, and [.9995, .9999375, .9999861111111111] values. Do not remove
accepted public headers to manufacture a new RED.

- [ ] **Step 2: Create acceptance checklist and run fresh gates**

Before invoking tests, create an unchecked acceptance checklist that covers
Task 41 genuine RED, public-only consumer, Python oracle, AppleClang headless,
LLVM 22 ASan+UBSan headless, default ImGui/ImPlot/demo compile/link, invalid
ImGui negative configure, path audit, warning/sanitizer evidence, no GUI
execution, and deferred scope. Run all fresh gates in separate mktemp roots:

~~~sh
phase6b_gtest_source="$PWD/build/_deps/googletest-src"

phase6b_apple_root="$(mktemp -d /tmp/fincore-task42-apple.XXXXXX)"
cmake -S . -B "$phase6b_apple_root/build" -G Ninja \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$phase6b_gtest_source" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build "$phase6b_apple_root/build" --parallel 4
ctest --test-dir "$phase6b_apple_root/build" --output-on-failure

phase6b_llvm_root="$(mktemp -d /tmp/fincore-task42-llvm.XXXXXX)"
cmake -S . -B "$phase6b_llvm_root/build" -G Ninja \
  -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm/bin/clang \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF \
  -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$phase6b_gtest_source" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build "$phase6b_llvm_root/build" --parallel 4
ctest --test-dir "$phase6b_llvm_root/build" --output-on-failure

phase6b_implot_root="$(mktemp -d /tmp/fincore-task42-implot.XXXXXX)"
cmake -S . -B "$phase6b_implot_root/build" -G Ninja \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_BUILD_IMPLOT_DEMO=ON \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$phase6b_gtest_source" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build "$phase6b_implot_root/build" --parallel 4
ctest --test-dir "$phase6b_implot_root/build" --output-on-failure --parallel 4

phase6b_invalid_root="$(mktemp -d /tmp/fincore-task42-invalid-imgui.XXXXXX)"
cmake -S . -B "$phase6b_invalid_root/build" -G Ninja \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_IMGUI_SOURCE_DIR="$phase6b_invalid_root/not-a-dear-imgui-checkout" \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$phase6b_gtest_source" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
~~~

Record only actual completed versions, test counts, warnings, sanitizer
diagnostics, ImPlotAdapterCompile/demo link status, and invalid-path
diagnostic. Update README with the prejoined transaction/bar-volume input,
quadratic formula, return-only index, zero-capital/recovery semantics, and
deferred market-data integration without claiming GUI or full-project
completion.

- [ ] **Step 3: Commit Task 42**

~~~sh
git add tests/unit/metrics_umbrella_test.cpp README.md \
        docs/acceptance/2026-08-11-phase-6b-quadratic-slippage-acceptance.md
git commit -m "docs: accept phase 6B quadratic slippage"
~~~

Record Task 41's full SHA but do not self-reference the initial Task 42
acceptance commit.

### Task 43: Record immutable Task 42 acceptance trace

**Files:**

- Modify: docs/acceptance/2026-08-11-phase-6b-quadratic-slippage-acceptance.md

**Consumes:** completed Task 42 initial acceptance commit.

**Produces:** a documentation-only, non-self-referential trace to that full immutable SHA.

- [ ] **Step 1: Record the initial acceptance evidence SHA**

Confirm the current immediate parent:

~~~sh
git log -1 --format='%H %s'
~~~

Add a note beside Task 41 stating the complete Task 42 initial acceptance SHA,
that this new commit is docs-only, and that it neither self-references nor
amends Task 42.

- [ ] **Step 2: Verify exact documentation scope**

~~~sh
git diff --check
git diff -- docs/acceptance/2026-08-11-phase-6b-quadratic-slippage-acceptance.md
~~~

Expected: one traceability-only documentation change and no whitespace error.

- [ ] **Step 3: Commit Task 43**

~~~sh
git add docs/acceptance/2026-08-11-phase-6b-quadratic-slippage-acceptance.md
git commit -m "docs: trace phase 6B quadratic slippage acceptance"
~~~

## Plan self-review

- [x] Task 41 specifies one bounded API, exact raw quadratic formula, exact
  capacity and duplicate-intraday goldens, structural validation, zero-capital
  decision, NaN/infinity behavior, public RED/GREEN, and a three-file commit.
- [x] Task 42 proves the already-exported umbrella without unnecessary header
  changes and records only completed fresh compiler/sanitizer/graphics/negative
  evidence.
- [x] Task 43 gives immutable Task 42 traceability without self-reference.
- [x] The plan contains no generic market data, symbol/SID, DataFrame/calendar,
  CMake, dependency, GUI, or nonfinite-sanitization expansion.

## Deferred after this plan

daily_txns_with_bar_data; symbols/SIDs and transaction mapping; liquidity and
liquidation metrics; external market-data providers; alternate execution-cost
and market-impact models; round trips; position grouping/factor exposure;
performance attribution; bootstrap/distribution and Bayesian statistics;
advanced-risk models; optimization; simulation; reports/tearsheets; a full
native facade; packaging/release; and interactive ImPlot visual smoke testing
remain separate work. Completing this plan is not full fincore parity or
project completion.
