# Architecture and design choices

This page explains how the library is put together and why the public surface
looks the way it does. It is the "why" companion to the reference, which is the
"what".

## Layers

```
data            dataframe, calendar, UTC timestamps, frequency helpers
metrics         returns, ratios, drawdowns, rolling windows, statistics
domain          risk, portfolio, factor, attribution, optimization
presentation    report documents, HTML/XLSX/PDF/Plotly/Bokeh/axes renderers
runtime         operation catalog, sessions, serialization, extensions
```

Each layer may use the ones above it and never below it: a metric does not know
about report documents, and a renderer never recomputes a statistic. That is why
the report model carries values rather than frames — rendering cannot change a
number, only present it.

## Ownership

One rule explains most of the API shape: **inputs are copied at the boundary, and
results own their data**.

- `portfolio_inputs`, `analysis_context` and `rolling_engine` take their frames
  by value and store their own copy, so a caller can mutate a source frame
  without changing a result already produced.
- Functions that return a `dataframe` return a new frame. Accessors that return a
  span or `std::string_view` are views and are valid only while their owner is
  alive.
- `materialize()` returns fresh frames on every call, which is why mutating its
  result cannot corrupt the next call.

The trade-off is allocation against predictability. For the daily and monthly
panel sizes this library targets, predictable ownership was judged worth the copy;
the benchmark harness exists to keep that judgment honest at larger scales.

## Determinism

Anything that could be compared, stored or replayed is reduced to a digest:

- Runtime serialization length-prefixes each field and tags its type, so two
  different payloads cannot collide through separators or a `bool`/`1`
  ambiguity.
- The factor fingerprint hashes **inputs**; report digests cover the **document**,
  so two backends rendering one document agree by construction.
- Floating-point values are serialized as IEEE-754 bit patterns where the bytes
  matter, which preserves `NaN` payloads and signed zeros.

Randomised methods take an explicit seed. A seed reproduces a stream inside this
library; only functions that document a reference mapping promise NumPy-identical
numbers.

## Errors and degenerate results

The library distinguishes three outcomes, and the distinction is part of the
contract rather than an implementation detail:

| Outcome | Meaning | Example |
|---|---|---|
| Exception (`input_error`, `factor_data_error`) | The request is structurally wrong | Missing column, duplicated index, embedded NUL |
| `NaN` | The request is well formed but the statistic is undefined | Too few observations, zero deviation, no drawdown |
| Status field | A fit or validation ran and reports its own verdict | `risk_estimate_status`, `walk_forward_status`, `risk_backtest_status` |

A status field exists precisely so callers do not have to infer failure from a
plausible-looking number.

## Concurrency

Free functions are stateless: the same arguments produce the same result, and two
threads may call them concurrently on separate frames. Stateful objects are
explicit:

- `analysis_context` caches results, so one instance is single-threaded by
  construction.
- `analysis_session` owns its inputs and outputs and refuses calls after
  `close()`.
- `rolling_engine` owns copies and is safe to use from one thread at a time.

No hidden thread pools, no global caches, and no background work: a call does what
it says before it returns.

## Extension points

| Point | Contract |
|---|---|
| Data providers | A callable that returns a frame; validation and digesting stay in the library |
| Report backends | A renderer consumes `report_document` and returns an owning artifact bundle |
| Extensions | Native snapshot/hook ABI: hooks carry an event and a priority, ties break by fingerprint |
| Operations | Descriptors list inputs and outputs; execution returns a typed `runtime_value` |

Optional surfaces stay optional: the ImPlot adapter and its GUI dependencies are
never on the core `fincore::fincore` path.

## What is deliberately not here

- **No Python object compatibility.** `fincore::dataframe` is its own type; the
  parity work is about numbers, not object identity.
- **No implicit data access.** Nothing downloads a price series or reads a file
  behind your back; I/O is the caller's decision.
- **No unit conversion.** Returns are decimal fractions, annualization is an
  argument, and weights carry their call-site meaning.
- **No silent repair.** A duplicated index is rejected, not deduplicated; a
  missing observation is `NaN`, not zero.

## How the documentation mirrors this

The site follows the same split: the tutorial and cookbook show usage, the guides
explain behaviour, the reference is generated from the headers, and this page
explains the structure. Publication state lives in the repository and is never
asserted on the home page.
