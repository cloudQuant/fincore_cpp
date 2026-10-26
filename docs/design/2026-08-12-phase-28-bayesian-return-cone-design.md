# Phase 28 Bayesian Posterior Return-Cone Design

## Scope decision

Python's `fincore.metrics.bayesian` mixes a deterministic posterior-return
post-processing helper with PyMC model construction, posterior sampling,
bootstrap sampling, trace objects, plotting data, and `Bayesian` class state.
Only `compute_bayes_cone` is a stable, dependency-free numerical boundary: it
compounds each predicted-return path and reports fixed cross-path percentiles at
every forecast step.

Three approaches were considered:

1. **Selected — native DataFrame cone leaf.** Add one value-returning
   `compute_bayes_cone` function operating on a DataFrame of predicted returns.
   It directly maps the deterministic source behavior into the project's
   native tabular representation.
2. **Full PyMC/`Bayesian` migration.** This would introduce Python model
   lifecycles, sampling backends, trace ownership, and external probabilistic
   dependencies. It is not a thin C++/DataFrame boundary and is out of scope.
3. **Reusing simulation paths or the ImPlot adapter.** Simulation price paths
   do not represent posterior return draws, while ImPlot is presentation-only.
   Either choice would conflate numerical construction with unrelated layers.

Phase 28 selects option 1. `compute_consistency_score` has distinct
returns-versus-prediction pairing rules and remains a separately designed
future slice.

## Public API and DataFrame mapping

Create `<fincore/metrics/bayesian.hpp>` with seven fixed result-column
constants and exactly:

```cpp
[[nodiscard]] dataframe compute_bayes_cone(
    const dataframe& predicted_returns,
    double starting_value = 1.0);
```

Python accepts a two-dimensional ndarray shaped
`[posterior_path, forecast_step]`; it computes
`np.cumprod(preds + 1, axis=1) * starting_value`, then applies NumPy's default
linear percentile method across paths for each forecast step. The native input
is deliberately its DataFrame transpose: raw index rows are forecast steps and
each genuine-`double` column is one posterior path. This preserves the
project's row-index semantics and avoids inventing an array wrapper. The result
retains that raw index, including duplicate/non-monotonic values, and contains
the ordered genuine-`double` columns `percentile_1`, `percentile_5`,
`percentile_25`, `percentile_50`, `percentile_75`, `percentile_95`, and
`percentile_99`.

For each path, native code starts wealth at `starting_value`, multiplies it by
`1 + predicted_return` in input row order, and computes the seven cross-path
percentiles at each row with accepted `detail::linear_percentile`. This is the
same linear interpolation rule as the source oracle. The returned DataFrame is
value-owning; it retains no input frame or column view.

## Native boundaries

At least one column is required, every input column must be a genuine `double`
column, and every column length must equal the frame index length. An
index-empty frame with at least one genuine empty double column is valid and
returns a typed-empty seven-column frame, matching Python's seven empty lists
for an array with paths but zero time steps. A nonempty index with no paths, a
mixed/non-double frame, or a malformed column is an `input_error`.

Unlike Python's permissive NumPy propagation, native code requires a finite
`starting_value`, finite every predicted return, and finite cumulative wealth
and interpolated percentiles. Zero, negative zero, negative starting values,
and returns at or below `-1.0` remain valid if their arithmetic stays finite.
This project-wide explicit-error policy prevents a partially non-finite table
from silently crossing the C++ API boundary. Allocation and length failures
are translated to the existing `input_error` boundary; no random engine,
sorting of the raw index, selector, cache, or Python/NumPy embedding is added.

The required source oracle uses three paths and four forecast steps with
`starting_value=2.0`; for example its median is
`[2.0, 2.0188, 2.0188, 2.0389880000000002]`, while its 1st percentile is
`[1.9608, 1.980384, 2.00018, 2.0389880000000002]`. Native tests transpose that
fixture into DataFrame columns and assert all seven columns row by row.

## Test and acceptance strategy

The leaf begins with a public-header-only RED before the header exists. GREEN
coverage proves the exact function/default expression; Python-oracle values in
the native orientation; raw-index preservation; one-path degenerate percentiles;
typed-empty output; strict structural/finite/overflow rejection; and ownership
after source mutation/destruction. The leaf must not test private helpers.

The follow-up façade consumer includes only `<fincore/metrics.hpp>` and first
captures the missing-export RED before adding the single umbrella include. It
asserts exact total schema/type/index and representative numerical endpoints.
Acceptance retains current/fresh AppleClang headless, registration-audited
Homebrew LLVM ASan+UBSan, ImPlot/demo compile/link-only without launching GUI,
invalid-ImGui multiline diagnostics, and actual GoogleTest FetchContent
provenance. It explicitly excludes PyMC, a Bayesian class, posterior fitting or
sampling, NumPy random-stream parity, `compute_consistency_score`, reports,
GUI runtime work, and any change to accepted metrics.
