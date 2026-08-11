# Phase 15 factor-regression prediction acceptance

## Scope and consumed history

This bounded acceptance covers deterministic, DataFrame-native consumption of
a supplied factor-regression result to produce predictions. It accepts the
attribution-umbrella export, strict model/selector validation, raw-index
preservation, typed-empty behavior, isolated consumer, and retained headless
build evidence. It is not a release, stateful Python-model, provider,
full-Python-parity, or GUI-runtime acceptance.

Task 70 consumes the complete Task 69 implementation revision:

- `539de95ce8d2475dc018e61d331bf56f78c75b27`
  (`feat: add factor regression predictions`).

Task 69's ignored report retains the complete local Anaconda source oracle,
genuine public-leaf RED, configured GREEN/full-suite evidence, and scope
audit. Its fixed eight-row `MKT`/`SMB`/`HML` source predictions are:

```text
0.0068203580214942685
-0.02992443386996989
0.0533598324629174
-0.015348761635518682
0.029356958651646607
-0.0227002927762671
0.05139868826161392
-0.0009623491159163553
```

They came from the local Python source checkout through exactly
`/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`, running
`FamaFrenchModel("3factor").fit(...); model.predict(factors)` on the plan's
fixed fixture. The native operation is deliberately stateless: it consumes a
caller-supplied `factor_regression_result` rather than creating a Python-like
stateful model object or provider lifecycle.

## Accepted public contract

`<fincore/attribution.hpp>` exports:

```cpp
fincore::dataframe factor_regression_predictions(
    const fincore::factor_regression_result& regression,
    const fincore::dataframe& factor_returns,
    std::span<const std::string_view> factor_column_names);
```

The explicit selected-factor span is nonempty. Every name is nonempty,
distinct, free of embedded NUL, and not reserved `alpha`; selectors are owned
before reaching the DataFrame C-string boundary, so a bounded
non-NUL-terminated view is valid. The supplied public regression value is
validated strictly only through its consumed `alpha` and `coefficients`:
`alpha` is finite and exactly equals row zero of the coefficient values; the
coefficient table has exactly a genuine `std::string factor` column and a
genuine `double coefficient` column, each with owning length `K + 1`, ordinal
index `0..K`, labels `alpha` followed by the caller's factor names in exact
order, and finite values. `r_squared`, `fitted_returns`, and `residuals` are
not consumed.

Every selected factor column must exist, be genuine `double`, match the raw
prediction index length, and be finite. Unselected malformed metadata is
irrelevant. The result retains the raw supplied prediction index and row order
exactly, including nonmonotonic values and duplicates: no alignment, sorting,
deduplication, training-index comparison, or refit takes place. It owns that
index and its values independently. The result has exactly one genuine
`double` `predicted_returns` column and no extras; each row is evaluated in
requested selector order as `alpha + sum(beta[j] * factor[j])`. A nonfinite
product, running sum, or result raises `fincore::input_error`. A valid model
and selected typed-empty frame yield a typed-empty output column.

This is a bounded native prediction consumer, not a stateful
`FamaFrenchModel`, standard 3/5/momentum selection, provider/fetch/cache,
refit, QR/OLS change, residual/idiosyncratic-risk, rolling-exposure,
attribution, Newey-West/p-value, Python-dict, report/release, external
numerical dependency, full-Python-parity, or GUI-runtime surface.

## Umbrella-only RED and GREEN

`tests/unit/attribution_factor_prediction_umbrella_test.cpp` includes only
`<fincore/attribution.hpp>` among fincore headers. It has no
factor-prediction, factor-regression, exposure, metric, or source-header
include. It makes a minimal full-rank one-factor fit, then predicts a small
nonmonotonic duplicate-index factor frame and verifies that exact raw index,
the sole genuine-double column, and all three direct prediction values.

Before the umbrella export existed, this newly registered consumer was built
with:

```sh
cmake --build build --parallel 4
```

The command exited `1` solely because the attribution umbrella did not export
`factor_regression_predictions` or `factor_prediction_column`. CTest was not
run after that compilation failure, so no stale `fincore_tests` executable was
used as RED evidence. The retained log is
`/tmp/fincore-phase15-task70-umbrella-red.etiUcN/build.log`.

After adding exactly
`#include <fincore/attribution/factor_prediction.hpp>` to the attribution
umbrella, the configured build succeeded. The isolated
`AttributionFactorPredictionUmbrella` CTest passed `1/1`; the relevant
prediction/regression/exposure/attribution/DataFrame selection passed
`116/116`; and the configured full headless CTest suite passed `283/283`.
`git diff --check` had no output. A post-GREEN include audit confirms the
consumer has no prohibited direct prediction, regression, exposure,
attribution, metric, or source-header include.

## Fresh retained gates

Every fresh command's terminal output was retained with `tee` in a unique
directory. Positive gates configured, built, and completed CTest before their
post-CTest scans. No GUI executable was launched. CMake was `4.0.2`.

### AppleClang headless

Fresh directory: `/tmp/fincore-phase15-apple.OdEdmr`. `/usr/bin/c++ --version`
reported Apple clang `17.0.0 (clang-1700.6.3.2)`; CMake identified AppleClang
`17.0.0.17000603` for C and C++.

```sh
cmake -S . -B /tmp/fincore-phase15-apple.OdEdmr -DCMAKE_BUILD_TYPE=Debug -DFINCORE_BUILD_IMPLOT=OFF
cmake --build /tmp/fincore-phase15-apple.OdEdmr --parallel 4
ctest --test-dir /tmp/fincore-phase15-apple.OdEdmr --output-on-failure
```

All commands exited `0`; CTest passed `283/283`. Only after CTest, the
retained `warning:` scan over configure, build, and CTest logs completed with
`rg` status `1` (expected no match), recorded in `warnings-scan-final.log`.

### Homebrew LLVM 22 ASan+UBSan headless

Fresh directory: `/tmp/fincore-phase15-llvm.p31C2b`.
`/opt/homebrew/opt/llvm/bin/clang++ --version` reported Homebrew clang
`22.1.8`; CMake identified Clang `22.1.8` for C and C++. The fresh configure
succeeded directly, so no FetchContent/DNS/network failure occurred and no
retry directory was required.

```sh
cmake -S . -B /tmp/fincore-phase15-llvm.p31C2b -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ -DCMAKE_BUILD_TYPE=Debug -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON -DFINCORE_BUILD_IMPLOT=OFF
cmake --build /tmp/fincore-phase15-llvm.p31C2b --parallel 4
ctest --test-dir /tmp/fincore-phase15-llvm.p31C2b --output-on-failure
```

All commands exited `0`; CTest passed `283/283`. The retained sanitizer scan
found no `AddressSanitizer`, `UndefinedBehaviorSanitizer`, or `runtime error`
diagnostic (`rg` status `1`, expected no match). A separate warning inspection
found one third-party GoogleTest `-Wcharacter-conversion` warning at
`gtest-printers.h:524` for implicit `char8_t` to `char32_t` conversion; it is
recorded rather than treated as a fincore or sanitizer failure.

### ImPlot and demo compile/link boundary

The first fresh directory, `/tmp/fincore-phase15-implot.xlyugI`, configured,
built, and passed `284/284` CTest cases, including `ImPlotAdapterCompile`; its
build log records `Linking CXX executable fincore_implot_demo`. The plan's
post-CTest existence check expected
`/tmp/fincore-phase15-implot.xlyugI/examples/fincore_implot_demo`, but this
CMake target has no runtime-output override and was actually emitted at the
build root as `/tmp/fincore-phase15-implot.xlyugI/fincore_implot_demo`.
Consequently that planned path assertion exited nonzero after the successful
compile/link and CTest evidence; it did not indicate a failed build or an
unlaunched demo.

To make the artifact-path correction independently reproducible without
changing production configuration, a second fresh directory,
`/tmp/fincore-phase15-implot-retry.SeG8Bn`, ran the same configure, build, and
CTest commands and then verified the actual target path:

```sh
cmake -S . -B /tmp/fincore-phase15-implot-retry.SeG8Bn -DCMAKE_BUILD_TYPE=Debug -DFINCORE_BUILD_IMPLOT=ON -DFINCORE_BUILD_IMPLOT_DEMO=ON
cmake --build /tmp/fincore-phase15-implot-retry.SeG8Bn --parallel 4
ctest --test-dir /tmp/fincore-phase15-implot-retry.SeG8Bn --output-on-failure
test -x /tmp/fincore-phase15-implot-retry.SeG8Bn/fincore_implot_demo
```

All four commands exited `0`; CTest passed `284/284`, `ImPlotAdapterCompile`
passed, and the demo was compiled/linked but never executed. The only warnings
were the expected three macOS OpenGL deprecation diagnostics in
`examples/implot_dashboard.cpp` for `glViewport`, `glClearColor`, and
`glClear`.

### Invalid ImGui override

Fresh directory: `/tmp/fincore-phase15-invalid-imgui.ZoBDaK`. The expected
negative configure command exited `1`:

```sh
cmake -S . -B /tmp/fincore-phase15-invalid-imgui.ZoBDaK -DFINCORE_BUILD_IMPLOT=ON -DFINCORE_IMGUI_SOURCE_DIR=/definitely/not/a/dear-imgui-checkout
```

Its retained diagnostic is:

```text
FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required
files; received '/definitely/not/a/dear-imgui-checkout'
```

The plan's exact `rg -U` pattern used `[[:space:]]+`, which did not match this
line break under the local ripgrep implementation despite the complete
diagnostic being present. A portable multiline whitespace pattern and the two
literal fragments both matched the retained log. This is recorded as a test
pattern portability discrepancy, not as a successful positive configuration
or a GUI-runtime claim.

## Scope and retained report

The ignored Task 70 record is
`.superpowers/sdd/2026-08-12-fincore-cpp-phase-15-factor-regression-prediction/task-70-report.md`.
The tracked Task 70 scope is limited to the attribution umbrella, this
isolated consumer and its registration, README, and this acceptance record.
It does not alter the Task 69 leaf, factor-regression solver, previous phase
implementations, third-party configuration, or ImPlot source.

## Commit traceability

Task 69 is the implementation revision
`539de95ce8d2475dc018e61d331bf56f78c75b27`
(`feat: add factor regression predictions`). Its direct parent is the Phase 15
plan revision `8513c211ed42ef4f7bec385b5f4f3233defef10f`, and its exact tracked
path scope is:

- `CMakeLists.txt`
- `tests/CMakeLists.txt`
- `include/fincore/attribution/factor_prediction.hpp`
- `src/attribution/factor_prediction.cpp`
- `tests/unit/factor_prediction_test.cpp`

Task 70 is the initial acceptance revision
`1632b043a229181fd11b635afd43da0bbe8113d0`
(`docs: accept phase 15 factor regression predictions`), whose direct parent
is Task 69. Its distinct exact tracked path scope is:

- `include/fincore/attribution.hpp`
- `tests/CMakeLists.txt`
- `tests/unit/attribution_factor_prediction_umbrella_test.cpp`
- `README.md`
- `docs/acceptance/2026-08-12-phase-15-factor-regression-prediction-acceptance.md`

Task 71 is documentation-only traceability. It does not amend either prior
revision, and this record deliberately neither states nor anticipates Task
71's own commit SHA. No review-correction revision is represented by this
trace entry.
