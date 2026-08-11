# fincore C++ Phase 7 Return Transformations and Calendar Aggregation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (- [ ]) syntax for tracking.

**Goal:** Add DataFrame-native Python-compatible starting-value cumulative returns, normalization, and calendar return aggregation with explicit native grouping labels.

**Architecture:** Extend only the existing returns module. Preserve the accepted zero-starting-value overloads and add distinct numeric overloads, then implement normalize as a single-series transform. Model Python aggregation's one-level/MultiIndex output as one DataFrame containing a real UTC bucket-start index plus integer year/period label columns and the caller-requested compounded-return column. Keep C++23 chrono grouping private to returns.cpp.

**Tech Stack:** C++23, cloudQuant/DataFrame, CMake, GoogleTest, the local Python fincore oracle through /Users/yunjinqi/opt/anaconda3/bin/conda run -n base python, Homebrew LLVM 22 ASan+UBSan, and the existing optional local ImGui/ImPlot compile/link gate.

## Global Constraints

- Keep dataframe as hmdf::StdDataFrame<timestamp> and use only the local cloudQuant/DataFrame source; add no pandas, Python runtime, third-party date, or MultiIndex dependency.
- Keep the old cumulative-return overloads source-compatible and semantically unchanged. Numeric starting-value overloads must be distinct from input-column overloads.
- Add return_aggregation_period only in returns.hpp, with weekly, monthly, quarterly, and yearly values. Invalid enum values must throw input_error; daily aggregation is deliberately not an alias.
- aggregate_returns must represent Python labels with output integer columns year and period, followed by the requested output double column. Reject an input column name that collides with either reserved label column.
- Aggregation consumes raw input timestamps as UTC; unordered, duplicate, and intraday observations are legal. Preserve their original order within each group and emit groups sorted lexicographically by (year, period).
- Weekly grouping must be exactly Python's (observation calendar year, ISO week number), not ISO year/week. The weekly index is the ISO Monday clamped to January 1 of the label year.
- Compound only NaN return observations as zero. Do not finite-filter or globally sanitize infinity in cumulative or aggregation paths.
- normalize preserves the requested input-column name, original index, ordinary IEEE propagation, and maps an exact +0.0 or -0.0 first value to a full NaN output without a warning channel.
- Do not change calendar.hpp, basic.hpp, DataFrame aliases, CMake/dependency settings, positions/transactions/yearly semantics, ImPlot adapter, or any previously accepted public API. Do not launch a GUI.
- All Python oracle commands must use /Users/yunjinqi/opt/anaconda3/bin/conda run -n base python; do not invoke bare python or python3.

---

### Task 44: Add native return transformations and calendar aggregation

**Files:**

- Modify: include/fincore/metrics/returns.hpp
- Modify: src/metrics/returns.cpp
- Modify: tests/unit/returns_test.cpp

**Consumes:** dataframe, timestamp, make_series, require_double_column, input_error, and public utc_timestamp; the existing Python fincore.metrics.returns oracle.

**Produces:**

~~~cpp
inline constexpr std::string_view return_aggregation_year_column{"year"};
inline constexpr std::string_view return_aggregation_period_column{"period"};

enum class return_aggregation_period {
    weekly,
    monthly,
    quarterly,
    yearly,
};

dataframe cumulative_returns(const dataframe& returns,
                             double starting_value,
                             std::string_view column = returns_column);

double cumulative_return(const dataframe& returns,
                         double starting_value,
                         std::string_view column = returns_column);

dataframe normalize(const dataframe& values,
                    double starting_value = 1.0,
                    std::string_view column = returns_column);

dataframe aggregate_returns(
    const dataframe& returns,
    return_aggregation_period period = return_aggregation_period::monthly,
    std::string_view column = returns_column);
~~~

- [ ] **Step 1: Write exact public-header RED tests and record Python oracle**

Extend tests/unit/returns_test.cpp, which includes only
<fincore/metrics/returns.hpp> among fincore headers. Add compile-visible tests
for all new declarations before production edits. Use the required Anaconda
command to record the following oracle values:

~~~sh
/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python -c '...'
~~~

For returns [.1, NaN, -.1], assert the new nonzero-starting forms:

~~~cpp
const auto returns = fincore::make_series({1, 2, 3}, "returns",
    {.1, std::numeric_limits<double>::quiet_NaN(), -.1});
expect_values(fincore::cumulative_returns(returns, 100.0)
                  .get_column<double>("returns"),
              {110.0, 110.0, 99.0});
EXPECT_NEAR(fincore::cumulative_return(returns, 100.0), 99.0, 1e-12);
expect_values(fincore::cumulative_returns(returns, -2.0)
                  .get_column<double>("returns"),
              {-2.2, -2.2, -1.98});
EXPECT_NEAR(fincore::cumulative_return(returns, -2.0), -1.98, 1e-12);
~~~

Keep assertions that the original no-number overload still emits pure returns
[.1, .1, -.01] and scalar -.01. Assert empty nonzero frame output is
typed-empty and scalar output remains NaN.

Add normalization tests:

~~~cpp
const auto values = fincore::make_series({11, 12, 13}, "equity",
    {100.0, 110.0, 90.0});
expect_values(fincore::normalize(values, -2.0, "equity")
                  .get_column<double>("equity"),
              {-2.0, -2.2, -1.8});
~~~

Assert same index/name, default starting value [1, 1.1, .9], typed-empty
custom-column output, and full-NaN output for both {0, 10, NaN} and
{-0.0, 10, NaN}. Pin ordinary infinity propagation with a nonzero base:
{2, infinity, 4} normalizes to {1, infinity, 2}.

Use this nine-observation UTC fixture for all aggregation periods:

~~~cpp
const auto returns = fincore::make_series(
    {utc_timestamp(2020, 12, 28, 15), utc_timestamp(2020, 12, 31, 9),
     utc_timestamp(2021, 1, 1, 11), utc_timestamp(2021, 1, 3, 18),
     utc_timestamp(2021, 1, 4, 8), utc_timestamp(2021, 3, 31, 12),
     utc_timestamp(2021, 4, 1, 12), utc_timestamp(2021, 12, 31, 12),
     utc_timestamp(2022, 1, 1, 12)},
    "strategy", {.1, .02, .03, nan, -.1, .1, .2, .01, .02});
~~~

For weekly aggregation, assert exact int labels and values:

~~~text
year   = {2020, 2021, 2021, 2021, 2021, 2022}
period = {53,   1,    13,   52,   53,   52}
value  = {.122, -.1, .32, .01, .03, .02}
index  = {utc_timestamp(2020, 12, 28), utc_timestamp(2021, 1, 4),
          utc_timestamp(2021, 3, 29), utc_timestamp(2021, 12, 27),
          utc_timestamp(2021, 1, 1), utc_timestamp(2022, 1, 1)}
~~~

This intentionally proves label sorting rather than global timestamp sorting
at the (2021, 52) / (2021, 53) boundary. Assert shape().second == 3,
the exact column order {year, period, strategy}, and genuine types
int, int, and double.

Also assert:

~~~text
monthly labels/values:
(2020,12)=.122, (2021,1)=-.073, (2021,3)=.1,
(2021,4)=.2, (2021,12)=.01, (2022,1)=.02

quarterly labels/values:
(2020,4)=.122, (2021,1)=.0197, (2021,2)=.2,
(2021,4)=.01, (2022,1)=.02

yearly labels/values:
(2020,1)=.122, (2021,1)=.2358764, (2022,1)=.02
~~~

Add one unordered/intraday/duplicate input fixture that asserts legal input and
sorted label output, one +infinity group fixture that returns +infinity, and
typed-empty aggregation output with the three exact typed columns.

Add input_error coverage for missing, wrong-typed, malformed, and empty input
columns; year/period reserved-name collision; and
static_cast<return_aggregation_period>(999). Do not use private helpers or
test unsupported pandas MultiIndex, local time zones, reports, or plotting.

- [ ] **Step 2: Run the RED test**

Run:

~~~sh
cmake --build build --parallel 4
ctest --test-dir build -R Returns --output-on-failure
~~~

Expected: compilation fails only because return_aggregation_period, normalize,
aggregate_returns, and the numeric cumulative overloads are not declared in
the public returns header.

- [ ] **Step 3: Implement the smallest native operation**

In returns.hpp, add the two constants, enum, and overloads exactly as shown.
Do not alter existing declarations.

In returns.cpp:

1. Add a private compound_wealth(std::span<const double>) helper that begins
   at 1.0, substitutes only NaN return observations with 0.0, and leaves all
   other IEEE arithmetic intact. Keep existing no-number overloads routed
   through the same zero-start transformation so their values and fixed
   returns_column output name do not change.
2. Implement numeric cumulative frame/scalar overloads with
   starting_value == 0.0 ? wealth - 1.0 : wealth * starting_value; retain
   empty typed frame and NaN scalar behavior.
3. Implement normalize with require_double_column. If empty, return
   make_series with the requested name and empty values. If first value is
   exactly zero, fill a same-length vector with quiet NaN; otherwise calculate
   starting_value * value / first element by element.
4. Add private C++23 chrono helpers that floor a timestamp to UTC day, obtain
   calendar year/month/day, calculate ISO weekday/week number, and create a
   canonical bucket-start timestamp through utc_timestamp. ISO-week number
   must use the observation's calendar year for its first label and the
   standard 52/53-week rule for its second label.
5. Add a private comparable (year, period) key and group state with
   bucket_start plus wealth. Iterate input index/value positions in original
   order; lookup/create the key in std::map, then compound its wealth. Resolve
   buckets as weekly calendar-year plus ISO-week, monthly month, quarterly
   (month - 1) / 3 + 1, and yearly period 1.
6. Reject the two reserved input names before loading output, and reject an
   unknown enum with input_error. Emit map rows in key order by manually
   loading one index, int year, int period, and requested-name double result
   column with dont_pad_with_nans.

No input-order constraint, date normalization API change, duplicate filtering,
finite cleanup, or CMake change is permitted.

- [ ] **Step 4: Run focused and regression verification**

Run:

~~~sh
cmake --build build --parallel 4
ctest --test-dir build -R 'Returns|Dataframe|Yearly|MetricsUmbrella' --output-on-failure
ctest --test-dir build --output-on-failure
git diff --check
~~~

Expected: focused and complete headless CTest pass and diff check has no
output.

- [ ] **Step 5: Commit Task 44**

Run:

~~~sh
git add include/fincore/metrics/returns.hpp \
        src/metrics/returns.cpp \
        tests/unit/returns_test.cpp
git commit -m "feat: add native return transformations"
~~~

### Task 45: Prove umbrella consumption and accept Phase 7

**Files:**

- Modify: tests/unit/metrics_umbrella_test.cpp
- Modify: README.md
- Create: docs/acceptance/2026-08-11-phase-7-return-transformations-acceptance.md

**Consumes:** Task 44's existing metrics.hpp re-export and public calendar.hpp.

**Produces:** a public-only consumer regression and evidence-backed Phase 7
acceptance record.

- [ ] **Step 1: Add a public-only umbrella consumer**

Do not include returns.hpp directly. Use only <fincore/metrics.hpp> and
<fincore/calendar.hpp> among fincore headers. Construct the cross-year fixture
with utc_timestamp, then call all of:

~~~cpp
fincore::cumulative_returns(returns, 100.0, "strategy");
fincore::cumulative_return(returns, 100.0, "strategy");
fincore::normalize(values, 1.0, "equity");
fincore::aggregate_returns(
    returns, fincore::return_aggregation_period::weekly, "strategy");
~~~

Assert public constants, output index, exact values, exact three-column
aggregation schema, and integer/double genuine column types. Do not remove a
previously accepted header to manufacture a second RED.

- [ ] **Step 2: Create an unchecked acceptance checklist and run fresh gates**

Before running fresh commands, create the acceptance record with unchecked
items for Task 44 public-header RED, Python oracle, public-only consumer,
AppleClang headless, Homebrew LLVM 22 ASan+UBSan headless, default
ImGui/ImPlot/demo compile/link, invalid ImGui configure, exact owned-path
audit, warnings/sanitizer evidence, no GUI execution, and deferred scope.

Use separate mktemp -d roots and the pre-existing local GoogleTest source:

~~~sh
phase7_gtest_source="$PWD/build/_deps/googletest-src"

phase7_apple_root="$(mktemp -d /tmp/fincore-task45-apple.XXXXXX)"
cmake -S . -B "$phase7_apple_root/build" -G Ninja \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$phase7_gtest_source" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build "$phase7_apple_root/build" --parallel 4
ctest --test-dir "$phase7_apple_root/build" --output-on-failure

phase7_llvm_root="$(mktemp -d /tmp/fincore-task45-llvm.XXXXXX)"
cmake -S . -B "$phase7_llvm_root/build" -G Ninja \
  -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm/bin/clang \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF \
  -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$phase7_gtest_source" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build "$phase7_llvm_root/build" --parallel 4
ctest --test-dir "$phase7_llvm_root/build" --output-on-failure

phase7_implot_root="$(mktemp -d /tmp/fincore-task45-implot.XXXXXX)"
cmake -S . -B "$phase7_implot_root/build" -G Ninja \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_BUILD_IMPLOT_DEMO=ON \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$phase7_gtest_source" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
cmake --build "$phase7_implot_root/build" --parallel 4
ctest --test-dir "$phase7_implot_root/build" --output-on-failure --parallel 4

phase7_invalid_root="$(mktemp -d /tmp/fincore-task45-invalid-imgui.XXXXXX)"
cmake -S . -B "$phase7_invalid_root/build" -G Ninja \
  -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
  -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=ON \
  -DFINCORE_IMGUI_SOURCE_DIR="$phase7_invalid_root/not-a-dear-imgui-checkout" \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$phase7_gtest_source" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON
~~~

Record only completed versions, exact test counts, warnings, sanitizer
diagnostics, ImPlotAdapterCompile registration/demo link status, and the
invalid-path diagnostic. Do not fetch, clone, vendor, fabricate, or substitute
dependencies; do not run a GUI.

Update README with the native start-value, normalization, and aggregation
representation, especially its explicit year/period labels, calendar-year plus
ISO-week rule, output-order caveat, and deferred pandas MultiIndex/local
timezone/full-project scope. Include Task 44 full SHA but do not
self-reference the initial Task 45 acceptance commit.

- [ ] **Step 3: Commit Task 45**

Run:

~~~sh
git add tests/unit/metrics_umbrella_test.cpp README.md \
        docs/acceptance/2026-08-11-phase-7-return-transformations-acceptance.md
git commit -m "docs: accept phase 7 return transformations"
~~~

### Task 46: Record immutable Task 45 acceptance trace

**Files:**

- Modify: docs/acceptance/2026-08-11-phase-7-return-transformations-acceptance.md

**Consumes:** completed Task 45 acceptance commit.

**Produces:** a minimal documentation-only, non-self-referential SHA trace.

- [ ] **Step 1: Record the direct Task 45 acceptance SHA**

Run:

~~~sh
git log -1 --format='%H %s'
~~~

Add the complete direct-parent Task 45 SHA beside Task 44 traceability. State
that this Task 46 commit changes only documentation and neither self-references
nor amends the initial Task 45 acceptance record.

- [ ] **Step 2: Verify exact scope**

Run:

~~~sh
git diff --check
git diff -- docs/acceptance/2026-08-11-phase-7-return-transformations-acceptance.md
~~~

Expected: a traceability-only documentation diff with no whitespace error.

- [ ] **Step 3: Commit Task 46**

Run:

~~~sh
git add docs/acceptance/2026-08-11-phase-7-return-transformations-acceptance.md
git commit -m "docs: trace phase 7 return transformations acceptance"
~~~

## Plan self-review

- [x] Task 44 supplies exact declarations, compatibility constraints, C++23 UTC/ISO grouping rules, Python oracle vectors, typed output schema, error paths, public RED/GREEN, and source/test commit scope.
- [x] Task 45 proves consumption through the public umbrella and records fresh core, sanitizer, optional graphics compile/link, and negative dependency evidence without a GUI-runtime claim.
- [x] Task 46 provides immutable Task 45 traceability without self-reference.
- [x] The plan deliberately represents Python MultiIndex labels with DataFrame columns rather than silently dropping labels or inventing a pandas wrapper.
- [x] No task changes CMake, calendars, positions/transactions, external dependencies, data providers, reports, plotting runtime, or the broader unresolved Python facade.

## Deferred after this plan

Pandas MultiIndex object compatibility; local-time-zone semantics; rolling
aggregation; benchmark/group alignment; positions, transactions, round trips,
attribution, bootstrap/distribution and Bayesian statistics, advanced risk
models, optimization, simulation, reports/tearsheets, data providers, a full
native/Python facade, packaging/release, and interactive ImPlot visual smoke
testing remain separate work. Completing this plan is not full fincore parity
or project completion.
