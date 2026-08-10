# fincore C++ Phase 2B Calendar Metrics Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add UTC calendar-year DataFrame metrics that match the supported
Python yearly and annual alpha/beta behavior while preserving the existing
`timestamp = int64_t` DataFrame boundary.

**Architecture:** Keep `dataframe` as `hmdf::StdDataFrame<timestamp>`, where a
timestamp is UTC Unix nanoseconds. A small C++23 chrono helper converts civil
UTC values and extracts calendar years. Yearly outputs are one-column
DataFrames indexed at each UTC year start; all paired metrics first perform
the existing common-index alignment and delegate their numerical calculations
to accepted scalar APIs.

**Tech Stack:** C++23, CMake, GoogleTest, cloudQuant/DataFrame, standard
`std::chrono`, local Python fincore oracle via the required Anaconda command.

---

## Contract shared by every task

- `timestamp` stays signed Unix nanoseconds in UTC. Do not migrate to
  `hmdf::DateTime`, add a second public DataFrame alias, use local time/DST,
  or reinterpret existing indexes.
- A yearly result is a single-double-column `dataframe`, indexed at
  `YYYY-01-01T00:00:00Z`, ascending by year. Its input observations keep their
  original order within a group.
- Structural errors throw `fincore::input_error`; numerical undefined cases
  continue to follow the delegated public scalar metric's contract.
- Pair-dependent APIs call `align_on_common_index` before year partitioning;
  never pair by position.
- Use `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python ...` for
  any Python oracle command.
- The positive ImPlot build/demo is out of scope and remains open without a
  real local Dear ImGui checkout.

### Task 15: UTC calendar boundary

**Files:**

- Create: `include/fincore/calendar.hpp`
- Create: `src/calendar.cpp`
- Create: `tests/unit/calendar_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write failing public tests**

Create a test through only `<fincore/calendar.hpp>` that requires:

```cpp
EXPECT_EQ(fincore::utc_timestamp(1970, 1, 1), 0);
EXPECT_EQ(fincore::utc_timestamp(2020, 2, 29), 1582934400000000000LL);
EXPECT_EQ(fincore::utc_year(-1), 1969);
EXPECT_EQ(fincore::utc_year(fincore::utc_timestamp(2021, 1, 1)), 2021);
EXPECT_THROW(fincore::utc_timestamp(2021, 2, 29), fincore::input_error);
```

Add a sub-day exact-nanosecond roundtrip and an overflow/invalid component
case. Register `Calendar` in the test executable.

**Step 2: Run it to verify failure**

Run from a headless build configured with `FINCORE_BUILD_IMPLOT=OFF`:

```sh
cmake --build build --parallel 4
ctest --test-dir build -R Calendar --output-on-failure
```

Expected: compilation fails because `<fincore/calendar.hpp>` / its public APIs
do not exist.

**Step 3: Implement the minimal UTC helper**

Declare `utc_timestamp(int, unsigned, unsigned, unsigned, unsigned, unsigned,
unsigned)` and `utc_year(timestamp) noexcept`. In `src/calendar.cpp`, use
`std::chrono::year_month_day`, `sys_days`, and `sys_time<nanoseconds>`. Reject
invalid components and conversion overflow with `input_error`; use UTC only.
Add the source to `fincore`.

**Step 4: Run focused verification**

Run the `Calendar` tests. Expected: all pass on C++23 without a host-time-zone
dependency.

**Step 5: Commit**

```sh
git add include/fincore/calendar.hpp src/calendar.cpp tests/unit/calendar_test.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add UTC calendar timestamp helpers"
```

### Task 16: DataFrame-native yearly returns, ratio, and active metrics

**Files:**

- Create: `include/fincore/metrics/yearly.hpp`
- Create: `src/metrics/yearly.cpp`
- Create: `tests/unit/yearly_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Step 1: Generate and record Python oracle values, then write failing tests**

Use a deterministic UTC-nanosecond fixture spanning 2019, 2020, and 2021.
Run the local Python reference under Anaconda for `annual_return_by_year`,
`annual_volatility_by_year`, `sharpe_ratio_by_year`, `max_drawdown_by_year`,
`annual_active_return`, `annual_active_return_by_year`, and
`information_ratio_by_year`. Put literal goldens and a tight numerical
tolerance in C++ tests.

Tests must assert all result frames use year-start UTC timestamps, fixed
column names, ascending years, and preserve input order inside an unsorted
within-year path. Include:

- ordinary cross-year scalar and DataFrame result goldens;
- empty returns -> empty correctly typed result frame;
- daily/weekly custom annualization behavior;
- paired returns/benchmark timestamp mismatch, where only common timestamps
  participate;
- pairwise NaN/+infinity isolation across years;
- named-column, missing-column, wrong-type, and invalid annualization paths.

**Step 2: Run it to verify failure**

```sh
cmake --build build --parallel 4
ctest --test-dir build -R Yearly --output-on-failure
```

Expected: missing `yearly.hpp`/API declarations cause a compile failure.

**Step 3: Implement grouped delegation**

Add the seven new APIs from the Phase 2B design. Together with the already
accepted scalar `annual_return`, these cover the eight Python `yearly.py`
functions in scope; do not duplicate or change `annual_return`. Resolve the
annualization once through `annualization_factor`; group a validated one-column frame by
`utc_year(index)` in sorted-year buckets while retaining row sequence;
materialize each bucket through `make_series`; call accepted `annual_return`,
`annual_volatility`, `sharpe_ratio`, or `max_drawdown`.

For active and information APIs, align both frames first and only then group.
Use annual-start timestamps and the fixed result-column names from the design.
Do not copy numeric formulas from the existing metric implementations.

**Step 4: Run focused verification**

Run all `Yearly` tests and existing `Returns|Ratios|Risk|Drawdown` tests.
Expected: all pass and no old scalar behavior changes.

**Step 5: Commit**

```sh
git add include/fincore/metrics/yearly.hpp src/metrics/yearly.cpp tests/unit/yearly_test.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add DataFrame-native yearly metrics"
```

### Task 17: Annual alpha and beta result DataFrames

**Files:**

- Modify: `include/fincore/metrics/alpha_beta.hpp`
- Modify: `src/metrics/alpha_beta.cpp`
- Create: `tests/unit/annual_alpha_beta_test.cpp`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write failing Python-oracle tests**

Add tests that compile against public alpha/beta and calendar headers and call
`annual_alpha` / `annual_beta` on the cross-year deterministic fixture.
Generate exact Python goldens under Anaconda. Verify result columns `alpha`
and `beta`, UTC year-start output index, alignment before grouping, risk-free
handling, non-finite pairs in only one year, per-year flat benchmarks, empty
aligned input, and named-column structural errors.

**Step 2: Run it to verify failure**

```sh
cmake --build build --parallel 4
ctest --test-dir build -R AnnualAlphaBeta --output-on-failure
```

Expected: compilation fails because annual alpha/beta declarations are absent.

**Step 3: Implement thin per-year delegation**

Add native DataFrame-returning annual alpha/beta APIs. Align first, group by
UTC year, make single-column group DataFrames, and invoke the existing public
`alpha` / `beta` APIs. `annual_alpha` resolves frequency plus optional
annualization; `annual_beta` keeps Python-compatible parameters but must not
invent annual scaling for beta. Do not change existing scalar alpha/beta
semantics.

The exact declarations are:

```cpp
dataframe annual_alpha(const dataframe&, const dataframe&, double risk_free = 0.0,
                       frequency period = frequency::daily,
                       std::optional<double> annualization = std::nullopt,
                       std::string_view returns_column_name = returns_column,
                       std::string_view benchmark_column_name = benchmark_column);
dataframe annual_beta(const dataframe&, const dataframe&, double risk_free = 0.0,
                      frequency period = frequency::daily,
                      std::optional<double> annualization = std::nullopt,
                      std::string_view returns_column_name = returns_column,
                      std::string_view benchmark_column_name = benchmark_column);
```

**Step 4: Run focused verification**

Run `AnnualAlphaBeta`, `AlphaBeta`, `ExtendedAlphaBeta`, and `Yearly` tests.

**Step 5: Commit**

```sh
git add include/fincore/metrics/alpha_beta.hpp src/metrics/alpha_beta.cpp tests/unit/annual_alpha_beta_test.cpp tests/CMakeLists.txt
git commit -m "feat: add annual alpha beta metrics"
```

### Task 18: Phase 2B public-surface and acceptance gate

**Files:**

- Modify: `include/fincore/metrics.hpp`
- Modify: `tests/unit/metrics_umbrella_test.cpp`
- Create: `docs/acceptance/2026-08-10-phase-2b-calendar-metrics-acceptance.md`
- Modify: `README.md`
- Modify: `tests/CMakeLists.txt` (only if test registration changes)

**Step 1: Write the public umbrella regression and acceptance checklist**

Extend `MetricsUmbrella` to include/use `yearly.hpp` through
`<fincore/metrics.hpp>` and construct timestamps through only the supported
public calendar header. Add unchecked acceptance items for calendar exactness,
full core CTest, Homebrew LLVM ASan+UBSan, and the independent open ImPlot
positive-build gate.

**Step 2: Run it to verify failure**

Run the focused umbrella test before adding `yearly.hpp` to the umbrella.
Expected: failure due to missing yearly declaration from the public umbrella.

**Step 3: Implement/document the accepted surface**

Include `fincore/metrics/yearly.hpp` from `metrics.hpp`; do not include a
detail header, analysis context, rolling engine, or ImPlot adapter. Update the
README and acceptance record with actual commands, compilers, full SHAs, exact
test counts, sanitizer outcome, and retained ImGui diagnostic. Explicitly
defer weekly/monthly/quarterly aggregation, deep drawdown/consecutive,
positions/transactions/round trips, Bayesian/bootstrap/attribution, and the
full facade.

**Step 4: Run final verification**

Use fresh temporary directories:

```sh
cmake -S . -B build-core -G Ninja -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF
cmake --build build-core --parallel 4
ctest --test-dir build-core --output-on-failure

cmake -S . -B build-sanitize -G Ninja -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++
cmake --build build-sanitize --parallel 4
ctest --test-dir build-sanitize --output-on-failure
```

Also confirm an ImPlot-enabled configuration with an invalid `FINCORE_IMGUI_SOURCE_DIR`
still fails with the documented precise diagnostic. Record exact observed
counts and outputs rather than assuming them.

**Step 5: Commit**

```sh
git add include/fincore/metrics.hpp tests/unit/metrics_umbrella_test.cpp docs/acceptance/2026-08-10-phase-2b-calendar-metrics-acceptance.md README.md tests/CMakeLists.txt
git commit -m "docs: accept phase 2B calendar metrics"
```

## Deferred after this plan

Calendar aggregation at weekly/monthly/quarterly frequencies, composite-key
result schemas, deep drawdown tables/durations, consecutive-period/date
metrics, positions, transactions, round trips, Bayesian/bootstrap metrics,
performance attribution, optimization, simulation, risk models, reports, data
providers, tearsheets, a comprehensive native facade, packaging/release, and
positive ImPlot build/demo remain separate work. This plan's completion is not
full Phase 2 or full project completion.
