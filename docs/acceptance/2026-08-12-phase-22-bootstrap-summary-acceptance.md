# Phase 22 bootstrap-summary acceptance

## Accepted implementation chain and automatic facade

The Task 96 native leaf implementation is
c3917f9096a74829f4e2f94aca8a9180c54a28e0 (feat: add bootstrap summary). Its
exact five-path scope is:

- include/fincore/simulation/bootstrap.hpp
- src/simulation/bootstrap.cpp
- tests/CMakeLists.txt
- tests/unit/bootstrap_summary_test.cpp
- tests/unit/simulation_bootstrap_summary_umbrella_test.cpp

Its direct child, 5297942038083355896f82901984f26cdbe69418 (test: distinguish
bootstrap summary ratios), is the independent-review test-only correction. Its
exact one-path scope is tests/unit/bootstrap_summary_test.cpp; it changes no
production, facade, CMake, or dependency code.

The existing simulation facade already includes the bootstrap leaf:
<fincore/simulation.hpp> includes <fincore/simulation/bootstrap.hpp>. Task 96
therefore needs no facade diff. Before declarations existed, registered
leaf-only and facade-only public consumers each failed only because the summary
types/function were absent; CTest was deliberately not run after that RED. The
GREEN consumers compile and run the same declarations through both headers.

## Public contract and native semantics

~~~cpp
struct bootstrap_summary_statistic {
    double value;
    double standard_error;
    bootstrap_confidence_interval_result confidence_interval;
};

struct bootstrap_summary_result {
    bootstrap_summary_statistic mean;
    bootstrap_summary_statistic sample_standard_deviation;
    bootstrap_summary_statistic sharpe_ratio;
    bootstrap_summary_statistic sortino_ratio;
};

bootstrap_summary_result bootstrap_summary(
    const dataframe& returns,
    std::size_t resample_count = 10000U,
    double alpha = 0.05,
    std::optional<std::uint64_t> seed = std::nullopt,
    std::string_view column = returns_column);
~~~

The value-owning result validates finite strict alpha in (0, 1), then
resample_count >= 2, before input/delegate work. It owns a nonempty NUL-free
selected name before the DataFrame C-string boundary, requires a
genuine-double length-matched selected column, drops only NaNs, rejects +inf
and -inf, and requires at least one retained source value. Every observed
value uses that same retained source sequence.

Its fixed source order is mean, sample_standard_deviation, sharpe_ratio, then
sortino_ratio. Each entry calls public bootstrap_distribution exactly once,
preserving the DataFrame, count, seed, and selected column and varying only the
statistic enum. It does not duplicate sampling, random-engine, selector,
DataFrame-materialization, or named-statistic logic.

Each entry copies its generated distribution, requires at least two finite
statistics, computes standard_error as sample standard deviation (ddof=1), and
sorts that same distribution for lower/upper linear-percentile endpoints at
alpha / 2 and 1 - alpha / 2. For ordered values and percentile p, the position
is p * (n - 1), with adjacent-order-statistic interpolation. The private
finite distribution/linear-percentile helper is shared with the existing Phase
21 confidence-interval implementation. The summary does not call public
bootstrap_confidence_interval, so it does not resample. Every
observed/generated statistic, standard error, and endpoint must be finite;
otherwise it throws fincore::input_error and returns no partial four-field
result.

A native fixed seed is repeatable for the supported C++ implementation. This
does not claim that its MT19937-64/rejection-sampling stream equals NumPy's.
Tests also prove result and selector/source lifetime independence after source
or name mutation and source destruction.

## Source oracle and deliberate safety differences

The semantic source oracle is
/Users/yunjinqi/Documents/new_projects/fincore/fincore/simulation/bootstrap.py.
It was freshly rerun from that source checkout with the mandated Anaconda
command /Users/yunjinqi/opt/anaconda3/bin/conda run -n base python; raw
stdout/stderr is retained in the ignored Task 97 report/log directory.

For [0.01, -0.01, 0.02, 0.0], five resamples, alpha=0.2, seed 42, the source
returned:

| Statistic | value | se | [ci_lower, ci_upper] |
| --- | ---: | ---: | --- |
| mean | 0.005 | 0.0027386127875258306 | [0.004999999999999999, 0.01] |
| std | 0.012909944487358056 | 0.0020561550117877245 | [0.009517781639083364, 0.012909944487358056] |
| sharpe | 6.148170459575759 | 6.065816032488248 | [6.148170459575758, 17.16442409108116] |
| sortino | 15.874507866387544 | nan | [15.87450786638754, nan] |

The same run gives these actual boundary observations:

- With a raw NaN source, Python's four value fields are nan even though its
  bootstrap distribution drops NaNs.
- It accepts alpha=0 and alpha=1; alpha=nan and alpha=inf raise ValueError:
  Percentiles must be in the range [0, 100].
- Count zero reaches IndexError: index -1 is out of bounds for axis 0 with
  size 0. Count one returns a dictionary whose standard errors are nan.
- Constant positive and singleton samples produce infinity/nan statistic
  components, not an all-finite summary.
- The run emitted five NumPy RuntimeWarnings: two invalid-subtract warnings,
  a degrees-of-freedom warning, an invalid scalar-divide warning, and an
  invalid-divide warning. The raw log retains their exact paths/messages.

The native boundary intentionally differs: it uses retained values for
observed fields too, rejects alpha endpoints/nonfinite alpha and zero/one
counts, and rejects degenerate or nonfinite observed/generated/output
statistics rather than returning a Python dictionary containing nan or
infinity. These differences, and the absence of a NumPy stream-equality
claim, are tested rather than hidden. The source is a semantic reference, not
proof of nonconstant random-stream equality.

## Task 96 RED/GREEN and current configured evidence

The RED leaf consumer includes only <fincore/simulation/bootstrap.hpp>; the
RED facade consumer includes only <fincore/simulation.hpp>. Before the
declaration both compilation units failed only on absent public summary names.
Independent review corrected the mixed-sign Sortino test oracle in its one-path
test-only child. The final Task 96 scoped GREEN passed 81/81 and full CTest
passed 342/342.

For this acceptance, the current configured build was freshly checked:

~~~sh
cmake --build build --parallel 4
ctest --test-dir build -R \
  'BootstrapSummary|SimulationBootstrapSummaryUmbrella|BootstrapConfidenceInterval|BootstrapDistribution' \
  --output-on-failure
ctest --test-dir build --output-on-failure
~~~

All exited 0: the focused selection passed 26/26 and the full configured suite
passed 342/342.

## Fresh compiler, dependency, and optional-build gates

Raw tee logs, exact commands, temporary roots, and exit-status files are
retained in
.superpowers/sdd/2026-08-12-fincore-cpp-phase-22-bootstrap-summary/task-97-report.md
and its adjacent task-97-logs directory.

| Gate | Fresh evidence |
| --- | --- |
| AppleClang headless | Unique /tmp/fincore-phase22-task97-apple.quTy0p/build, AppleClang 17.0.0.17000603, Debug Ninja, project C++23 (-std=c++2b in AppleClang), testing on/ImPlot off: configure, build, and full CTest each exited 0; CTest passed 342/342. A raw warning scan returned no matches, with expected rg exit 1. |
| Homebrew LLVM 22 ASan+UBSan | Separate /tmp/fincore-phase22-task97-llvm.IUqTPH/build, Homebrew clang 22.1.8, Debug Ninja with FINCORE_ENABLE_ASAN=ON, FINCORE_ENABLE_UBSAN=ON, C++23, testing on/ImPlot off: configure/build exited 0; ctest -N first registered exactly 342 tests; full CTest passed 342/342. The raw scan for AddressSanitizer, UndefinedBehaviorSanitizer, and runtime error had zero matches, hence expected rg exit 1. The one build warning is third-party GoogleTest gtest-printers.h:524:35 -Wcharacter-conversion, not a sanitizer finding. |
| ImPlot/demo compile-link only | Unique /tmp/fincore-phase22-task97-implot.NUdNrL/build, AppleClang Debug C++23 with testing, ImPlot, and demo on, using the real local ImGui/ImPlot source directories: configure/build and full CTest exited 0; CTest passed 343/343 including ImPlotAdapterCompile. The actual build-root fincore_implot_demo passed both test -f and test -x. It was not launched and no GUI process was started. Exactly three expected macOS OpenGL deprecation warnings were recorded: glViewport, glClearColor, and glClear. |
| Invalid ImGui expected failure | Unique /tmp/fincore-phase22-task97-invalid-imgui.FMaBvm/build, ImPlot on/testing off with an explicitly nonexistent FINCORE_IMGUI_SOURCE_DIR: configure exited 1. The required whitespace-tolerant multiline PCRE rg -n -U -P 'FINCORE_IMGUI_SOURCE_DIR(?s:.*?)all required\s+files' exited 0 against CMake's wrapped diagnostic. A retained initial line-oriented scan exited 1 because the message is physically wrapped; CMake was not changed. |

All fresh positive caches state:

~~~text
FETCHCONTENT_SOURCE_DIR_GOOGLETEST:PATH=
FETCHCONTENT_FULLY_DISCONNECTED:BOOL=OFF
FETCHCONTENT_UPDATES_DISCONNECTED:BOOL=OFF
~~~

Each generated _deps/googletest-src has remote
https://github.com/google/googletest.git at
b514bdc898e2951020cbdca1304b75f5950d1f59; the project declares FetchContent
tag v1.15.2. This is actual FetchContent provenance, not a claim of local/no-
fetch GoogleTest.

## Scope and caveats

This accepts the bounded native bootstrap-summary leaf only. It does not accept
a release, full Python parity, NumPy RNG parity, Python dictionary/map output,
callable statistics, endpoint alpha support, nonfinite summary results, Monte
Carlo, antithetic or Latin-hypercube sampling, providers, reports, packaging,
or GUI runtime. The ImPlot gate is compile/link-only; no GUI was launched.
