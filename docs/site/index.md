# fincore_cpp

Native C++23 analytics for portfolios, factors, performance and risk — one
typed API over a local `DataFrame`, with no Python dependency at run time.

[Quickstart](quickstart.md){.fc-button .fc-primary} [Installation](installation.md){.fc-button} [API reference](api/index.md){.fc-button} [GitHub](https://github.com/cloudQuant/fincore_cpp){.fc-button}

```bash
git clone https://github.com/cloudQuant/fincore_cpp.git
cd fincore_cpp && git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Find your path

| If you want to… | Start here |
|---|---|
| Get something running | [Quickstart](quickstart.md) — build a frame, compute a metric, render a report |
| Pick the right metric | [Metrics and rolling windows](guides/metrics.md) — definitions, annualization and degenerate cases |
| Produce risk numbers | [Portfolio, risk, optimization and simulation](guides/portfolio-risk.md) — estimators, backtests, statuses |
| Study a factor panel | [Factor research](guides/factor-research.md) — IC, quantiles, tears and exports |
| Look up a symbol | [API reference](api/index.md) — every public symbol, generated from the headers |
| Understand a design choice | [Data and numerical conventions](conventions.md) — timestamps, NaN, ownership, digests |

## What is inside

| Area | Highlights |
|---|---|
| Core data | `dataframe`, UTC nanosecond timestamps, calendar and frequency helpers |
| Metrics | Returns, ratios, drawdowns, rolling windows, stability statistics |
| Risk | Historical, EVT and GARCH VaR/ES, backtests, walk-forward, Bayesian cone |
| Portfolio | Positions, transactions, round trips, turnover, capacity, cash flows |
| Factor research | Analysis pipeline, IC and quantile tables, tears, point-in-time joins |
| Attribution | Brinson, factor and style regression, exposures, prediction |
| Optimization | Mean-variance, frontier, risk parity with box and sector constraints |
| Reporting | One document rendered to HTML, XLSX, PDF, Plotly, Bokeh, axes, ImPlot |
| Runtime | Operation catalog, session caching, extensions, deterministic digests |
| Simulation | GBM, bootstrap, Latin hypercube, antithetic variates |

## How this documentation is organised

- **Start here** — installation, quickstart, data and numerical conventions.
- **Guides** — task-oriented explanations of each area, with the failure rules
  that matter in practice.
- **Reference** — one page per module, generated from the public headers; the
  text you read there is the text in the source.
- **Project** — migration notes, performance method, troubleshooting, publication
  state and contribution rules.

```{toctree}
:maxdepth: 2
:caption: Start here

installation
quickstart
conventions
```

```{toctree}
:maxdepth: 2
:caption: Guides

guides/metrics
guides/portfolio-risk
guides/factor-research
guides/reports-runtime
```

```{toctree}
:maxdepth: 2
:caption: Reference

api/index
migration
performance
troubleshooting
contributing
publication
maintainer/README
```

## Scope and status

The library consumes its own `DataFrame` type and returns owning typed results.
The stable public surface is documented separately from internal `fincore::detail`
helpers, and optional visualization adapters are built only when their CMake
options and GUI dependencies are enabled.

Every public symbol is documented and tracked in the API ledger; both language
builds run strict Sphinx checks on every change, and the C++ test suite runs on
each push. Published state, including which platforms are live, is recorded in
[publication notes](publication.md) rather than asserted here.
