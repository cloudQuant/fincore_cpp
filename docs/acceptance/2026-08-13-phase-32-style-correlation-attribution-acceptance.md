# Phase 32 native style correlation attribution acceptance

## Candidate, route, and exact scope

This record accepts Plan A: a pure native C++23 implementation over the local
cloudQuant DataFrame, without an embedded Python/pandas runtime. Phase 32
completes the successful local calculation path in Python
`fincore.attribution.style` by adding style correlation attribution. The
provider-only `fetch_style_factors` stub remains excluded because it has no
successful source path without separately designed provider policy.

The reviewed Phase 32 design and implementation plan is
`3fb3ecb2aa24d45fa4e163eb62ca70efddce0100`, whose exact two-path scope is:

- `docs/design/2026-08-13-phase-32-style-correlation-attribution-design.md`
- `docs/plans/2026-08-13-fincore-cpp-phase-32-style-correlation-attribution.md`

Task 137 implementation commit
`890a4ae4dfa186a9929b550f6739414115f42f7b` (`feat: add native style
correlation attribution`) is its direct child and has exactly this five-path
scope:

- `include/fincore/attribution/style.hpp`
- `src/attribution/style.cpp`
- `tests/CMakeLists.txt`
- `tests/unit/style_regression_attribution_test.cpp`
- `tests/unit/attribution_style_regression_attribution_umbrella_test.cpp`

The direct follow-up `77b33470af02ddffdd58e246f96300d813880b64`
(`test: harden style attribution boundaries`) changes only
`tests/unit/style_regression_attribution_test.cpp`. It adds below-threshold
standard-deviation cases for both sides and a constructible empty portfolio
column-name failure; it makes no production change.

The initial Task 138 acceptance commit is
`931dcb986c18d197dbd145ee7d6377ae7201a4de` (`docs: accept phase 32 style
correlation attribution`). It is the direct child of `77b3347` and changes
exactly:

- `README.md`
- `docs/acceptance/2026-08-13-phase-32-style-correlation-attribution-acceptance.md`

Thus the accepted implementation and review-fix trace is
`890a4ae4dfa186a9929b550f6739414115f42f7b` ->
`77b33470af02ddffdd58e246f96300d813880b64`, followed directly by the initial
two-path acceptance `931dcb986c18d197dbd145ee7d6377ae7201a4de`.
Task 139 adds only this historical trace to the acceptance document; it does
not amend any earlier commit and neither states nor anticipates its own SHA.

Final independent Task 137 specification and task-quality reviews both
returned **PASS**, with Critical/Important/Minor `0/0/0`. At this review
handoff the Task 138 documentation candidate consisted only of `README.md`
and this acceptance record; both were kept unstaged and uncommitted for a
fresh independent document/gate review. That review returned Spec **PASS**
and Task-Quality **PASS**, with Critical/Important/Minor `0/0/0`, before the
two-path acceptance commit was created.

## Exact API and source mapping

The existing `<fincore/attribution.hpp>` facade includes the style leaf, which
declares exactly these constants and overloads:

```cpp
inline constexpr std::string_view style_attribution_contribution_column{
    "contribution"};
inline constexpr std::string_view style_attribution_residual_name{"residual"};

[[nodiscard]] dataframe calculate_regression_attribution(
    const dataframe& asset_returns);

[[nodiscard]] dataframe calculate_regression_attribution(
    const dataframe& portfolio_returns,
    const dataframe& style_returns,
    std::span<const std::string_view> selected_styles);
```

The one-argument overload maps Python's automatic DataFrame path. The explicit
overload maps both source portfolio forms without introducing a native variant:
one genuine-double portfolio column is the Series-shaped case, while multiple
genuine-double columns are averaged across columns at each row before
attribution.

Python's explicit `style_exposures` frame is not accepted as a native
overload. The source never reads its rows, values, types, or index; it uses
only exposure column membership. Native `selected_styles` therefore expresses
the actual contract directly. Selectors are copied and validated before any
C-string DataFrame lookup. A selector absent from `style_returns` is ignored,
and selector order does not reorder output: selected styles remain in stable
`style_returns` double-column order. An empty selector emits only the
residual row. Compile-time tests reject an exposure-DataFrame third argument.

## Correlation formula and alignment

The source calls its coefficient `style_beta`, but it does not fit an OLS
model and does not calculate covariance divided by style variance. For each
selected style `j`, native code uses checked centered online moments:

```text
common_j       = unique timestamp intersection in portfolio-left order

if size(common_j) < 3:
    contribution_j = 0
else:
    p_std = population_std(portfolio on common_j)
    s_std = population_std(style_j on common_j)
    if p_std < 1e-15 or s_std < 1e-15:
        contribution_j = 0
    else:
        corr_j = population_covariance(portfolio, style_j) / (p_std * s_std)
        contribution_j = corr_j * mean(style_j on common_j)

portfolio_mean = mean(portfolio over every portfolio row)
residual       = portfolio_mean - sum(contribution_j)
```

The standard-deviation comparison is strictly `<`, so an exactly `1e-15`
deviation participates in correlation. Correlation is not clamped. Tests use
a non-perfect centered Pearson fixture that differs from both an uncentered
dot product and an OLS slope, and separately lock the common-only style mean
and full-portfolio residual mean.

Explicit mode requires unique portfolio and style-return timestamps but does
not require monotonic order. It builds the common pairs in portfolio-left
order. This intentionally rejects pandas duplicate-label
`.intersection(...).loc[...]` expansion, which can broadcast or fail based on
duplicate multiplicities.

For a nonempty-shaped automatic input, the composer calls public
`style_analysis(asset_returns)` exactly once and public
`analyze_performance_by_style(asset_returns, analysis.exposures)` exactly
once. It consumes `analysis.overall_returns["return"]`, maps each ordered
exposure style to its `{style}_return` performance column, strips the suffix
in output, and pairs rows positionally. It never aligns the ordinal
performance index to raw timestamps, so duplicate and nonmonotonic raw input
labels remain valid.

## Output, validation, native divergences, and ownership

The result owns an ordinal timestamp index `0..N`, one genuine
`std::string` column named `style`, and one genuine-double column named
`contribution`. Selected style rows appear first in stable style-return order;
the literal `residual` row is always last. There are no hidden columns and no
input timestamp is reused as output identity.

An empty-shaped automatic input or explicit portfolio input returns a fresh
canonical `0x0` DataFrame before nonempty validation. For a nonempty
portfolio, no selected/present styles returns one residual row. A selected
zero-row style or a style with fewer than three common rows receives zero
contribution.

Nonempty explicit portfolio frames must contain at least one genuine-double,
exact-index-length, finite column and no other typed column. Portfolio names
must be nonempty, embedded-NUL-free, and distinct. Explicit indexes must be
unique. Selectors must be nonempty, embedded-NUL-free, distinct, fit
cloudQuant DataFrame's fixed column-name capacity with terminator room, and
must not be `residual`. Present selected columns must be genuine double,
exact-index-length, and finite; unselected columns are not read and may be
wrong-typed, short, unnamed, or nonfinite. Counts, ordinal labels, cells,
names, row sums, centered moments, covariance, correlation, contributions,
attributed sum, residual, lengths, and allocations are checked. Public
allocation, malformed-library, capacity, duplicate-index, and arithmetic
failures cross as `fincore::input_error`.

These are deliberate native divergences from Python:

- a style-name selector replaces the unread exposure DataFrame values;
- a value-owning two-column DataFrame replaces the insertion-ordered `dict`;
- explicit duplicate timestamps are rejected instead of reproducing pandas
  label broadcasting;
- nonfinite portfolio and selected-style values are rejected instead of
  dropping NaN pairs or allowing infinity to leak through warnings/results;
  and
- selector and output-name capacity are checked before C-string boundaries.

Selectors are copied before lookup, and the materialized index, style strings,
and contributions retain no input or intermediate views. Tests mutate the
portfolio, style returns, and selector backing strings after the call and also
exercise scope destruction.

## Mandatory Python source oracle

Task 137 ran the plan's exact oracle from
`/Users/yunjinqi/Documents/new_projects/fincore` through
`/Users/yunjinqi/opt/anaconda3/bin/conda run --no-capture-output -n base
python`. It exited `0` and printed:

```text
EXPLICIT {'growth': 0.06, 'value': -0.03, 'residual': 0.0}
EXPLICIT_FRAME {'growth': 0.018000000000000006, 'value': -0.009000000000000003, 'residual': 0.061000000000000006}
AUTOMATIC {'equal_weight': 0.009333333333333332, 'winner': 0.002613098319673955, 'loser': 0.0007593263966019986, 'value': 0.009333333333333332, 'growth': 0.009333333333333332, 'high': 0.0, 'low': 0.0, 'residual': -0.02203909138294262}
SHORT {'growth': 0.0, 'residual': 0.03}
```

The primary explicit fixture has portfolio index `[30,10,50,20,40]`, style
index `[40,20,50,10,30]`, style columns in `growth,value,unselected` order,
and selector membership in `value,growth` order. This jointly proves
timestamp reordering, style-return order rather than selector order, centered
correlation, unread exposure values, and residual conservation. The complete
retained source record is
`.superpowers/sdd/2026-08-13-fincore-cpp-phase-32-style-correlation-attribution/source-oracle.md`.

## Genuine RED, GREEN, and reviews

Both consumers and their CMake registrations existed before the production
constants or declarations. The leaf consumer included only
`<fincore/attribution/style.hpp>` among fincore headers; the facade consumer
included only `<fincore/attribution.hpp>`. On baseline `3fb3ecb`,
`cmake --build build --parallel 4` exited `1`, and both translation units
failed only because `calculate_regression_attribution` and the two planned
constants were absent. No CTest command ran after that failed build, so no
stale executable is cited as RED evidence.

Task 137's implementation GREEN passed focused `15/15`, related `87/87`, and
full `434/434` CTest; diff checks were clean. The specification review found
no API or semantic defect; its evidence-governance notes were corrected in
ignored evidence and the scoped re-review returned PASS, C/I/M `0/0/0`.
The code-quality review found no production correctness, lifetime, UB,
exception, schema, or alignment defect, but requested two mutation-resistant
test boundaries. Commit `77b3347` added those tests only. Post-hardening
focused `15/15` and full `434/434` remained green, and the independent scoped
quality re-review returned PASS, C/I/M `0/0/0`.

## Task 138 current and fresh gates

Raw Task 138 logs are under
`.superpowers/sdd/2026-08-13-fincore-cpp-phase-32-style-correlation-attribution/task-138-logs/`.

### Current configured tree

- `cmake --build build --parallel 4`: exit `0`, no work.
- `ctest --test-dir build -N`: exactly `434` tests.
- focused
  `StyleRegressionAttribution|AttributionStyleRegressionAttributionUmbrella`:
  `15/15`, exit `0`.
- full CTest: `434/434`, exit `0`.

The raw current logs are in the `current/` subdirectory.

### Fresh AppleClang 17 Debug headless

Root: `/tmp/fincore-phase32-task138-apple.yG59sA`. The explicit configure used
Ninja, `/usr/bin/c++`, Debug, local DataFrame, testing ON, ImPlot/demo OFF,
and ASan/UBSan OFF. Configure and build exited `0`; Ninja completed `116/116`.
`ctest -N` registered exactly `434`; focused passed `15/15`; full passed
`434/434`. A case-insensitive warning scan over configure/build logs had no
match.

### Fresh Homebrew LLVM 22 ASan+UBSan headless

Root: `/tmp/fincore-phase32-task138-llvm.uli6c4`. The explicit configure used
Ninja, `/opt/homebrew/opt/llvm/bin/clang++` 22.1.8, Debug, local DataFrame,
testing ON, ImPlot/demo OFF, and both sanitizer options ON. Configure and
build exited `0`; Ninja completed `116/116`. `ctest -N` registered exactly
`434`. Full CTest ran with `ASAN_OPTIONS`, `UBSAN_OPTIONS`, and the LLVM
symbolizer and passed `434/434` in 105.77 seconds.

A raw scan found no AddressSanitizer, UndefinedBehaviorSanitizer,
LeakSanitizer, sanitizer-summary, or runtime-error diagnostic. Ninja's command
graph contains 35 address and 35 undefined sanitizer flags, and the final
test binary loads LLVM 22's
`libclang_rt.asan_osx_dynamic.dylib`. The sole build warning is upstream
GoogleTest `gtest-printers.h:524`, a `char8_t` to `char32_t`
`-Wcharacter-conversion`; it is neither fincore code nor a sanitizer finding.

### Fresh local ImGui/ImPlot compile-link gate

Root: `/tmp/fincore-phase32-task138-implot.Uuox5h`. AppleClang 17 Debug/Ninja
used testing, ImPlot, and demo all ON, with the local DataFrame, ImGui, and
ImPlot paths. Configure and build exited `0`; Ninja completed `130/130`.
`ctest -N` registered exactly `435`, including `#435 ImPlotAdapterCompile`;
full CTest passed `435/435`.

The build-root `fincore_implot_demo` is a non-symlink regular Mach-O arm64
executable. It is not CTest-registered and was never invoked; no GUI was
launched. The only build warnings are three known macOS OpenGL deprecations in
`examples/implot_dashboard.cpp`: `glViewport`, `glClearColor`, and `glClear`.
This is compile/link and adapter-test evidence only, not plotting appearance,
interaction, or runtime acceptance.

### Fresh invalid-ImGui negative configure

Root: `/tmp/fincore-phase32-task138-invalid-imgui.hX8q6X`. The explicit
nonexistent-path precondition passed. Configure with ImPlot ON, demo/testing
OFF, and
`FINCORE_IMGUI_SOURCE_DIR=/tmp/fincore-phase32-task138-invalid-imgui.hX8q6X/not-a-dear-imgui-checkout`
exited exactly `1` at `cmake/fincore_implot.cmake:47`. A multiline
whitespace-tolerant `rg -U -P` expression matched the complete diagnostic and
exact missing path, exit `0`:

```text
FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required
files; received
'/tmp/fincore-phase32-task138-invalid-imgui.hX8q6X/not-a-dear-imgui-checkout'
```

## Dependency provenance and exclusions

Every fresh cache points `FINCORE_DATAFRAME_SOURCE_DIR` to the local checkout
`/Users/yunjinqi/Documents/new_projects/DataFrame`, HEAD
`90aa4acf6437d5a34f7ab8dfb6e9250c3d53d230`, origin
`https://github.com/cloudQuant/DataFrame.git`. GoogleTest is the project's
actual official FetchContent dependency, origin
`https://github.com/google/googletest.git`, HEAD
`b514bdc898e2951020cbdca1304b75f5950d1f59`, exact tag `v1.15.2`; it is not a
substituted local checkout.

The graphics gate uses local ImGui HEAD
`46d39d56febc2a00bdd2270dc88c8a13f2a0441a`, origin
`https://github.com/ocornut/imgui.git`, and local cloudQuant ImPlot HEAD
`7eeb9168d2e5e6b14e266d8782ecf7e649dfc3a4`, origin
`https://github.com/cloudQuant/implot.git`. The demo resolved Homebrew GLFW
`3.4` through `/opt/homebrew/lib/cmake/glfw3` and linked
`/opt/homebrew/lib/libglfw.3.4.dylib`; OpenGL resolved to the Apple SDK
framework. No ImGui, ImPlot, or DataFrame source was fetched or fabricated by
the gate.

Acceptance covers only deterministic native style correlation attribution and
its public facade. It does not add an exposure-value regression, OLS, QR or
another solver, `fetch_style_factors`, any provider/network/cache policy,
Python/pandas embedding, factor-regression changes, plots or reports,
packaging/release work, or GUI runtime behavior. ImPlot evidence is strictly
compile/link and adapter-test evidence.
