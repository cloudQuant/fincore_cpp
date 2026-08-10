# fincore C++ Phase 3A Rolling Scalar Metrics Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add DataFrame-native Python-compatible scalar rolling metrics without
changing the accepted `rolling_engine` or inventing paired/multi-column output
contracts.

**Architecture:** A new public `metrics/rolling.hpp` owns the four scalar
facade functions. Its implementation validates once and builds timestamped
window outputs in supplied frame order. It delegates to accepted scalar metrics
when their behavior matches Python, while `roll_max_drawdown` owns the
necessary raw-NaN cumulative path privately. The existing `rolling_engine` is
unchanged.

**Tech Stack:** C++23, CMake, GoogleTest, cloudQuant/DataFrame, local Python
fincore oracle through `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base
python`, and Homebrew LLVM for ASan+UBSan verification.

---

## Contract shared by every task

- `fincore::dataframe` remains `hmdf::StdDataFrame<fincore::timestamp>` and
  timestamps remain signed UTC Unix nanoseconds. No second DataFrame alias,
  local-time conversion, or timezone dependency is allowed.
- Public inputs call `require_double_column` exactly at their boundary. An
  empty genuine double column is valid and returns an empty result; missing,
  wrong-typed, and short columns preserve `input_error` behavior.
- Preserve row order. Do not sort, align, deduplicate, or add a global
  finite-only filter. NaN behavior is function-specific as documented in the
  design; infinity uses ordinary C++ arithmetic/comparison.
- The output column name is the requested input column. `roll_*` indexes only
  completed-window endpoints; `rolling_*` retains the full input index with
  leading `NaN` values.
- `window == 0` throws `input_error`. A nonempty source shorter than the window
  yields an empty `roll_*` result and a full-index all-`NaN` `rolling_*` result.
  Do not change existing `rolling_engine`, `drawdown_series`, or scalar
  `max_drawdown`.
- Use only public headers in consumer tests, retain optional ImPlot behavior,
  and never fetch, clone, vendor, or fabricate Dear ImGui.

### Task 23: Implement the scalar rolling facade

**Files:**

- Create: `include/fincore/metrics/rolling.hpp`
- Create: `src/metrics/rolling.cpp`
- Create: `tests/unit/rolling_metrics_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Step 1: Generate the Python oracle and write public-header RED tests**

Under Anaconda, run the local `fincore.metrics.rolling` functions with UTC
dates `2021-01-01` through `2021-01-06`, returns
`[.01, -.02, .03, 0, -.01, .02]`, and window `3`:

```sh
PYTHONPATH=/Users/yunjinqi/Documents/new_projects/fincore \
  /Users/yunjinqi/opt/anaconda3/bin/conda run -n base python -c '
import pandas as pd
from fincore.metrics import rolling
index = pd.date_range("2021-01-01", periods=6, freq="D", tz="UTC")
returns = pd.Series([.01, -.02, .03, 0, -.01, .02], index=index)
for function in (rolling.roll_sharpe_ratio, rolling.roll_max_drawdown,
                 rolling.rolling_volatility, rolling.rolling_sharpe):
    print(function.__name__, function(returns, 3).tolist())
'
```

Record the goldens from the design, endpoint/full-index shapes, custom
annualization/frequency behavior, short and one-window behavior, named columns,
original unsorted index preservation, empty genuine double columns, and all
missing/wrong-typed/short structural errors. Add the
critical NaN fixtures:

```cpp
// [0.01, NaN, -0.01], window 3
// roll_sharpe_ratio: 0.0
// roll_max_drawdown: 0.0
// rolling_volatility / rolling_sharpe last output: NaN
```

The test may include only `<fincore/metrics/rolling.hpp>` plus public
construction headers. Register it in `tests/CMakeLists.txt` and add the test
source before implementation.

**Step 2: Run it to verify failure**

Run:

```sh
cmake --build build --parallel 4
ctest --test-dir build -R RollingMetrics --output-on-failure
```

Expected: the build fails because `fincore/metrics/rolling.hpp` and the four
public declarations do not exist.

**Step 3: Implement the minimal order-preserving facade**

Declare exactly these functions:

```cpp
dataframe roll_sharpe_ratio(const dataframe&, std::size_t window = 252U,
                            double risk_free = 0.0,
                            frequency period = frequency::daily,
                            std::optional<double> annualization = std::nullopt,
                            std::string_view column = returns_column);
dataframe roll_max_drawdown(const dataframe&, std::size_t window = 252U,
                            std::string_view column = returns_column);
dataframe rolling_volatility(const dataframe&, std::size_t window,
                             frequency period = frequency::daily,
                             std::optional<double> annualization = std::nullopt,
                             std::string_view column = returns_column);
dataframe rolling_sharpe(const dataframe&, std::size_t window,
                         frequency period = frequency::daily,
                         std::optional<double> annualization = std::nullopt,
                         std::string_view column = returns_column);
```

Use anonymous-namespace helpers to validate a positive window, materialize
complete windows with their original timestamp/index order, and create the two
output shapes. `roll_sharpe_ratio` delegates completed windows to
`sharpe_ratio` using `annualization_factor`; `rolling_volatility` and
`rolling_sharpe` emit `NaN` for any complete window containing a NaN and
otherwise delegate to `annual_volatility`/zero-risk-free `sharpe_ratio`.
Implement `roll_max_drawdown` privately from a raw `1.0` wealth path that lets
NaN contaminate the remainder of a window and takes the minimum non-NaN
drawdown. Do not change accepted scalar drawdown behavior or `rolling_engine`.

Add only `src/metrics/rolling.cpp` to the library sources and
`tests/unit/rolling_metrics_test.cpp` to test sources.

**Step 4: Run focused verification**

Run:

```sh
cmake --build build --parallel 4
ctest --test-dir build -R 'RollingMetrics|Returns|Ratios|Drawdown' --output-on-failure
git diff --check
```

Expected: all focused tests pass and the diff check has no output.

**Step 5: Commit**

```sh
git add include/fincore/metrics/rolling.hpp src/metrics/rolling.cpp \
        tests/unit/rolling_metrics_test.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add scalar rolling metrics"
```

### Task 24: Publish and accept the Phase 3A surface

**Files:**

- Modify: `include/fincore/metrics.hpp`
- Modify: `tests/unit/metrics_umbrella_test.cpp`
- Create: `docs/acceptance/2026-08-10-phase-3a-rolling-scalar-metrics-acceptance.md`
- Modify: `README.md`
- Modify: `tests/CMakeLists.txt` only if test registration changes

**Step 1: Add a public umbrella RED regression**

Extend `MetricsUmbrella` through only `<fincore/metrics.hpp>` to construct a
one-column rolling output. Before adding `metrics/rolling.hpp` to the umbrella,
build and record the expected missing-declaration failure. Start an unchecked
tracked acceptance checklist for public output shape, Python oracle, fresh
core/sanitizer CTest, and the independent ImPlot gate.

**Step 2: Publish and document only accepted behavior**

Include `fincore/metrics/rolling.hpp` from `metrics.hpp`; do not add
`rolling_engine.hpp`, calendar/detail/context/ImPlot headers, or paired rolling
APIs. Document actual full commit SHAs, compiler versions, observed test counts,
and the two rolling output shapes. Explicitly retain the deferred paired
`roll_alpha`/`roll_beta`/capture functions, `roll_alpha_beta`, `rolling_beta`,
`rolling_regression`, portfolio-wide tables, reports, optimization, simulation,
packaging/release, and positive ImPlot build/demo.

**Step 3: Run final verification in fresh directories**

Run exactly (using fresh directories created by `mktemp -d` and preserved):

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

Also configure ImPlot with a fresh invalid `FINCORE_IMGUI_SOURCE_DIR` and
record that it fails with the documented precise diagnostic. Do not attempt a
positive ImPlot build without a real user-supplied Dear ImGui checkout.

**Step 4: Commit**

```sh
git add include/fincore/metrics.hpp tests/unit/metrics_umbrella_test.cpp \
        docs/acceptance/2026-08-10-phase-3a-rolling-scalar-metrics-acceptance.md \
        README.md tests/CMakeLists.txt
git commit -m "docs: accept phase 3A rolling scalar metrics"
```

## Deferred after this plan

Benchmark-paired rolling alpha/beta/capture APIs, multi-column
`roll_alpha_beta` and `rolling_regression`, `rolling_beta`, portfolio positions
and transactions, round trips, performance attribution/stats, Bayesian and
bootstrap functions, optimization, simulation, reports/tearsheets, a full
native facade, packaging/release, and positive ImPlot build/demo remain
separate work. Completing this plan is not full fincore parity or project
completion.
