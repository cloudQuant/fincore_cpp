# Examples and demos

Two kinds of runnable material live in the repository: small programs that show
the API in use, and benchmark harnesses that measure it. Everything here builds
from the same checkout; none of it downloads data.

## Render a factor report

`examples/render_factor_report.cpp` builds a synthetic factor panel, analyses it
and writes three artefacts: an English HTML report, a Chinese HTML report and an
XLSX workbook. It shows the shortest path from prepared data to a rendered
document.

```bash
cmake --build build --parallel --target fincore_report_render
./build/fincore_report_render report.html
# writes report.html, report.zh.html and report.xlsx
```

Pass `--echarts` to render the charts with the embedded ECharts build instead of
inline SVG:

```bash
./build/fincore_report_render --echarts report.html
```

The report is deterministic: rendering the same document twice produces identical
bytes, which is what makes the files safe to diff and hash.

## Render one document through every backend

`examples/render_iteration8_artifacts.cpp` takes a directory and writes the same
document as HTML (inline SVG and ECharts), XLSX, Plotly JSON, Bokeh JSON and PDF,
then prints the document's semantic digest.

```bash
cmake --build build --parallel --target fincore_i8_artifacts
./build/fincore_i8_artifacts ./out
```

The digest depends only on the document, not on the backend, so the six files can
be compared against a recorded digest even though their formats differ.

## An interactive ImPlot dashboard

`examples/implot_dashboard.cpp` draws the tear charts, frontier, heatmap and
event panels in an ImGui/ImPlot window. It is built only when the GUI options are
on and the GUI dependencies are present:

```bash
cmake -S . -B build-gui -DFINCORE_BUILD_IMPLOT=ON -DFINCORE_BUILD_IMPLOT_DEMO=ON
cmake --build build-gui --parallel --target fincore_implot_demo
```

| Variable | Effect |
|---|---|
| `FINCORE_IMPLOT_VIEW` | `general`, `factor`, `groups`, `events` or `specialized` |
| `FINCORE_IMPLOT_CAPTURE` | Renders three frames headlessly and writes a PPM image |

An unrecognized view name is rejected with exit code 2 rather than silently
falling back to the general view.

## Benchmarks

| Target | What it does |
|---|---|
| `fincore_factor_benchmark` | Compute-once factor pipeline cases, one JSON record per case |
| `fincore_alpha003_research` | The Alpha003 A-share research workload used in the performance comparisons |

```bash
cmake --build build --parallel --target fincore_factor_benchmark fincore_alpha003_research
python benchmarks/alpha003_a_stocks/run_benchmark.py --help
```

The Python driver runs the paired reference/candidate campaign, compares the
research tables by digest, and writes one HTML comparison report. Read
[performance](performance.md) for what the resulting ratio does and does not
mean, and keep generated results out of git.

## Where the code lives

```
examples/        report rendering and dashboard demos
benchmarks/      factor and alpha003 workloads, plus the Python drivers
tests/           unit, consumer and oracle tests
```

Every example is a normal consumer of the installed library: it includes
`<fincore/...>` headers and links `fincore::fincore`, so it also serves as a
compile-time check that the public surface is usable from outside the build tree.
