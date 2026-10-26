# Phase 23 Latin-hypercube sampling acceptance

## Accepted implementation and automatic facade

The Task 100 leaf implementation is
`f013aa9659099f6b0e60f9edb1c8052b2ba72024` (`feat: add latin hypercube
sampling`), a direct child of the Phase 23 design/plan commit
`acea9f973ce9915d4d8e43f147686904962d152e`. Its exact five-path scope is:

- `include/fincore/simulation/paths.hpp`
- `src/simulation/paths.cpp`
- `tests/CMakeLists.txt`
- `tests/unit/latin_hypercube_sampling_test.cpp`
- `tests/unit/simulation_latin_hypercube_umbrella_test.cpp`

Independent review found a real IEEE-754 boundary defect and two coverage
gaps. The direct child repair
`afb162dd492433fe9ebbece8149ab2ec0018337d` (`fix: preserve latin hypercube
strata`) changes only `src/simulation/paths.cpp` and
`tests/unit/latin_hypercube_sampling_test.cpp`. Its fresh re-review is **Spec
PASS / Task-Quality PASS** with no Critical, Important, or Minor findings.

`<fincore/simulation.hpp>` already includes
`<fincore/simulation/paths.hpp>`, so the existing façade exports the leaf
without a redundant public-header edit. Before the declaration existed,
registered leaf-only and façade-only consumers each failed only because
`latin_hypercube_sampling` and
`latin_hypercube_dimension_column_prefix` were absent. No stale CTest command
was run after that expected RED. The GREEN façade consumer includes only
`<fincore/simulation.hpp>`.

## Public contract and native behavior

```cpp
inline constexpr std::string_view latin_hypercube_dimension_column_prefix{
    "lhs_dimension_"};

dataframe latin_hypercube_sampling(
    std::size_t sample_count,
    std::size_t dimension_count,
    std::optional<std::uint64_t> seed = std::nullopt);
```

The result owns ordinal sample index `0..sample_count-1` and exactly the
genuine-`double` columns `lhs_dimension_0..N-1` in numeric order. It preserves
the source zero shapes `(0,d)`, `(n,0)`, and `(0,0)`. Each nonempty column
uses a local Fisher-Yates permutation of the source strata and has exactly one
finite value in every half-open interval `[k/n, (k+1)/n)`; all values are in
`[0,1)`.

The native implementation reuses the paths leaf's local `std::mt19937_64`
engine and open-unit uniform primitive. An explicit seed repeats within the
supported C++ build; a missing seed uses a newly local engine from
`std::random_device`; no global engine, external RNG parameter, NumPy stream
parity, sort, clipping, or post-generation normalization is introduced.

The repair addresses floating-point boundary rounding without changing the
surface. Positive two-dimensional shapes with more than
`2^numeric_limits<double>::digits` strata are rejected before allocation or
random work, because a double cannot provide a distinct representable value
for every stratum near one. A generated candidate is accepted only when its
observable classification `floor(value * sample_count)` remains the selected
permutation stratum. Candidates are redrawn rather than clipped or moved.

Before allocation/random work, native validates ordinal index, name,
vector-capacity, and output-cell representability; allocation/length errors
are translated to `fincore::input_error`. The tests use a legal public count
pair to reach the cell-product guard, without an invalid span or fake backing
buffer. Python negative sizes are outside the stronger `std::size_t` API
boundary.

## Source oracle and Task 100 evidence

The semantic source is
`/Users/yunjinqi/Documents/new_projects/fincore/fincore/simulation/paths.py`.
The mandated Anaconda command was rerun in that source checkout and is
retained verbatim in the ignored Task 100 report. With seed 42 it records:

| Input | Source shape / structural result |
| --- | --- |
| `(4,3)` | shape `(4,3)`; every dimension has strata `[0,1,2,3]` |
| `(1,3)` | shape `(1,3)`; every dimension has stratum `[0]` |
| `(4,0)`, `(0,3)`, `(0,0)` | the corresponding three zero shapes |
| `(-1,3)`, `(3,-1)` | Python `ValueError: negative dimensions are not allowed` |

Native tests use the oracle for shape and strata semantics, not nonconstant
NumPy random-stream equality. The initial Task 100 GREEN had LHS/façade
`8/8`, required focused `68/68`, and full `350/350` tests. The independent
review repair first reproduced the old numerical expression with the largest
open-unit midpoint: for `n=2`, selected stratum `1`, it yielded
`value=1, observed_stratum=2` and exited nonzero. The repaired result passes
LHS/façade `10/10`, required focused `70/70`, and full `352/352` tests.
It additionally covers source `(1,3)`, precision-limit rejection, and an
actual cell-count overflow guard reached before allocation.

The leaf and façade tests cover API/defaults, exact schema/index/type/order,
all zero shapes, range and strata coverage, same-seed repetition,
different-seed divergence, finite unseeded output, name/index/vector/cell
errors, ownership, and automatic façade consumption. They do not reimplement
the sampler or assert a NumPy stream.

## Current and fresh build gates

Raw `tee` logs, commands, status files, source-oracle output, and review notes
are retained under
`.superpowers/sdd/2026-08-12-fincore-cpp-phase-23-latin-hypercube-sampling/`.

| Gate | Evidence |
| --- | --- |
| Current configured build | `cmake --build build --parallel 4` exited 0; focused LHS/paths/simulation regression CTest passed `70/70`; full CTest passed `352/352`. |
| Fresh AppleClang headless | `/tmp/fincore-phase23-task101-apple.cXVHyW/build`, AppleClang `17.0.0.17000603`, Debug Ninja, C++23, testing on and ImPlot off: configure/build/full CTest each exited 0; full CTest passed `352/352`. The raw warning scan had no matches (expected `rg` exit 1). |
| Fresh LLVM ASan+UBSan | `/tmp/fincore-phase23-task101-llvm.c0dsDM/build`, Homebrew Clang `22.1.8`, Debug Ninja, C++23, testing on, ImPlot off, ASan and UBSan on: configure/build exited 0; `ctest -N` registered exactly `352` tests before execution; full CTest passed `352/352`. The sanitizer scan found no AddressSanitizer, UndefinedBehaviorSanitizer, or runtime-error diagnostic (expected `rg` exit 1). The only build warning is third-party GoogleTest `gtest-printers.h:524` `-Wcharacter-conversion`. |
| ImPlot/demo compile-link only | `/tmp/fincore-phase23-task101-implot.4VNScV/build`, AppleClang Debug C++23, testing/ImPlot/demo on, and the real local ImGui/ImPlot directories: configure/build/full CTest each exited 0; CTest passed `353/353`, including `ImPlotAdapterCompile`. The build-root `fincore_implot_demo` passed `test -f` and `test -x`; it was not launched and no GUI process was started. Exactly three expected macOS OpenGL deprecation warnings were recorded: `glViewport`, `glClearColor`, and `glClear`. |
| Invalid ImGui expected failure | `/tmp/fincore-phase23-task101-invalid-imgui.k3luCI/build`, ImPlot on/testing off with an explicitly nonexistent `FINCORE_IMGUI_SOURCE_DIR`: configure exited `1` at `cmake/fincore_implot.cmake:47`. The whitespace-tolerant multiline PCRE `FINCORE_IMGUI_SOURCE_DIR(?s:.*?)all required\s+files` matched the wrapped diagnostic with exit `0`. |

The three fresh positive caches each record
`FETCHCONTENT_SOURCE_DIR_GOOGLETEST:PATH=` and
`FETCHCONTENT_FULLY_DISCONNECTED:BOOL=OFF`. Their generated
`_deps/googletest-src` checkout has remote
`https://github.com/google/googletest.git` at
`b514bdc898e2951020cbdca1304b75f5950d1f59`; `tests/CMakeLists.txt` declares
the pinned FetchContent tag `v1.15.2`. This is actual FetchContent provenance,
not a claim that GoogleTest was locally supplied or unfetched.

## Traceability

The Task 100 implementation chain is
`f013aa9659099f6b0e60f9edb1c8052b2ba72024` followed directly by
`afb162dd492433fe9ebbece8149ab2ec0018337d`. The first commit changes exactly
`include/fincore/simulation/paths.hpp`, `src/simulation/paths.cpp`,
`tests/CMakeLists.txt`, `tests/unit/latin_hypercube_sampling_test.cpp`, and
`tests/unit/simulation_latin_hypercube_umbrella_test.cpp`; the repair changes
only `src/simulation/paths.cpp` and
`tests/unit/latin_hypercube_sampling_test.cpp`.

The direct Task 101 acceptance commit is
`525fbe4d687dd3b092a8839d6434585d0e2ed6df`, whose parent is the repair. Its
exact scope is `README.md` and this acceptance document. The subsequent trace
is documentation-only, does not amend any prior commit, and does not state or
anticipate its own commit identifier.

## Scope and exclusions

This accepts only the bounded, DataFrame-native Latin-hypercube sampling leaf
and its existing simulation façade exposure. It does not accept antithetic
transforms, `MonteCarlo`/`SimResult`, full NumPy RNG parity, a caller-provided
RNG, normal/correlated transformations, GBM behavior changes, providers,
reports, packaging/release work, or GUI runtime behavior. The ImPlot evidence
is compile/link-only; no GUI was launched.
