# fincore C++ Phase 2C Path Metrics Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Port Python-compatible detailed drawdown and daily consecutive-return
metrics to the C++23 DataFrame-native core without adding a second tabular
representation or changing accepted scalar metric semantics.

**Architecture:** Time-series inputs remain validated one-double-column
`dataframe` values indexed by UTC nanoseconds.  Detailed Python list/dict/tuple
results become strongly typed C++ value objects with optional timestamps;
private state scanners preserve input order and delegate no numerical policy to
new global filters.  The existing `max_drawdown` implementation is retained;
new detailed drawdown logic intentionally follows the Python path that starts
at the first observed cumulative wealth.

**Tech Stack:** C++23, CMake, GoogleTest, cloudQuant/DataFrame, standard
`std::optional`/containers/chrono, and the local Python fincore oracle through
`/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python ...`.

---

## Contract shared by every task

- `fincore::timestamp` stays signed UTC Unix nanoseconds and `dataframe` stays
  `hmdf::StdDataFrame<timestamp>`; do not introduce `hmdf::DateTime`, a second
  DataFrame alias, a local-time conversion, or a timezone dependency.
- All public time-series inputs call `require_double_column`.  Missing, empty,
  wrong-typed, and short columns throw the existing `input_error`.
- Preserve supplied row order.  Do not sort input, realign it, deduplicate it,
  or impose a global finite-only rule.  NaN behavior follows the corresponding
  Python oracle; signed infinities retain ordinary C++ comparison/arithmetic
  behavior.
- Detailed drawdown results use native `std::optional<timestamp>` or
  `std::optional<std::size_t>` for unavailable recovery.  Do not use timestamp
  sentinels: every `int64_t` timestamp is valid under the accepted calendar
  boundary.
- Use only public headers in consumer tests.  Keep `DataFrame::DataFrame` as
  the only data dependency and keep ImPlot optional.  Never fetch, clone,
  vendor, or fabricate Dear ImGui.

### Task 19: Detailed drawdown episode API

**Files:**

- Modify: `include/fincore/metrics/drawdown.hpp`
- Modify: `src/metrics/drawdown.cpp`
- Modify: `tests/unit/drawdown_test.cpp`

**Step 1: Generate the oracle and write failing public tests**

Under Anaconda, run the local Python `fincore.metrics.drawdown` helpers on UTC
daily returns:

```python
[.10, -.10, -.10, .25, -.05, -.05, .11, -.20, .30, -.10]
```

at `2020-01-01` through `2020-01-10`.  Record `get_all_drawdowns` as
`[-.19, -.0975, -.20, -.10]` within floating tolerance and detailed periods
with durations `(1, 1)`, `(1, 1)`, `(0, 1)`, and `(0, None)`.  The maximum
window is peak `2020-01-07`, valley `2020-01-08`, recovery `2020-01-09`.

Add public-header tests for these declarations:

```cpp
struct drawdown_period {
    timestamp start;
    timestamp valley;
    std::optional<timestamp> recovery;
    double value;
    std::size_t duration_periods;
    std::optional<std::size_t> recovery_duration_periods;
};

struct drawdown_window {
    timestamp peak;
    timestamp valley;
    std::optional<timestamp> recovery;
};

std::vector<double> get_all_drawdowns(
    const dataframe&, std::string_view column = returns_column);
std::vector<drawdown_period> get_all_drawdowns_detailed(
    const dataframe&, std::string_view column = returns_column);
std::optional<drawdown_window> max_drawdown_window(
    const dataframe&, std::string_view column = returns_column);
```

Also prove chronological output, earliest trough tie behavior, a final open
period, NaN-as-zero compounding, empty results, named columns, and structural
errors.  Include the Python-compatible initial-loss distinction: existing
`max_drawdown({-.01}) == -.01`, while `get_all_drawdowns_detailed({-.01})`
and `max_drawdown_window({-.01})` are empty because detailed Python paths have
no synthetic pre-observation peak.

**Step 2: Run it to verify failure**

```sh
cmake --build build --parallel 4
ctest --test-dir build -R Drawdown --output-on-failure
```

Expected: compilation fails because the detailed drawdown declarations are
absent.

**Step 3: Implement one private underwater-event scanner**

Extend the public header with the exact types/APIs above.  In
`src/metrics/drawdown.cpp`, add an anonymous-namespace scanner that compounds
NaN as zero, initializes the running peak from the first observed wealth,
marks only strictly negative underwater samples, emits chronological episodes,
and records first minima/recovery boundaries.  Materialize none of these
results as a fake DataFrame.  Reuse the scanner for all three public APIs, but
do not alter `drawdown_series` or accepted `max_drawdown`.

**Step 4: Run focused verification**

```sh
cmake --build build --parallel 4
ctest --test-dir build -R 'Drawdown|Returns' --output-on-failure
```

Expected: new and existing drawdown/return behavior passes.

**Step 5: Commit**

```sh
git add include/fincore/metrics/drawdown.hpp src/metrics/drawdown.cpp tests/unit/drawdown_test.cpp
git commit -m "feat: add detailed drawdown episodes"
```

### Task 20: Drawdown durations and severity ranks

**Files:**

- Modify: `include/fincore/metrics/drawdown.hpp`
- Modify: `src/metrics/drawdown.cpp`
- Modify: `tests/unit/drawdown_test.cpp`

**Step 1: Write failing duration/rank tests**

On the Task 19 fixture, assert these local Python goldens:

```cpp
max_drawdown_days(...) == 1.0;
max_drawdown_weeks(...) == 1.0 / 7.0;
max_drawdown_months(...) == 1.0 / 30.0;
max_drawdown_recovery_days(...) == 1.0;
second_max_drawdown(...) == -0.19;
third_max_drawdown(...) == -0.10;
second_max_drawdown_days(...) == 1.0;
second_max_drawdown_recovery_days(...) == 1.0;
third_max_drawdown_days(...) == 0.0;
third_max_drawdown_recovery_days(...) is NaN;
```

Add empty, no-underwater, unrecovered, first-observation-loss, tie, and
sub-day UTC duration tests.  Verify days are elapsed UTC 24-hour days from
timestamps, not row counts.  Existing Python semantics return zero duration
for nonempty paths without an underwater period; preserve that behavior.

**Step 2: Run it to verify failure**

```sh
cmake --build build --parallel 4
ctest --test-dir build -R Drawdown --output-on-failure
```

Expected: compilation fails because duration/rank declarations are absent.

**Step 3: Add thin scanner-based helpers**

Add these APIs, all with the normal optional `column` default:

```cpp
double max_drawdown_days(const dataframe&, std::string_view column = returns_column);
double max_drawdown_weeks(const dataframe&, std::string_view column = returns_column);
double max_drawdown_months(const dataframe&, std::string_view column = returns_column);
double max_drawdown_recovery_days(const dataframe&, std::string_view column = returns_column);
double max_drawdown_recovery_weeks(const dataframe&, std::string_view column = returns_column);
double max_drawdown_recovery_months(const dataframe&, std::string_view column = returns_column);
double second_max_drawdown(const dataframe&, std::string_view column = returns_column);
double third_max_drawdown(const dataframe&, std::string_view column = returns_column);
double second_max_drawdown_days(const dataframe&, std::string_view column = returns_column);
double second_max_drawdown_recovery_days(const dataframe&, std::string_view column = returns_column);
double third_max_drawdown_days(const dataframe&, std::string_view column = returns_column);
double third_max_drawdown_recovery_days(const dataframe&, std::string_view column = returns_column);
```

Use the same private cumulative/underwater scan rather than copying arithmetic.
For the maximum-duration family, reproduce Python's nonempty no-drawdown zero
case independently from the detailed-event empty case.  Rank detailed episodes
by ascending drawdown value with stable chronological tie behavior.  Return
`NaN` for an unavailable rank/recovery; do not change `max_drawdown`.

**Step 4: Run focused verification**

```sh
cmake --build build --parallel 4
ctest --test-dir build -R 'Drawdown|Returns|Yearly' --output-on-failure
```

**Step 5: Commit**

```sh
git add include/fincore/metrics/drawdown.hpp src/metrics/drawdown.cpp tests/unit/drawdown_test.cpp
git commit -m "feat: add drawdown durations and ranks"
```

### Task 21: Daily consecutive-return metrics

**Files:**

- Create: `include/fincore/metrics/consecutive.hpp`
- Create: `src/metrics/consecutive.cpp`
- Create: `tests/unit/consecutive_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write failing public tests from the Python oracle**

Use UTC daily dates `2021-02-01` through `2021-02-12` and returns:

```python
[.01, .02, .03, 0, -.02, -.03, .01, .02, -.04, -.05, -.06, .01]
```

Verify exact daily Python values: up/down run lengths `3`/`3`, max gain/loss
`.06`/`-.15`, max daily gain/loss `.03`/`-.06`, and the expected first-run/
single-day timestamps.  Add zero and NaN breaking behavior, signed-infinity
comparison behavior, earliest ties, empty input (`NaN` numeric results and
empty optional dates), no-up/no-down behavior, order preservation, named
columns, and structural errors.

For the typed C++ date accessors, skip NaN single-day candidates.  A nonempty
all-NaN column therefore returns `std::nullopt` for both single-day extrema
dates (rather than reproducing current pandas `idxmax`/`idxmin`'s `ValueError`);
its numeric extrema remain `NaN` and its run dates remain empty.  Zero and
infinity are eligible single-day candidates and retain first-occurrence ties.

The public surface is:

```cpp
struct daily_consecutive_statistics {
    double max_consecutive_up_days;
    double max_consecutive_down_days;
    double max_consecutive_gain;
    double max_consecutive_loss;
};

daily_consecutive_statistics daily_consecutive_stats(
    const dataframe&, std::string_view column = returns_column);
double max_consecutive_up_days(const dataframe&, std::string_view column = returns_column);
double max_consecutive_down_days(const dataframe&, std::string_view column = returns_column);
double max_consecutive_gain(const dataframe&, std::string_view column = returns_column);
double max_consecutive_loss(const dataframe&, std::string_view column = returns_column);
double max_single_day_gain(const dataframe&, std::string_view column = returns_column);
double max_single_day_loss(const dataframe&, std::string_view column = returns_column);
std::optional<timestamp> max_single_day_gain_date(const dataframe&, std::string_view column = returns_column);
std::optional<timestamp> max_single_day_loss_date(const dataframe&, std::string_view column = returns_column);
std::optional<timestamp> max_consecutive_up_start_date(const dataframe&, std::string_view column = returns_column);
std::optional<timestamp> max_consecutive_up_end_date(const dataframe&, std::string_view column = returns_column);
std::optional<timestamp> max_consecutive_down_start_date(const dataframe&, std::string_view column = returns_column);
std::optional<timestamp> max_consecutive_down_end_date(const dataframe&, std::string_view column = returns_column);
```

Do not declare Python `consecutive_stats`, weekly, or monthly functions in this
task: those APIs promise calendar resampling that is expressly out of scope.

**Step 2: Run it to verify failure**

```sh
cmake --build build --parallel 4
ctest --test-dir build -R Consecutive --output-on-failure
```

Expected: compilation fails because `consecutive.hpp` and its APIs are absent.

**Step 3: Implement a single order-preserving run scanner**

Validate the double column once.  Build an anonymous-namespace run summary
that scans in frame order, treats only `> 0` as up and `< 0` as down, records
first maximum ties, sums each qualifying contiguous run, and returns optional
timestamps for unavailable dates.  Public scalar functions project from that
summary; do not repeatedly scan or implement weekly/monthly resampling.

**Step 4: Run focused verification**

```sh
cmake --build build --parallel 4
ctest --test-dir build -R 'Consecutive|Drawdown|Returns' --output-on-failure
```

**Step 5: Commit**

```sh
git add include/fincore/metrics/consecutive.hpp src/metrics/consecutive.cpp tests/unit/consecutive_test.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add daily consecutive metrics"
```

### Task 22: Phase 2C public-surface and acceptance gate

**Files:**

- Modify: `include/fincore/metrics.hpp`
- Modify: `tests/unit/metrics_umbrella_test.cpp`
- Create: `docs/acceptance/2026-08-10-phase-2c-path-metrics-acceptance.md`
- Modify: `README.md`
- Modify: `tests/CMakeLists.txt` only if test registration changes

**Step 1: Write umbrella RED regression and acceptance checklist**

Extend `MetricsUmbrella` through only `<fincore/metrics.hpp>` (and public
`calendar.hpp` only when a date fixture needs construction) to instantiate a
detailed drawdown result and a daily consecutive result.  Before adding
`consecutive.hpp` to the metric umbrella, build the test and record the
expected missing-declaration failure.  Create an unchecked acceptance record
for detailed-path oracle coverage, full core CTest, Homebrew LLVM ASan+UBSan,
and the independent positive ImPlot gate.

**Step 2: Implement/document only the accepted surface**

Include `fincore/metrics/consecutive.hpp` from `metrics.hpp`.  Do not add a
calendar, detail, context, rolling-engine, or ImPlot header.  Update the README
and acceptance record with actual full commit SHAs, commands, compiler
versions, exact observed test counts, sanitizer outcomes, and the retained
invalid-ImGui diagnostic.  State clearly that weekly/monthly consecutive
aggregation, `get_top_drawdowns`/`gen_drawdown_table`, positions,
transactions, round trips, attribution, Bayesian/bootstrap, a full façade,
and positive ImPlot remain deferred/open.

**Step 3: Run final verification in fresh directories**

```sh
cmake -S . -B build-core -G Ninja -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF
cmake --build build-core --parallel 4
ctest --test-dir build-core --output-on-failure

cmake -S . -B build-sanitize -G Ninja -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++
cmake --build build-sanitize --parallel 4
ctest --test-dir build-sanitize --output-on-failure
```

Create fresh `/tmp` directories with `mktemp -d`; preserve them rather than
recursively deleting broad paths.  Also verify an ImPlot configuration with an
invalid `FINCORE_IMGUI_SOURCE_DIR` still fails with the documented precise
diagnostic.  Do not attempt a positive ImPlot build without a real user-supplied
Dear ImGui checkout.

**Step 4: Commit**

```sh
git add include/fincore/metrics.hpp tests/unit/metrics_umbrella_test.cpp docs/acceptance/2026-08-10-phase-2c-path-metrics-acceptance.md README.md tests/CMakeLists.txt
git commit -m "docs: accept phase 2C path metrics"
```

## Deferred after this plan

Weekly/monthly consecutive aggregation and `consecutive_stats`, top-drawdown
selection/table generation, positions, transactions, round trips, attribution,
Bayesian/bootstrap metrics, optimization, simulation, reports, tearsheets, a
comprehensive native façade, packaging/release, and positive ImPlot build/demo
remain separate work.  Completing this plan is not full fincore parity or
project completion.
