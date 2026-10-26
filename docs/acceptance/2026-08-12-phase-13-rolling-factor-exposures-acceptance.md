# Phase 13 rolling factor exposures acceptance

## Scope and consumed history

This is a bounded acceptance of the deterministic, DataFrame-native,
no-state rolling factor-exposure surface. It accepts public umbrella export,
strict selected-input behavior, source-derived finite oracle coverage, and the
recorded build gates below. It is not a release, full Python-parity, provider,
model, or GUI-runtime acceptance.

Task 64 consumes both of these complete, already accepted Task 63 revisions:

- implementation: `fe4319be60052f6963a31e341ffc3fac8803184b`
  (`feat: add rolling factor exposures`);
- post-review request-order test hardening:
  `504487779b8c283535c3e66d0dfc1726e81a7c5e`
  (`test: cover factor exposure request order`).

Their historical counts are intentionally not relabelled as Task 64 evidence.
The initial Task 63 leaf-focused and full configured CTest results were `7/7`
and `262/262`; the hardening revision then passed `8/8` and `263/263`.
Task 63's ignored report retains the complete required Anaconda finite Python
oracle for expanding and width-four trailing windows, the genuine public-leaf
RED, and those GREEN results. It also proves exact source goldens, custom
returns selection, raw order, typed empty and short-window output, strict
contracts, metadata exclusion, ownership, and direct public regression parity
for every eligible window. Task 64 does not alter the Task 63 leaf, source,
or hardening test.

A final phase-review boundary correction is recorded in
`44e143fb9d0175a77e5b9158bb99a37f80cd0b88`
(`fix: harden factor exposure selectors`). It retains the normal numerical
behavior while owning a valid bounded return selector before passing it to the
public regression leaf, and rejects embedded-NUL return or factor selectors
before DataFrame C-string name boundaries. This corrects the Task 63 boundary
contract without changing the historical numerical or gate claims above.

## Accepted public contract

`<fincore/attribution.hpp>` now exports:

```cpp
fincore::dataframe rolling_factor_exposures(
    const fincore::dataframe& returns,
    const fincore::dataframe& factor_returns,
    std::span<const std::string_view> factor_column_names,
    std::optional<std::size_t> rolling_window = std::nullopt,
    std::string_view returns_column_name = fincore::returns_column);
```

It is no-state rolling regression over supplied data. With no `rolling_window`,
each endpoint uses the expanding raw history from row zero. A positive width
uses the trailing raw rows ending at that endpoint; `0` is an
`fincore::input_error`. The independently owned result retains the complete
validated returns index and has genuine-`double` columns in exact insertion
order `alpha`, then the caller-supplied factor names. A valid empty input keeps
that typed-empty schema. Rows with fewer than `K + 1` observations have an
ordinary all-`NaN` prefix, so a valid positive width below `K + 1` produces a
full typed result of `NaN` values.

The factor span must be nonempty with nonempty, distinct names that do not use
reserved `alpha`. Selected return/factor columns must exist, be genuine
`double`, have exact owning index lengths, and be finite across the whole
input, even where an insufficient-history prefix would otherwise prevent a
fit. Both raw timestamp indexes must be duplicate-free and exactly equal in
length, value, and supplied order. The operation does not positional-zip,
align on common timestamps, sort, resample, deduplicate, or repair inputs;
unrelated malformed metadata is ignored and factor names are not inferred from
metadata. Each eligible raw window delegates to public
`fit_factor_regression`. An eligible rank-deficient window propagates
`input_error`; it never publishes a minimum-norm solution or a fit-failure
`NaN`.

This deliberately excludes a stateful model receiver, a full Fama-French
model/provider, Newey-West, standard errors, p-values, risk-free adjustment,
factor provider/fetch/cache, factor prediction, attribution decomposition,
idiosyncratic risk, generic rolling-metric changes, external numerical
dependencies, reports/releases, full Python parity, and GUI runtime. It does
not change the Phase 12 factor-regression solver or duplicate its QR work.

## Umbrella-only RED and GREEN

`tests/unit/attribution_factor_exposures_umbrella_test.cpp` includes only
`<fincore/attribution.hpp>` among fincore headers. It neither includes the
factor-exposures/factor-regression leaves, metrics, nor source headers. Its
one-factor exact-fit frame verifies the `alpha`, `MKT` genuine-double schema,
the ordinary first-row `NaN` prefix, and the fitted endpoint `alpha == 1.0`,
`MKT == 2.0`.

Before adding the umbrella include, CMake regenerated the registered consumer
and `cmake --build build --parallel 4` exited `1` solely at:

```text
error: no member named 'rolling_factor_exposures' in namespace 'fincore'
```

CTest was deliberately not run after that compilation failure: a pre-existing
`fincore_tests` executable would be stale and is not RED evidence. The ignored
Task 64 log records the failed build, status `1`, and the empty prohibited-leaf
include audit.

After adding exactly `<fincore/attribution/factor_exposures.hpp>` to the
attribution umbrella, the configured build succeeded. The focused
`AttributionFactorExposuresUmbrella` CTest passed `1/1`, and the configured
full CTest passed `264/264`. The configured AppleClang build/test logs had no
`warning:` match; the consumer's prohibited-leaf include audit remained a
no-match.

## Fresh retained gates

All fresh terminal output was retained with `tee` in the listed unique
directories. Each positive gate ran configure, build, then CTest before its
scanner command. No scanner ran before that gate's CTest finished. CMake was
`4.0.2`.

### AppleClang headless

Fresh directory: `/tmp/fincore-phase13-apple.DoTdS5`. `/usr/bin/c++ --version`
reported Apple clang `17.0.0 (clang-1700.6.3.2)`; CMake identified
`AppleClang 17.0.0.17000603`.

```bash
cmake -S . -B /tmp/fincore-phase13-apple.DoTdS5 \\
  -DCMAKE_BUILD_TYPE=Debug -DFINCORE_BUILD_IMPLOT=OFF
cmake --build /tmp/fincore-phase13-apple.DoTdS5 --parallel 4
ctest --test-dir /tmp/fincore-phase13-apple.DoTdS5 --output-on-failure
```

All commands exited `0`; CTest passed `264/264`. Only after CTest, the
`warning:` scan over configure/build/CTest logs completed with `rg` status `1`
(expected no match); `warnings-scan-final.log` records that result.

### Homebrew LLVM 22 ASan+UBSan headless

Fresh directory: `/tmp/fincore-phase13-llvm.kLhhts`.
`/opt/homebrew/opt/llvm/bin/clang++ --version` reported Homebrew clang
`22.1.8`; CMake used Clang `22.1.8` for C and C++. The fresh configure
succeeded directly, so there was no FetchContent/DNS/network failure and no
retry directory.

```bash
cmake -S . -B /tmp/fincore-phase13-llvm.kLhhts \\
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \\
  -DCMAKE_BUILD_TYPE=Debug -DFINCORE_ENABLE_ASAN=ON \\
  -DFINCORE_ENABLE_UBSAN=ON -DFINCORE_BUILD_IMPLOT=OFF
cmake --build /tmp/fincore-phase13-llvm.kLhhts --parallel 4
ctest --test-dir /tmp/fincore-phase13-llvm.kLhhts --output-on-failure
```

All commands exited `0`; CTest passed `264/264`. After CTest, the retained
scanner found no `AddressSanitizer`, `UndefinedBehaviorSanitizer`, or
`runtime error` diagnostic (`rg` status `1`, expected no match). The separate
post-CTest warning scan found one third-party GoogleTest
`-Wcharacter-conversion` warning at `gtest-printers.h:524` for an implicit
`char8_t` to `char32_t` conversion; it is recorded, not treated as a fincore
or sanitizer failure.

### ImPlot and demo compile/link boundary

Fresh directory: `/tmp/fincore-phase13-implot.1LQHoO`. AppleClang configured
with local Dear ImGui/ImPlot defaults, `FINCORE_BUILD_IMPLOT=ON`, and
`FINCORE_BUILD_IMPLOT_DEMO=ON`.

```bash
cmake -S . -B /tmp/fincore-phase13-implot.1LQHoO \\
  -DCMAKE_BUILD_TYPE=Debug -DFINCORE_BUILD_IMPLOT=ON \\
  -DFINCORE_BUILD_IMPLOT_DEMO=ON
cmake --build /tmp/fincore-phase13-implot.1LQHoO --parallel 4
ctest --test-dir /tmp/fincore-phase13-implot.1LQHoO --output-on-failure
```

All commands exited `0`; the build linked `fincore_implot_demo`, and CTest
passed `265/265`, including `ImPlotAdapterCompile`. The post-CTest warning
scan recorded exactly the three macOS OpenGL deprecation warnings in
`examples/implot_dashboard.cpp` for `glViewport`, `glClearColor`, and
`glClear`. `fincore_implot_demo` is not a CTest registration, and no GUI
executable was launched: this is compile/link and headless CTest evidence only.

### Invalid ImGui override

Fresh directory: `/tmp/fincore-phase13-invalid-imgui.lK9eG6`. The expected
negative configure command exited `1`:

```bash
cmake -S . -B /tmp/fincore-phase13-invalid-imgui.lK9eG6 \\
  -DFINCORE_BUILD_IMPLOT=ON \\
  -DFINCORE_IMGUI_SOURCE_DIR=/definitely-invalid-imgui
```

After the expected failure, this required multiline diagnostic check ran with
`rg -n -U 'FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required\s+files'`:

```text
FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required
files; received '/definitely-invalid-imgui'
```

This confirms the dependency-validation error only; it does not invalidate the
positive default-path ImPlot compile/link gate.

## Scope, retained report, and deferred work

The ignored retained record is
`.superpowers/sdd/2026-08-12-fincore-cpp-phase-13-rolling-factor-exposures/task-64-report.md`;
its sibling `task-64-logs/` retains all commanded terminal logs, exact paths,
statuses, compiler versions, CTest summaries, scanner timing, warning matches,
and invalid-ImGui output. No LLVM retry evidence exists because its fresh
configure did not fail.

This Task 64 acceptance changes only the attribution umbrella, its isolated
consumer and CMake registration, README, and this acceptance record. It does
not expand accepted scope to providers/models, numerical solver changes,
interactive GUI validation, packaging/release validation, or full Python API
parity.

## Task 65 traceability

- Complete Task 63 implementation commit: `fe4319be60052f6963a31e341ffc3fac8803184b`.
- Task 64 initial-acceptance commit: `f57bd479c276b95348e48647a5f2fc31d4e539a3`.

Task 65 is documentation-only traceability work and does not amend either
prior commit.
