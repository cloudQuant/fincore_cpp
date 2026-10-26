# Phase 21 Bootstrap Confidence-Interval Design

**Date:** 2026-08-12

## Decision

Phase 21 ports the finite percentile interval core of Python
`fincore.simulation.bootstrap.bootstrap_ci`.  It is a small consumer of the
accepted Phase 20 bootstrap-distribution leaf: validate the confidence level,
call `bootstrap_distribution` exactly once, and linearly interpolate the two
percentile cutoffs from that distribution.  No second random engine, sampler,
statistic implementation, DataFrame helper, or Python binding is introduced.

The scope intentionally stops before Python's summary dictionary and callable
statistics.  It also exposes only the source's implemented percentile method;
the Python `method` parameter only rejects every non-`"percentile"` value, so
adding an otherwise unusable native method enum would be API surface without
capability.

## Public API

The API extends the already-exported
`include/fincore/simulation/bootstrap.hpp`:

```cpp
struct bootstrap_confidence_interval_result {
    double lower;
    double upper;
};

bootstrap_confidence_interval_result bootstrap_confidence_interval(
    const dataframe& returns,
    std::size_t resample_count = 10000U,
    double alpha = 0.05,
    bootstrap_statistic statistic = bootstrap_statistic::mean,
    std::optional<std::uint64_t> seed = std::nullopt,
    std::string_view column = returns_column);
```

`alpha` must be finite and strictly inside `(0, 1)`.  The lower and upper
cutoffs are respectively `alpha / 2` and `1 - alpha / 2`, identical to the
source percentile method.  The result is a small value-owning native struct,
not a one-row DataFrame or a string-keyed map: the two scalar endpoints are the
only stable data contract at this layer.

`bootstrap.hpp` was already exported by `<fincore/simulation.hpp>` in Phase
20.  Therefore the new declaration automatically becomes available through
the simulation facade.  Task 92 will capture both a leaf-only and a true
umbrella-only missing-symbol RED before adding the declaration; Task 93 needs
no redundant facade include and accepts the existing transitive export.

## Delegation and numerical semantics

After alpha validation, the operation delegates to
`bootstrap_distribution` exactly once with every caller-selected distribution
argument unchanged.  This preserves the accepted selector ownership,
genuine-double/index validation, source-order/NaN policy, signed-infinity
input rejection, native MT19937-64 stream, named statistic behavior, and
native-only seed-repetition boundary.  It then reads the one generated
`bootstrap` column and computes NumPy's finite `method="linear"` percentile:
sort finite values, form `p * (n - 1)`, and interpolate between its adjacent
order statistics.

For finite distributions this has the same mathematical percentile semantics
as Python `np.percentile`.  Native nonconstant seeded results deliberately do
not claim NumPy output equality, because the distribution producer uses a
different random engine.  Deterministic all-equal source fixtures are shared
exactly, and nonconstant native tests derive expected endpoints from the
accepted public distribution with an independent linear interpolation oracle.

Python can run `np.percentile` over a distribution containing `NaN` or signed
infinity and often returns warning-producing/nonportable `NaN` endpoints.  A
native confidence interval has a stronger boundary: any nonfinite bootstrap
statistic causes `fincore::input_error` before sorting.  Thus singleton and
zero-denominator Sharpe/Sortino distributions, which Phase 20 deliberately
permits as `NaN`/`+inf` data, are valid distribution outputs but invalid finite
confidence-interval inputs.  This explicit safety difference is documented
and tested; it does not alter Phase 20's behavior.

Likewise, zero resample count remains a valid typed-empty *distribution*, but
is an invalid confidence interval and throws `input_error`, rather than
allowing Python's implementation-dependent empty-percentile `IndexError`.
The interval rejects unknown statistic values, unsafe allocated/index sizes,
nonfinite source values, invalid selectors, and overflow in the finite
interpolation arithmetic through the existing delegate or a local checked
percentile routine.  No partial result is returned.

## Source oracle and verification

The mandatory Anaconda source oracle records the exact source values for
nonconstant seeded mean and all-equal mean/standard-deviation cases, plus
alpha errors, zero-sample behavior, and a degenerate Sharpe/Sortino output.
It records NumPy warnings truthfully.  The latter two establish a deliberate
native safety difference; they are not treated as a false parity assertion.

Task 92 starts with public-header-only leaf and umbrella consumers that fail
only because the new result/function is absent.  GREEN tests prove exact
signature/defaults, source-compatible deterministic endpoints, alpha cutoff
behavior, one-delegate public-distribution parity, zero-count/nonfinite-output
rejection, selector/type/value/unknown-enum propagation, no accidental
source-index influence, and native seed repeatability without NumPy RNG claims.
The next acceptance task preserves raw AppleClang, LLVM ASan+UBSan,
ImPlot compile/link-only/no-GUI, invalid-ImGui, and FetchContent provenance
evidence.  Bootstrap summaries, callable statistics, method variants,
full NumPy nonfinite percentile behavior, Monte Carlo, reports, packaging,
and GUI runtime remain excluded.
