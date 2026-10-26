# fincore_cpp

**English** | [简体中文](README.zh-CN.md)

`fincore_cpp` is a native C++23 library for quantitative portfolio, factor,
performance and risk analytics. It ships its own typed API over a local
`DataFrame` type and does not depend on Python, pandas or NumPy at runtime — nor
does it promise pandas object compatibility.

```cpp
#include <fincore/dataframe.hpp>
#include <fincore/metrics/drawdown.hpp>
#include <fincore/metrics/ratios.hpp>

const auto returns = fincore::make_series(
    {fincore::utc_timestamp(2025, 1, 2), fincore::utc_timestamp(2025, 1, 3)},
    "returns", {0.01, -0.02});

const double sharpe = fincore::sharpe_ratio(returns, "returns");
const double drawdown = fincore::max_drawdown(returns, "returns");
```

## What it covers

| Area | What you get |
|---|---|
| Core data | `fincore::dataframe` (index + typed columns), UTC nanosecond timestamps, calendar and frequency helpers |
| Metrics | Returns, ratios, drawdowns, rolling statistics, tables, stability and consecutive-run statistics |
| Risk | Historical and parametric VaR/ES, GARCH/EGARCH, EVT (GPD/GEV), Bayesian cone, backtests and walk-forward validation |
| Portfolio | Positions, transactions, round trips, turnover, capacity, cash-flow-aware performance |
| Factor research | Analysis pipeline, quantile/IC studies, tears, artifacts, point-in-time joins, export contracts |
| Attribution | Brinson (BHB/Fachler), factor and style regression, exposures, prediction, performance attribution |
| Optimization | Mean-variance and risk-parity style solvers, projected gradient, box/sector constraints |
| Reporting | One report model rendered to HTML, XLSX, PDF, Plotly, Bokeh, Matplotlib-style axes and ImPlot |
| Runtime | Operation catalog and dispatch, analysis context caching, extension snapshots, serialization |
| Simulation | GBM, bootstrap, Latin hypercube, antithetic variates, correlation breakdowns |

## Requirements

- CMake 3.25+ and a C++23 compiler (MSVC 19.4+, Clang 17+, GCC 13+).
- Pinned git submodules (`third_party/DataFrame`, `third_party/implot`).

Python is used only for tooling, benchmarks and documentation, never by the
library.

## Build and test

```bash
git clone https://github.com/cloudQuant/fincore_cpp.git
cd fincore_cpp
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Install and consume from CMake:

```bash
cmake --install build --prefix /path/to/prefix
```

```cmake
find_package(fincore CONFIG REQUIRED COMPONENTS fincore)
target_link_libraries(my_target PRIVATE fincore::fincore)
```

Build options: `FINCORE_BUILD_TESTING` (default ON), `FINCORE_BUILD_IMPLOT`
(default OFF, GUI adapter), `FINCORE_BUILD_IMPLOT_DEMO` (default OFF),
`FINCORE_ENABLE_ASAN` and `FINCORE_ENABLE_UBSAN` (default OFF).

## Documentation

One bilingual documentation site, with a language switch in the top-right corner
of every page and your choice remembered between visits.

- Online: <https://cloudquant.github.io/fincore_cpp/en/latest/> ·
  <https://cloudquant.github.io/fincore_cpp/zh-cn/latest/>
- Start here: installation, quickstart, conventions
- Guides: metrics, portfolio and risk, factor research, reports and runtime
- Reference: one page per public module, generated from the headers
- Repository sources: [`docs/site/`](docs/site/), build steps in
  [`docs/site/README.md`](docs/site/README.md)

Public coverage is tracked by `docs/site/api-ledger.tsv`: every public symbol is
documented, and the strict Sphinx builds (`-W -n --keep-going`) run for both
languages on every change.

## Performance

The repository includes a paired benchmark harness that compares the native
implementation against the pinned Python reference on identical inputs. On the
Alpha003 A-share research workload the 500-file development batch reached
**41.20x** over the reference runner; separate 1,000/2,000-file diagnostic pairs
reached **34.46x / 35.63x**. These are measurements of that workload, not a
guarantee for every API. See the
[iteration 9 closeout](docs/iterations/iteration-9-performance-30x-100x/closeout.md)
for methodology, memory results and the evidence levels behind each number.

## Repository layout

```
include/fincore/     public headers, one directory per module
src/                 implementation
tests/               unit, consumer and oracle tests (1092 cases)
examples/            render and dashboard demos (excluded from the default build)
benchmarks/          alpha003 research benchmark and layout experiments
docs/site/           bilingual documentation sources and build tooling
docs/iterations/     iteration records and acceptance evidence
tools/               Python tooling, oracles and replay harnesses
third_party/         pinned submodules
```

## Status and support

- 1092 C++ tests, plus strict bilingual documentation builds, are part of the
  routine gate.
- Supported compilers: MSVC, Clang and GCC on their C++23 settings; the CI matrix
  builds Windows and macOS, with a known GCC 13 constexpr-limit failure tracked
  in the iteration record.
- The documentation is published to GitHub Pages; Read the Docs publication is
  configured through `.readthedocs.yaml` but not yet live — the current state is
  always recorded in [the publication page](docs/site/publication.md).

## Contributing

Issues and pull requests are welcome. Please read
[`CONTRIBUTING`](docs/site/contributing.md) for the build, test and documentation
rules, and keep changes comment-only when they claim to be. Historical planning
and acceptance records live under [`docs/iterations/`](docs/iterations/) and
[`docs/acceptance/`](docs/acceptance/).

## License

MIT — see [`LICENSE`](LICENSE).
