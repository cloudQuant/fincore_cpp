# Phase 21 bootstrap confidence-interval acceptance

## Public boundary and accepted implementation

Task 92 is exactly
`f15dbb95eaf1ebb4c0d2f4d4a17958c85c84789d` (`feat: add bootstrap confidence
interval`).  Its implementation scope is exactly:

- `include/fincore/simulation/bootstrap.hpp`
- `src/simulation/bootstrap.cpp`
- `tests/CMakeLists.txt`
- `tests/unit/bootstrap_confidence_interval_test.cpp`
- `tests/unit/simulation_bootstrap_confidence_interval_umbrella_test.cpp`

No façade change belongs to this phase.  The exact public boundary is that
`<fincore/simulation.hpp>` includes `<fincore/simulation/bootstrap.hpp>`;
Phase 20 established that include, and Task 92 extended the already-exported
leaf.  The Task 92 umbrella consumer includes only
`<fincore/simulation.hpp>` among fincore headers and successfully calls
`bootstrap_confidence_interval`.  That automatic export is the GREEN evidence,
so changing `simulation.hpp` would be redundant.

The accepted API is a value-owning two-double result and:

```cpp
bootstrap_confidence_interval_result bootstrap_confidence_interval(
    const dataframe& returns,
    std::size_t resample_count = 10000U,
    double alpha = 0.05,
    bootstrap_statistic statistic = bootstrap_statistic::mean,
    std::optional<std::uint64_t> seed = std::nullopt,
    std::string_view column = returns_column);
```

It first rejects nonfinite `alpha` and values outside `0 < alpha < 1`, then
delegates exactly once to public `bootstrap_distribution`, forwarding every
distribution argument unchanged.  It neither duplicates sampling, statistics,
nor RNG work.  From the copied, nonempty, finite generated statistics, it sorts
and calculates source `percentile` endpoints at `alpha / 2` and
`1 - alpha / 2`: for percentile `p` and `n` ordered values, the position is
`p * (n - 1)` and adjacent order statistics are linearly interpolated.  The
native seeded result is repeatable in the supported C++ implementation only;
it does not claim NumPy random-stream parity.

## Source oracle and intentional boundary differences

Task 92 ran its source oracle from `/Users/yunjinqi/Documents/new_projects/fincore`
using `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`.  The actual
recorded source cases are:

| Case | Python source result |
| --- | --- |
| Nonconstant mean, five resamples, `alpha=0.2`, seed 42 | `(0.004999999999999999, 0.01)` |
| Constant mean, four resamples, seed 42 | `(0.01, 0.01)` |
| Constant standard deviation, four resamples, seed 42 | `(0.0, 0.0)` |
| NaN-drop constant mean, three resamples, seed 42 | `(0.01, 0.01)` |
| `alpha` of `0`, `1`, `nan`, or `inf` | `ValueError` |
| Zero resample count | `IndexError` from source empty percentile |
| Constant Sharpe statistic | NumPy warning and `(nan, nan)` |

The C++ interval deliberately differs at the last two boundaries: a zero-count
generated distribution and any generated `NaN`, `+inf`, or `-inf` statistic
are rejected as `fincore::input_error` before sorting.  Thus the source oracle
anchors the finite source-compatible cases and alpha behavior, but it is not a
claim of Python warning/nonfinite-percentile or RNG-stream parity.

## RED, GREEN, and configured gate

Before Task 92 production declarations, the registered leaf-only consumer
(`bootstrap.hpp` only) and umbrella-only consumer (`simulation.hpp` only) were
built.  The build exited `1` solely because the new result/function were
absent from the leaf and the confidence-interval function was absent from the
umbrella.  CTest was not run after that expected failure, so no stale binary is
used as RED evidence.

After the accepted leaf change, the focused public-consumer GREEN passed `8/8`.
The prescribed selected suite passed `71/71`, and the full configured suite
passed `332/332`.  Task 93 reran the configured build and full CTest fresh for
this acceptance: `cmake --build build --parallel 4` exited `0`, and
`ctest --test-dir build --output-on-failure` exited `0` with `332/332`.
Raw output and status are retained in the ignored Task 93 log directory.

## Fresh compiler, dependency, and optional-build evidence

Every command below was run with `2>&1 | tee` and its raw output/status remains
under `.superpowers/sdd/2026-08-12-fincore-cpp-phase-21-bootstrap-confidence-interval/task-93-logs/`.

| Gate | Fresh evidence |
| --- | --- |
| AppleClang headless | `/tmp/fincore-phase21-task93-apple.BX0gIK`: AppleClang `17.0.0.17000603` Debug configure, build, and full CTest each exited `0`; CTest passed `332/332`. |
| Homebrew LLVM 22 ASan+UBSan headless | `/tmp/fincore-phase21-task93-llvm-clean.6lxdpL`: Homebrew clang `22.1.8` Debug configure/build exited `0`; `ctest -N` first recorded exactly `332` registered tests, then `ctest --parallel 4` passed `332/332`. The final sanitizer scan has its expected zero-match `rg` exit `1` for `AddressSanitizer`, `UndefinedBehaviorSanitizer`, and `runtime error`. Its one warning is the third-party GoogleTest `gtest-printers.h:524:35` `-Wcharacter-conversion` warning, not a sanitizer diagnostic. |
| ImPlot/demo compile-link only | `/tmp/fincore-phase21-task93-implot.oprtt6`: AppleClang Debug configure/build exited `0`; the build log links `fincore_implot_demo`, and a `test -f`/`test -x` check passed for that build-root executable without launching it. Full CTest passed `333/333`, including `ImPlotAdapterCompile`. No GUI process was started. |
| Invalid ImGui expected failure | `/tmp/fincore-phase21-task93-invalid-imgui-final.ulx1zM`: the configure exited `1` as expected. Its whitespace-tolerant multiline scanner `rg -UP 'FINCORE_IMGUI_SOURCE_DIR(?s:.*?)all required\\s+files'` exited `0` against CMake's physically wrapped required-files diagnostic. |

The clean Apple fresh cache records
`FETCHCONTENT_SOURCE_DIR_GOOGLETEST:PATH=` and
`FETCHCONTENT_FULLY_DISCONNECTED:BOOL=OFF`.  Its generated
`_deps/googletest-src` has remote `https://github.com/google/googletest.git`
at `b514bdc898e2951020cbdca1304b75f5950d1f59`, matching the project's pinned
`v1.15.2` declaration.  This is pinned GoogleTest FetchContent provenance, not
a claim that dependencies are all local or that no fetch occurred.

The earlier isolated LLVM directory
`/tmp/fincore-phase21-task93-llvm.QD6ahj` recorded `552` tests without a prior
registration audit.  It is retained as non-acceptance diagnostic evidence and
is deliberately excluded from every acceptance claim; the later clean,
registration-audited directory above is the LLVM acceptance proof.

## Scope and exclusions

Task 93 changes only `README.md` and this acceptance record.  It does not
change `simulation.hpp`, the Task 92 leaf/header/source/tests, CMake,
DataFrame behavior, any simulation parameter/GBM/correlation/stress behavior,
or ImPlot sources.

This acceptance excludes bootstrap summaries; Python method variants and
callable statistics; full Python RNG and nonfinite-percentile parity; Monte
Carlo; antithetic/LHS; SimResult; providers; reports; packaging/release work;
and GUI runtime behavior.

## Retained evidence

The ignored report
`.superpowers/sdd/2026-08-12-fincore-cpp-phase-21-bootstrap-confidence-interval/task-93-report.md`
indexes the raw `tee` logs, exact commands/statuses, dependency provenance,
diagnostic scanner, review verdict, and scope checks.

## Non-self-referential task traceability

The accepted implementation commit is Task 92,
`f15dbb95eaf1ebb4c0d2f4d4a17958c85c84789d` (`feat: add bootstrap confidence
interval`).  Its exact scope is the five files listed in **Public boundary and
accepted implementation**: the bootstrap public leaf, its implementation,
test registration, and the two confidence-interval consumers.  Its diff does
not contain `include/fincore/simulation.hpp`: the facade export is automatic
because that umbrella already includes the bootstrap leaf.  The leaf-only and
umbrella-only consumers therefore prove the existing facade path without a
redundant facade edit.

The initial acceptance commit is Task 93,
`49c19adaf1b765a8a3db09f562786a82d4f8ff91` (`docs: accept phase 21 bootstrap
confidence interval`).  It is the direct child of Task 92.  Its exact scope is
only `README.md` and this acceptance document; it records acceptance evidence
and public-facing wording, with no production, test, CMake, or facade change.

Task 94 is documentation-only and changes only this acceptance document to
make the preceding implementation and acceptance chain explicit.  It does not
amend either prior commit, does not add a `simulation.hpp` diff, and does not
make a self-referential claim about its own commit identity.
