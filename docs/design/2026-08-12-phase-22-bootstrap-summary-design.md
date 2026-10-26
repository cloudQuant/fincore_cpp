# Phase 22 Bootstrap Summary Design

**Date:** 2026-08-12

## Decision

Phase 22 adds a value-owning native summary for the four statistics exposed by
Python `fincore.simulation.bootstrap.bootstrap_summary`: mean, sample standard
deviation, Sharpe ratio, and Sortino ratio.  It is deliberately a consumer of
the accepted bootstrap-distribution/finite-confidence-interval core, rather
than a second sampler or a string-keyed Python-dictionary analogue.

The Python helper is not itself a safe public numerical boundary.  It uses raw
returns for its `value` fields after its sampler has dropped NaNs, accepts
`alpha=0` and `alpha=1`, permits an empty or singleton distribution to reach
NumPy warning/`NaN` paths, and returns `NaN`/infinity summary components for
degenerate ratio samples.  The native API therefore adopts the already accepted
Phase 20/21 safety policy: it filters input NaNs consistently for every field,
requires a finite strict alpha and at least two resamples, and returns a result
only if every source statistic, generated bootstrap value, standard error, and
interval endpoint is finite.  This is a documented native safety divergence,
not a claim of byte-for-byte NumPy parity.

## Public API

The API extends the already exported
`include/fincore/simulation/bootstrap.hpp`:

```cpp
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
```

The fixed field names make the four-statistic contract explicit and stable;
there is no map ordering, string lookup, DataFrame schema, callable hook, or
report type to own.  Each result is value-owning.  As in Phases 20 and 21,
`<fincore/simulation.hpp>` already includes `bootstrap.hpp`, so extending this
leaf automatically exports the new symbol through the simulation facade.

## Delegation, data ownership, and finite semantics

After validating `alpha` and `resample_count >= 2`, the implementation copies
the NUL-free selected column name, reads the genuine-double source column once,
and applies the accepted bootstrap NaN-drop/signed-infinity-rejection rule.
The retained finite values supply all four observed `value` fields.  This avoids
the Python mismatch where NaNs are excluded from the distribution but retained
in the summary value.

For each fixed enum in source order—`mean`,
`sample_standard_deviation`, `sharpe_ratio`, and `sortino_ratio`—the summary
calls public `bootstrap_distribution` exactly once with the caller's
`returns`, `resample_count`, `seed`, and selected column, changing only that
enum.  The same explicit seed is therefore passed to each independently
fresh-seeded distribution, matching the source helper's per-statistic seed
boundary without asserting a NumPy stream match.  No sampling, random engine,
DataFrame output materialization, selector validation, or named-statistic logic
is reimplemented.

Each generated distribution must contain exactly the accepted typed bootstrap
column and at least two finite values.  Its sample standard deviation (`ddof=1`)
is the reported standard error.  The Phase 21 finite linear-percentile helper
is factored privately and reused to calculate the two interval endpoints from
that *same* distribution; summary must not call public
`bootstrap_confidence_interval`, because that would resample a second time.
The existing CI still calls public `bootstrap_distribution` exactly once and
retains all Phase 21 behavior.

An observed ratio value or a generated distribution statistic that is `NaN`,
`+inf`, or `-inf` makes the whole summary invalid and raises `fincore::input_error`.
So do `resample_count` zero/one, invalid alpha, empty/all-NaN/infinite input,
wrong columns, unsafe selector names, unknown/overflow errors inherited from
the accepted leaf.  No partial four-field result is returned.  This makes a
constant positive-return source reject at the ratio component instead of
returning Python's `inf`/`NaN` dictionary values.  The restriction is deliberate
and specifically tested.

Finite nonconstant seeded values are not asserted equal to Python, because the
native producer's MT19937-64/rejection sampler intentionally differs from
NumPy `default_rng`.  Tests derive per-statistic expected standard errors and
intervals from one public native distribution using only published sample-
standard-deviation and linear-percentile formulas.  Source oracle fixtures are
used for deterministic relationships and to document the intentional safety
boundary.

## Scope and verification

The leaf task begins with both a leaf-header and simulation-facade public
consumer RED while the types/function do not exist.  GREEN coverage proves the
exact type/default contract; four distribution delegations with the same seed;
per-field public-distribution parity; alpha/count ordering; NaN filtering;
nonfinite/degenerate rejection; selector/type/value propagation; raw-index
independence; source mutation/destruction ownership; seeded repeatability; and
the automatic facade export.  It does not duplicate the sampler or claim
NumPy random-stream equality.

The acceptance task retains configured, fresh AppleClang headless, Homebrew
LLVM ASan+UBSan, ImPlot/demo compile/link-only/no-GUI, and invalid-ImGui raw
evidence, including actual GoogleTest FetchContent provenance.  It records the
new source oracle's safe-divergence observations without overstating Python
parity.  Callable statistics, source dictionary shape, endpoint alpha support,
nonfinite summary output, Monte Carlo, antithetic/LHS, reports, packaging, and
GUI runtime stay out of scope.
