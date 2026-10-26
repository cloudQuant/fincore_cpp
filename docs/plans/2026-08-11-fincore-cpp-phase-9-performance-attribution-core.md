# fincore C++ Phase 9 Performance Attribution Core Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the deterministic, DataFrame-native performance-attribution core of Python `fincore.metrics.perf_attrib` without pretending that cloudQuant/DataFrame has a pandas MultiIndex.

**Architecture:** Use a wide timestamp-indexed positions table and a wide timestamp-indexed factor-returns table. Represent Python's `(datetime, ticker)` factor loadings as a normal timestamp index plus an explicit string `symbol` column. A focused `perf_attrib` module validates this table contract, normalizes dollar holdings by gross absolute value, computes selected factor exposures, then constructs contribution/summary DataFrames by delegating existing scalar return and ratio APIs.

**Tech Stack:** C++23, the local cloudQuant/DataFrame dependency, CMake, GoogleTest, the local Python oracle via `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`, Homebrew LLVM 22 ASan+UBSan, and the optional local Dear ImGui/ImPlot compile/link gate.

## Global Constraints

- Use only the accepted `fincore::dataframe` alias (`hmdf::StdDataFrame<fincore::timestamp>`) and local dependencies. Do not add pandas, a Python runtime, a fake MultiIndex, a warning/logging dependency, or a new date-time library.
- Create one `perf_attrib` metric module. Do not modify existing scalar metric implementations, `dataframe.hpp`, `calendar.hpp`, transaction/position behavior, ImPlot source, dependency configuration, or accepted public semantics.
- `returns`, `positions`, and `factor_returns` must have the same duplicate-free raw timestamp index sequence in the same order. Preserve that sequence exactly in daily result tables; do not sort, floor dates, or silently align/drop rows.
- Positions are wide double asset columns with an optional genuine-double cash column. Every noncash double column is an asset; non-double metadata is ignored. Factor returns' genuine-double columns define the ordered factor set.
- Factor loadings are long-form: timestamp index, one required genuine `std::string` symbol column, and selected genuine-double factor columns. Required `(timestamp, symbol)` keys are unique and complete. Missing/duplicate keys and malformed/mismatched inputs throw `fincore::input_error`.
- Dollar normalization uses each row's `sum(abs(non-NaN asset value))`, excludes cash, and preserves ordinary IEEE results for zero gross totals. Do not reuse the existing net-value allocation helper.
- Exposure and contribution sums follow pandas-like skip-NaN behavior documented in the Phase 9 design: all-NaN sums are `0.0`; factor tilt means skip NaNs and are NaN for an empty valid sample; infinity remains ordinary IEEE arithmetic.
- Output schemas and concrete types are contractual. Result tables contain factors in factor-return order; factor summary uses an ordinal index with an explicit string `factor` column because DataFrame indexes are timestamps.
- The public core does not implement Python's warning-producing `align_and_warn`, turnover diagnostics, market/factor providers, style or Brinson attribution, Bayesian/bootstrap models, reports/tearsheets, packaging, or an ImPlot runtime.
- All Python oracle commands must use `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`; never use bare `python` or `python3`.

---

### Task 50: Add the native table contract, normalization, and exposure primitive

**Files:**

- Create: `include/fincore/metrics/perf_attrib.hpp`
- Create: `src/metrics/perf_attrib.cpp`
- Create: `tests/unit/perf_attrib_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Consumes:** `fincore::dataframe`, `timestamp`, `require_double_column`, `input_error`, `cash_column`, and C++23 `std::span`.

**Produces:**

~~~cpp
inline constexpr std::string_view attribution_symbol_column{"symbol"};
inline constexpr std::string_view attribution_total_returns_column{"total_returns"};
inline constexpr std::string_view attribution_common_returns_column{"common_returns"};
inline constexpr std::string_view attribution_specific_returns_column{"specific_returns"};
inline constexpr std::string_view attribution_tilt_returns_column{"tilt_returns"};
inline constexpr std::string_view attribution_timing_returns_column{"timing_returns"};
inline constexpr std::string_view attribution_factor_column{"factor"};

dataframe normalize_attribution_positions(
    const dataframe& positions,
    bool positions_in_dollars = true,
    std::string_view cash_column_name = cash_column);

dataframe compute_exposures(
    const dataframe& positions,
    const dataframe& factor_loadings,
    std::span<const std::string_view> factor_column_names,
    std::string_view cash_column_name = cash_column,
    std::string_view symbol_column_name = attribution_symbol_column);
~~~

- [ ] **Step 1: Record the Python exposure oracle and write a public-header RED test**

Create `tests/unit/perf_attrib_test.cpp`, include only `<fincore/metrics/perf_attrib.hpp>` among fincore headers, and register it in `tests/CMakeLists.txt`. Add the new source name to `CMakeLists.txt` only after the RED compile observation so the first failure is missing public API, not a missing implementation object.

Use this three-date dollar-position fixture in the required Python oracle and in the C++ test helpers:

~~~text
dates: 2024-01-02, 2024-01-03, 2024-01-04
returns: [.02, -.01, .03]
positions A:    [ 20,  30, -20]
positions B:    [ 50, -10,  20]
positions cash: [ 30,  10,  10]
factor returns market: [.01, .02, -.01]
factor returns value:  [.03, -.02,  .04]
loadings (date, A): market [.2, .5, .1], value [.4, .1, .5]
loadings (date, B): market [.4, .2, .6], value [-.2, .3, -.1]
~~~

Run:

~~~sh
/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python -c '... import normalize_and_stack_positions, perf_attrib_core; ...'
~~~

Record these exact core exposure goldens in the ignored Phase 9 evidence ledger:

~~~text
market = [0.342857142857143, 0.325, 0.25]
value  = [-0.028571428571429, 0.0, -0.3]
~~~

The C++ test must construct wide `positions`, long `factor_loadings` with a string `symbol` column, and `std::array<std::string_view, 2>` factor names. It must assert normalized weights `[2/7, 3/4, -1/2]` for `A`, `[5/7, -1/4, 1/2]` for `B`, the original timestamp index, factor column order/types, and the two exposure vectors above. Add public error assertions for empty factor list, empty/duplicate factor names, absent or wrong-type symbol, wrong-type cash, missing loading date/key, duplicate `(date, symbol)` loading key, and malformed column length. Task 51 owns the distinct returns/positions/factor-returns index-sequence mismatch assertion.

- [ ] **Step 2: Run the RED test before production edits**

Run:

~~~sh
cmake --build build --parallel 4
ctest --test-dir build -R PerfAttrib --output-on-failure
~~~

Expected: compilation fails in the public-only test because `fincore/metrics/perf_attrib.hpp` and its declarations do not exist. The existing tests must not become failures.

- [ ] **Step 3: Implement the structural helpers, normalizer, and exposure calculation**

Create the header with the constants and declarations above. In the source, keep all table inspection private. Use DataFrame's type metadata to identify genuine `double` position columns and a genuine `std::string` symbol column; verify every required vector length against its frame index before accessing values. Build the loading-row lookup with a value-owned key so views into DataFrame storage never outlive validation:

~~~cpp
using loading_key = std::pair<timestamp, std::string>;
std::map<loading_key, std::size_t> rows_by_loading_key;

for (std::size_t row = 0U; row < factor_loadings.get_index().size(); ++row) {
    const loading_key key{factor_loadings.get_index()[row], symbols[row]};
    if (!rows_by_loading_key.emplace(key, row).second) {
        throw input_error{"factor-loading timestamp and symbol keys must be unique"};
    }
}
~~~

Normalize dollar rows with the required absolute non-NaN denominator, emit a new wide result with noncash asset columns in frame order, and retain the input index exactly. For `compute_exposures`, validate each requested factor column, look up every required `(index[row], asset-name)` record, multiply the position and loading, skip only NaN products while summing, and emit `0.0` if a factor/date has no non-NaN products:

~~~cpp
double total = 0.0;
for (const auto& asset : assets) {
    const double product = asset.values[row] * factor_values[loading_row];
    if (!std::isnan(product)) {
        total += product;
    }
}
exposure_values.push_back(total);
~~~

Register `src/metrics/perf_attrib.cpp` in the library and the test in CMake. Do not add a generic string-column utility to `dataframe.hpp` and do not alter existing position or transaction functions.

- [ ] **Step 4: Run focused GREEN and the related regression set**

Run:

~~~sh
cmake --build build --parallel 4
ctest --test-dir build -R 'PerfAttrib|Positions|Transactions|Dataframe' --output-on-failure
git diff --check
~~~

Expected: the public header test compiles, the documented normalization and exposure goldens pass, every structural-error assertion throws `input_error`, related existing metrics remain green, and the diff check is empty.

- [ ] **Step 5: Review the task boundary and commit only owned files**

Review that the implementation has not changed existing helpers or introduced implicit date sorting/row dropping. Stage only:

~~~sh
git add CMakeLists.txt tests/CMakeLists.txt include/fincore/metrics/perf_attrib.hpp src/metrics/perf_attrib.cpp tests/unit/perf_attrib_test.cpp
git commit -m "feat: add native factor exposure metrics"
~~~

Expected: one focused commit with a clean worktree apart from ignored evidence.

### Task 51: Add attribution outputs, summary records, and cost-adjusted cumulative returns

**Files:**

- Modify: `include/fincore/metrics/perf_attrib.hpp`
- Modify: `src/metrics/perf_attrib.cpp`
- Modify: `tests/unit/perf_attrib_test.cpp`

**Consumes:** Task 50's normalizer and `compute_exposures`, plus accepted `annual_return`, `sharpe_ratio`, `cumulative_return`, `cumulative_returns`, `returns_column`, and `daily_annualization` APIs.

**Produces:**

~~~cpp
struct perf_attribution_result {
    dataframe risk_exposures;
    dataframe attribution;
};

struct perf_attribution_statistics {
    std::map<std::string, double> summary;
    dataframe factor_summary;
};

perf_attribution_result perf_attrib_core(...);
perf_attribution_result perf_attrib(...);
perf_attribution_statistics create_perf_attrib_stats(
    const perf_attribution_result& result,
    double annualization = daily_annualization);
dataframe cumulative_returns_less_costs(...);
~~~

- [ ] **Step 1: Extend the public-only test with unresolved core APIs and record full Python goldens**

Before adding these declarations, add tests that call `perf_attrib_core`, `perf_attrib`, `create_perf_attrib_stats`, and both `cumulative_returns_less_costs` overloads. Use the Task 50 fixture and record the following Anaconda Python goldens:

~~~text
factor contribution market = [ .003428571428571, .0065, -.0025]
factor contribution value  = [-.000857142857143, -.0, -.012]
common_returns             = [ .002571428571429, .0065, -.0145]
specific_returns           = [ .017428571428571, -.0165, .0445]
tilt_returns               = [-.000226190476190, .008309523809524, -.007440476190476]
timing_returns             = [ .002797619047619, -.001809523809524, -.007059523809524]
~~~

The `attribution` table must be asserted to contain ordered factor columns `market`, `value`, then `total_returns`, `common_returns`, `specific_returns`, `tilt_returns`, and `timing_returns`, all genuine doubles, on the original three timestamps. Verify `perf_attrib` with `positions_in_dollars=true` matches core invoked with the explicit normalizer, while `false` leaves provided weights unchanged.

Record and assert this summary map for daily annualization:

~~~text
Annualized Specific Return = 39.89648506339985
Annualized Common Return   = -0.37309539033531
Annualized Total Return    = 26.170501183432872
Specific Sharpe Ratio      = 7.864941869197365
Cumulative Specific Return = 0.0451695245
Cumulative Common Return   = -0.005543635214286
Total Returns              = 0.040094
~~~

The factor-summary table uses ordinal index `[0, 1]`, string `factor` values `["market", "value"]`, and ordered numeric columns:

~~~text
Average Risk Factor Exposure = [ .305952380952381, -.109523809523810]
Annualized Return            = [ .861676871127715, -.662479050655964]
Cumulative Return            = [ .00742598, -.012846857142857]
~~~

For returns `[.02, -.01, .03]` and costs `[.001, .002, .003]`, assert the cost-adjusted cumulative output is `[.019, .006772, .033954844]`. Also cover the no-cost overload by direct equality with existing `cumulative_returns`.

- [ ] **Step 2: Run the second RED test**

Run:

~~~sh
cmake --build build --parallel 4
ctest --test-dir build -R PerfAttrib --output-on-failure
~~~

Expected: compilation fails only because the newly tested result records and operations are absent from the public header. Task 50's exposure tests still compile up to that missing API boundary.

- [ ] **Step 3: Implement core delegation, output construction, statistics, and costs**

Add the value-owning records and declarations to the public header. In the source, derive ordered factor names from `factor_returns` double metadata, verify all three daily frames have identical indexes, and call the public `compute_exposures` once. Build each factor contribution and five reserved columns in the defined order:

~~~cpp
contribution[factor][row] = exposure[factor][row] * factor_return[factor][row];
common[row] = sum_skipping_nan(contribution_at_row);
specific[row] = return_values[row] - common[row];
timing[row] = common[row] - tilt_returns[row];
~~~

Compute one factor tilt mean from the full exposure column, skipping NaNs. `perf_attrib` must call `normalize_attribution_positions` only for dollar positions and delegate exactly once to `perf_attrib_core`.

For the statistics record, validate that every factor exposure has a matching attribution contribution column and call existing public APIs on temporary single-column DataFrames rather than copying their arithmetic. Construct the seven-label `std::map` and the ordinal-indexed mixed `factor_summary` table. For the costs overload, validate exact index equality, build `returns - costs` under the caller's requested column name, and delegate to `cumulative_returns`.

Do not modify `annual_return`, `sharpe_ratio`, `cumulative_return`, or `cumulative_returns`. Keep the unavailable warning/turnover wrapper out of this module.

- [ ] **Step 4: Run focused GREEN, semantic regressions, and full CTest**

Run:

~~~sh
cmake --build build --parallel 4
ctest --test-dir build -R 'PerfAttrib|Returns|Ratios|Positions|Transactions|Dataframe' --output-on-failure
ctest --test-dir build --output-on-failure
git diff --check
~~~

Expected: all exact Python goldens and public-delegation assertions pass. Add and pass edge regressions for typed-empty valid schemas, custom return/cash/symbol names, zero-gross rows, NaN-only exposure rows, positive/negative infinity propagation, no factors, factor/reserved-name collisions, absent factor-loading columns, malformed cost frames, and nonidentical indexes.

- [ ] **Step 5: Review and commit the complete numeric core**

Review that the only new public behavior is owned by `perf_attrib`, that the result records own their DataFrames, and that every result table preserves its contractual order/types. Stage only:

~~~sh
git add include/fincore/metrics/perf_attrib.hpp src/metrics/perf_attrib.cpp tests/unit/perf_attrib_test.cpp
git commit -m "feat: add performance attribution core"
~~~

Expected: one task-scoped commit with focused and full verification recorded.

### Task 52: Export the public umbrella and record Phase 9 acceptance

**Files:**

- Modify: `include/fincore/metrics.hpp`
- Modify: `tests/unit/metrics_umbrella_test.cpp`
- Modify: `README.md`
- Create: `docs/acceptance/2026-08-11-phase-9-performance-attribution-core-acceptance.md`

**Consumes:** all Task 50–51 public APIs and the project's established fresh-build, sanitizer, optional ImPlot, and invalid-ImGui gates.

**Produces:** public umbrella visibility for the Phase 9 API and an acceptance record that distinguishes deterministic numeric proof from deferred warning/turnover, provider, report, and GUI-runtime work.

- [ ] **Step 1: Add an umbrella-only RED consumer**

Edit `metrics_umbrella_test.cpp` so it still directly includes only `<fincore/calendar.hpp>` and `<fincore/metrics.hpp>`. Build the simple two-factor fixture through `fincore::perf_attrib`, and assert one exposure and one attribution result field plus the no-cost cumulative overload. Do not add a direct `perf_attrib.hpp` include.

- [ ] **Step 2: Run the umbrella RED test**

Run:

~~~sh
cmake --build build --parallel 4
ctest --test-dir build -R MetricsUmbrella --output-on-failure
~~~

Expected: the public-only consumer fails because `metrics.hpp` does not yet include `metrics/perf_attrib.hpp`; no private-header workaround is permitted.

- [ ] **Step 3: Export the header and write the acceptance boundary**

Add one public include in `metrics.hpp`:

~~~cpp
#include <fincore/metrics/perf_attrib.hpp>
~~~

Update the README's implementation/status table with the exact Phase 9 public surface, long-form loading model, test counts, known compiler warnings, and the explicit deferred scope. Create the acceptance document with the direct Task 50/51 commit SHAs, public API/schema mapping, RED evidence, Python-oracle evidence, and these required final commands:

~~~sh
cmake -S . -B /tmp/fincore-phase9-apple -DCMAKE_BUILD_TYPE=Debug -DFINCORE_BUILD_IMPLOT=OFF
cmake --build /tmp/fincore-phase9-apple --parallel 4
ctest --test-dir /tmp/fincore-phase9-apple --output-on-failure

cmake -S . -B /tmp/fincore-phase9-llvm -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ -DCMAKE_BUILD_TYPE=Debug -DFINCORE_ENABLE_SANITIZERS=ON -DFINCORE_BUILD_IMPLOT=OFF
cmake --build /tmp/fincore-phase9-llvm --parallel 4
ctest --test-dir /tmp/fincore-phase9-llvm --output-on-failure

cmake -S . -B /tmp/fincore-phase9-implot -DCMAKE_BUILD_TYPE=Debug
cmake --build /tmp/fincore-phase9-implot --parallel 4
ctest --test-dir /tmp/fincore-phase9-implot --output-on-failure
~~~

Run the existing invalid `FINCORE_IMGUI_SOURCE_DIR` configuration in a fresh directory and record its expected rejection. Do not launch the GUI.

- [ ] **Step 4: Run the umbrella GREEN and all acceptance gates**

Run the focused umbrella test, the full existing build CTest, then each fresh acceptance command from Step 3. Search the LLVM log for AddressSanitizer, UndefinedBehaviorSanitizer, and `runtime error`; record no sanitizer diagnostic only when all are absent. Record third-party or macOS OpenGL warnings exactly, without presenting them as a clean-warning claim.

- [ ] **Step 5: Commit only acceptance-owned tracked files**

Run `git diff --check`, audit the path list, then stage and commit exactly:

~~~sh
git add include/fincore/metrics.hpp tests/unit/metrics_umbrella_test.cpp README.md docs/acceptance/2026-08-11-phase-9-performance-attribution-core-acceptance.md
git commit -m "docs: accept phase 9 performance attribution core"
~~~

Expected: acceptance commit contains no source implementation change and leaves the evidence report ignored.

### Task 53: Trace the acceptance commit and run the whole-phase review gate

**Files:**

- Modify: `docs/acceptance/2026-08-11-phase-9-performance-attribution-core-acceptance.md`

**Consumes:** Task 52's acceptance commit SHA and the completed Task 50–52 implementation chain.

**Produces:** non-self-referential traceability for the first acceptance record and an independent whole-phase review verdict.

- [ ] **Step 1: Add the one-line trace correction after the initial acceptance commit**

Append the complete direct Task 52 acceptance SHA beside the Task 50/51 chain, and state that Task 53 is documentation-only, does not amend Task 52, and does not list its own SHA. Do not alter recorded compiler/test evidence.

- [ ] **Step 2: Verify the trace-only diff and focused public consumer**

Run:

~~~sh
git diff --check
ctest --test-dir build -R 'MetricsUmbrella|PerfAttrib' --output-on-failure
~~~

Expected: only the acceptance document changes, its trace is correct, and the public umbrella consumer remains green.

- [ ] **Step 3: Commit the non-self-referential trace**

Run:

~~~sh
git add docs/acceptance/2026-08-11-phase-9-performance-attribution-core-acceptance.md
git commit -m "docs: trace phase 9 performance attribution acceptance"
~~~

Expected: a one-file documentation commit with no self-SHA.

- [ ] **Step 4: Request and address an independent whole-phase review**

Give a fresh reviewer the Phase 9 design, this plan, Task 50–53 commit range, the Python oracle evidence, acceptance record, public headers, tests, and source diff. Require explicit Spec and Quality verdicts and separate Critical/Important/Minor findings. Independently reproduce the focused core, full CTest, public-header syntax, and at least the complex two-factor oracle.

If a finding exists, create a new narrowly scoped RED regression, make the minimal fix, rerun the affected focused tests and full CTest, obtain a scoped re-review, and append factual acceptance evidence in a non-self-referential documentation commit. If no finding exists, record the PASS verdict in the ignored task report without broadening the public claim to full Python parity.
