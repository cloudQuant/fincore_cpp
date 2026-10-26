# Reports, runtime and extensions

Two connected layers live here: a typed report model that every backend renders,
and a runtime layer that turns operations into catalog entries and execution
results. Both favour explicit contracts over convenience.

## One document, many backends

`report_document` is the single value a report is built from:

```cpp
fincore::report_document document;
document.domain = "portfolio";
document.title = "Monthly review";
document.metadata = {{"source", "fixture"}};
```

A document holds sections; a section holds `metrics`, `tables`, `series`,
`notes`, `units` and `legends`. Values are typed cells — text, number, boolean or
missing — so a backend never has to guess whether a column is numeric.

| Backend | Function | Notes |
|---|---|---|
| HTML | `render_html(document, options)` | `language` selects `en`/`zh`; `chart_backend` selects inline SVG or ECharts |
| XLSX | `render_xlsx(document, options)` | One sheet per section; cells use spreadsheet names and inline strings |
| PDF | `render_pdf_bytes(document)`, `write_pdf(document, path)` | `write_pdf` also creates parent directories |
| Plotly | `render_plotly(document)` | Returns a bundle with a `figure` artifact |
| Bokeh | `render_bokeh(document)` | Same bundle shape as Plotly |
| Static axes | `render_matplotlib(document, caller_axes)` | Caller-supplied axes are marked not owned |

Rendering the same document twice produces byte-identical output for the
deterministic backends. That is what makes reports safe to hash and diff, and it
is why the workbook writer fixes the ZIP timestamps instead of stamping the clock.

## Artifact bundles

The plotting and PDF renderers return a `report_artifact_bundle`:

- `metadata` always carries `backend` and `report_digest`; `report_digest()` is
  the document's semantic digest, so two backends of one document agree.
- `artifacts` carries named outputs (`figure`, `file`, `axis:<section>.<series>`)
  with `name`, `path`, `owned` and `content`.
- `find(name)` returns `nullptr` when the name is absent, so an unchecked
  dereference is a programming error rather than a silent default.
- A caller-supplied axis, and the file produced by `write_pdf`, are **not owned**:
  the caller keeps the lifetime.

## Escaping and non-finite values

Two escaping schemes coexist because two output channels do. Markup text goes
through HTML entities; the series payload embedded in a script element is
JSON-escaped, so `<`, `>` and `&` become `\u003c`, `\u003e` and `\u0026` and no
non-ASCII code point reaches the script source unescaped.

Non-finite numbers are not printed as `NaN` text. Where a channel can carry a gap
the value becomes a null; where the geometry cannot, the point is dropped or
collapsed to zero. The renderer documents which one it does, because a chart that
silently plots zero for infinity is worse than one that leaves a gap.

## Runtime catalog

Operations are described rather than discovered by name at run time. Each module
exposes its own descriptor list — `metrics_operations()`, `risk_operations()`,
`portfolio_operations()`, and so on — and `all_operations()` concatenates them in
a fixed order. The order is part of the digest, so it is not alphabetical and not
incidental.

`operation_catalog` registers specs and resolves requests; an unknown or ambiguous
name raises `operation_resolution_error`. A request produces an
`execution_plan`, and executing it yields a `runtime_result` whose payload is a
typed `runtime_value` rather than a stringly-typed map.

## Sessions and caching

`analysis_context` caches deterministic computations. `summary()` returns the same
map on a second call and `computation_count(key)` stays at one — the count, not
the equality, is what proves the cache was used. Because the cache is mutable,
one context is single-threaded by construction; frames can still be shared.

`analysis_session` owns its inputs and outputs and rejects calls after `close()`.
Closing twice is safe. An extension merge happens once even when every callable
is complete, so a session never half-applies a registration set.

## Extensions

Extension discovery uses the native snapshot and hook ABI documented in the
extension headers. Two properties matter when reading a snapshot:

- Hooks carry an event name and a priority: lower priorities run first, and hooks
  that share both an event and a priority are ordered by fingerprint. Sorting the
  whole registry by priority alone would therefore reorder hooks within an event.
- The snapshot digest covers the ASCII-escaped payload, not host-encoded text, so
  a non-ASCII identifier has one canonical byte form.

Plotting adapters (ImPlot, GLFW-backed demos) are not part of the core CMake
consumer contract; they are built only when their options are enabled.

## A minimal report

```cpp
#include <fincore/report/html.hpp>
#include <fincore/report/models.hpp>
#include <fincore/report/xlsx.hpp>

fincore::report_document document;
document.domain = "demo";
document.title = "Demo";
fincore::report_section section;
section.key = "summary";
fincore::report_metric metric;
metric.key = "sharpe";
metric.label = "Sharpe";
metric.number = 1.25;
section.metrics.push_back(metric);
document.sections.push_back(section);

const std::string html = fincore::render_html(document, {.language = "en"});
const std::string workbook = fincore::render_xlsx(document);
```

Both strings are complete files; nothing else needs to be generated or templated.
