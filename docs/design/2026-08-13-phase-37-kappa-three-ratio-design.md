# Phase 37 Native Kappa-Three Ratio Design

## Scope decision

Phase 36 completed native `r_cubed`. A fresh parity audit at accepted C++ HEAD
`f460860f5a40711b3adf1de23e36c8c8f738a046` and Python source HEAD
`1fd5f33e84bef2ec22e23eb9319f6e363084324b` selected
`kappa_three_ratio` as the next smallest **valuable, non-alias,
dependency-closed public calculation**.

That is a deliberately narrow route claim, not a claim that this is the
smallest absent Python name anywhere in the package:

- `stock_market_correlation`, `bond_market_correlation`, and
  `futures_market_correlation` are compatibility names over correlation logic
  already represented by native `safe_correlation`;
- `annualized_cumulative_return`, `annual_active_risk`, cross-module stats
  wrappers, and similar literal forwarding names map to existing native
  calculations rather than add a new numerical leaf; and
- `simulation.base.annualize` is a smaller standalone utility, but it is not an
  Empyrical-registered performance metric or a report calculation.

The ranked non-alias candidates are:

1. **Selected — `kappa_three_ratio`.** It is one one-column scalar calculation
   with NaN-only cleaning, a third-order lower partial moment, one strict
   threshold, and an explicit infinity policy. It is also the only candidate
   in this group consumed by the extended report.
2. **`adjusted_sharpe_ratio`.** Its three numerical leaves are native, but it
   adds raw-count dampening, NaN moment fallbacks, a squared Sharpe term, and
   distinct small- and large-sample clamps.
3. **`mar_ratio`.** Its drawdown and annualization leaves are native, but its
   raw-length, drawdown-before-cleaning, no-drawdown, invalid-annualization,
   NaN-only mean, and final-infinity order form a larger contract.
4. **`conditional_sharpe_ratio`.** It opens percentile interpolation,
   inclusive tail selection, tail sample-size, sample-standard-deviation,
   annualization, and nonfinite-percentile policies.

`r_cubed_turtle`, weekly/monthly resampling metrics, Hill EVT, solvers,
providers, reports as a native subsystem, plots, packages, and GUI runtime are
larger independent slices. Phase 37 adds no Python or pandas runtime,
NumPy/SciPy dependency, source target, facade include, result struct, provider,
network call, solver, report source, plot source, package, or GUI runtime.

## Python source, export, facade, and consumer contract

At the pinned Python HEAD, `fincore/metrics/ratios.py` exports
`kappa_three_ratio` through `ratios.__all__` and implements:

```python
def kappa_three_ratio(
    returns, risk_free=0.0, period=DAILY, annualization=None, mar=0.0
):
    if len(returns) < 2:
        return np.nan

    returns_array = np.asanyarray(returns)
    returns_clean = returns_array[~np.isnan(returns_array)]

    if len(returns_clean) < 2:
        return np.nan

    downside_deviations = np.maximum(0, mar - returns_clean)
    lpm3 = np.mean(downside_deviations**3)
    mu = np.mean(returns_clean)

    if lpm3 < 1e-30:
        return np.inf if mu > mar else np.nan

    return (mu - mar) / (lpm3 ** (1.0 / 3.0))
```

The name is metadata in `DUAL_RETURNS_REGISTRY`, but the current
`_populate_from_registry` decorator consumes only `CLASSMETHOD_REGISTRY` and
`STATIC_METHODS`. Actual class- and instance-level Empyrical reachability comes
from the explicit `@_dual_method` in `fincore/empyrical.py`, which fills stored
returns for an instance and forwards all five Python parameters in order.

The report engine constructs `Empyrical(returns=returns)` and calls
`emp.kappa_three_ratio(period=period)` for the `"Kappa Three Ratio"` extended
statistic. The forwarded `period` has no numerical effect. The name is not a
flat top-level `fincore` export and is not consumed by `AnalysisContext` or
`perf_stats`.

Existing Python tests cover the ordinary hand formula, all-positive `+inf`, a
high-MAR negative result, empty/singleton/all-NaN/one-clean input, acceptance
of `risk_free`, and Empyrical reachability. They do not fully lock dead-
parameter invariance, the strict threshold boundary, signed infinities,
nonfinite MAR, overflow-to-negative-zero, or index irrelevance.

## Native-first API and dead Python parameters

Extend `<fincore/metrics/ratios.hpp>` with exactly one declaration:

```cpp
double kappa_three_ratio(
    const dataframe& returns,
    double mar = 0.0,
    std::string_view column = returns_column);
```

Python's `risk_free`, `period`, and `annualization` arguments are deliberately
dead: instrumentation and source inspection show that the successful path
never reads them. The native leaf omits all three instead of accepting ignored
state or inventing validation that cannot affect the result. A future Python
adapter may preserve the Python call shape, accept those compatibility
arguments, deliberately discard them, and invoke this native leaf with only
`returns` and `mar`. The mandatory source oracle proves that even pathological
values for those three parameters leave the result unchanged.

This remains a one-column DataFrame scalar API. There is no overload,
automatic-input method, ndarray/Series API, multi-column result, parameter
object, alias, or new facade include. `<fincore/metrics.hpp>` already includes
`ratios.hpp` and must not change.

## Exact native control flow

The definition belongs in the existing `src/metrics/ratios.cpp` target and
must preserve this order:

```text
reject embedded NUL in column before every DataFrame call
raw_values = require_double_column(returns, column)
if raw_values.size < 2:
    return NaN
returns_clean = existing NaN-only valid_values(raw_values)
if returns_clean.size < 2:
    return NaN
if mar is NaN:
    return NaN

lpm3_total = 0
for value in returns_clean, in original row order:
    deviation = mar - value
    if deviation is NaN:       # including same-signed infinity subtraction
        return NaN
    downside = max(0, deviation)
    lpm3_total += downside * downside * downside
lpm3 = lpm3_total / returns_clean.size

mu = arithmetic mean(returns_clean), in original row order
if lpm3 < 1e-30:
    return +infinity if mu > mar else NaN
return (mu - mar) / std::pow(lpm3, 1.0 / 3.0)
```

The function must use the existing private `valid_values` helper in
`ratios.cpp`, whose contract removes NaN only and deliberately retains signed
infinity. It must not substitute `detail::finite_values`. The downside LPM is
computed before `mu`, matching source order. An explicit `std::isnan` check on
`mar - value` is required because `std::max(0.0, NaN)` does not reliably model
NumPy `maximum`, which propagates NaN for `+inf - +inf` and `-inf - -inf`.

The strict comparison is `< 1e-30`, not `<=`, and the branch predicate is
`mu > mar`, not numerator sign after a transformed or clamped calculation. The
ordinary result uses the direct expression with `std::pow` and a floating
`1.0 / 3.0` exponent. Do not use `std::cbrt`, `finite_quotient`, an absolute
value, epsilon addition, finite-result filter, range clamp, zero normalization,
or exception/warning translation. In particular, `+inf`, NaN, and negative
zero are meaningful source-compatible scalar results.

## Validation, indexing, and ownership

The pre-DataFrame NUL check prevents a bounded selector such as
`"strategy\0suffix"` from being truncated into a valid prefix by a downstream
C-string boundary. A legal bounded non-NUL-terminated `string_view` remains
supported because the DataFrame helper owns the exact selector during lookup.

After the NUL check, `require_double_column` owns missing-column, wrong-type,
and index/column-size diagnostics. Structural validation occurs before the raw
sample guard and before `mar` validation. A valid typed-empty frame and a
valid singleton return NaN; a malformed or wrong-type frame still throws even
when its apparent row count is short.

The scalar calculation reads values in raw DataFrame row order. It performs no
sorting, grouping, timestamp alignment, or deduplication. Duplicate,
nonmonotonic, and repeated timestamp labels therefore have no numerical effect
beyond preserving the associated raw value order. The returned scalar owns no
input span, column name, index, or DataFrame storage after the call.

## NaN, infinity, threshold, and overflow compatibility

Python removes NaN only. Native code deliberately preserves that rule for
this metric rather than inheriting the broader finite-only policy used by some
other leaves:

- positive infinity in returns can make `mu` positive infinity while downside
  risk remains finite, producing `+inf`;
- negative infinity creates infinite downside risk and a `-inf / +inf`
  result, producing NaN;
- both signed infinities make the mean NaN and LPM infinite, producing NaN;
- `mar=+inf` on ordinary finite values yields `-inf / +inf`, hence NaN;
- `mar=-inf` yields zero downside LPM and `mu > mar`, hence `+inf`;
- same-signed infinite `mar` and return values produce a NaN deviation and
  therefore NaN; the fixed `[+inf, 0.0]`/`mar=+inf` and
  `[-inf, 0.0]`/`mar=-inf` source cases each emit one subtraction warning and
  one scalar-subtraction warning before returning NaN; and
- a cubed downside overflow may make LPM infinite. For
  `[-DBL_MAX, 0.0]`, the finite numerator divided by an infinite cube root is
  source-compatible negative zero and must not be normalized to positive zero.

The threshold comparison is especially mutation-sensitive. Two identical
returns of `-nextafter(1e-10, 0.0)` produce an LPM strictly below `1e-30` and
take the threshold branch. Two identical `-1e-10` returns produce the exact
nominal `1e-30` boundary and must bypass that branch, returning approximately
`-0.9999999999999987`. Tests must include both sides and a below-threshold
fixture with positive `mu`, which must return `+inf`.

NumPy `RuntimeWarning` messages are source evidence only. Native C++ gains no
warning channel. Lowest-bit differences from compiler reduction and power
implementations are accepted only where tests specify an explicit tolerance;
classification, sign, strict-boundary, and negative-zero assertions remain
exact.

## Mandatory Anaconda source oracle

The fixed oracle at the pinned Python HEAD produces 23 metric-result lines plus
two threshold-diagnostic lines, 25 output lines in total:

```text
ordinary=0.1587401051968199; warnings=0; messages=-
ordinary_mar_0.1=-0.9280157765364051; warnings=0; messages=-
dead_params=0.1587401051968199; warnings=0; messages=-
empty=nan; warnings=0; messages=-
singleton=nan; warnings=0; messages=-
nan_mixed=0.4807498567691359; warnings=0; messages=-
one_clean=nan; warnings=0; messages=-
all_nan=nan; warnings=0; messages=-
all_positive=inf; warnings=0; messages=-
equal_mar=nan; warnings=0; messages=-
strict_below_negative=nan; warnings=0; messages=-
strict_below_positive_mu=inf; warnings=0; messages=-
nominal_boundary=-0.9999999999999987; warnings=0; messages=-
positive_inf=inf; warnings=0; messages=-
negative_inf=nan; warnings=1; messages=invalid value encountered in scalar divide
both_inf=nan; warnings=1; messages=invalid value encountered in reduce
mar_nan=nan; warnings=0; messages=-
mar_pos_inf=nan; warnings=1; messages=invalid value encountered in scalar divide
mar_neg_inf=inf; warnings=0; messages=-
same_pos_inf_mar=nan; warnings=2; messages=invalid value encountered in subtract | invalid value encountered in scalar subtract
same_neg_inf_mar=nan; warnings=2; messages=invalid value encountered in subtract | invalid value encountered in scalar subtract
max_overflow=-0.0; warnings=1; messages=overflow encountered in power
duplicate_nonmonotonic=0.19078570709222184; warnings=0; messages=-
below_lpm3=9.999999999999997e-31; hex=0x1.4484bfeebc29ep-100; below=True
exact_lpm3=1e-30; hex=0x1.4484bfeebc2a0p-100; below=False
```

For the dead-parameter case, `risk_free=NaN`, `period=object()`, and
`annualization=-inf` are passed together and the ordinary result is bitwise
unchanged. The threshold oracle also records that
`nextafter(1e-10, 0.0)^3` is `9.999999999999997e-31`, strictly below the
boundary, while `1e-10**3` is the nominal `1e-30`. Source warnings and their
counts are not C++ assertions.

## Mutation-resistant tests

The leaf and metrics-facade consumers must lock:

- the exact three-parameter function-pointer signature and a one-argument
  default call in both registered translation units;
- ordinary and nonzero-MAR Python goldens;
- dead Python parameter invariance in the source oracle and absence of all
  three dead parameters from the native function type;
- raw empty/singleton before cleaning, mixed NaN, one cleaned value, all NaN,
  and all-positive `+inf`;
- `mu == mar` with zero LPM returning NaN;
- strictly-below and exact-equality `1e-30` cases, plus below-threshold positive
  `mu`, killing `<`/`<=`, threshold, and branch-predicate mutations;
- positive infinity, negative infinity, both infinities, NaN MAR, positive and
  negative infinite MAR, and both fixed same-signed infinity/MAR subtraction
  cases `[+inf, 0.0]`/`+inf` and `[-inf, 0.0]`/`-inf`;
- maximum-finite downside-cube overflow returning a negative-zero result, with
  both `result == 0.0` and `std::signbit(result)` asserted;
- original-order arithmetic and duplicate/nonmonotonic timestamp irrelevance;
- named and default columns plus two legal bounded nonterminated selectors;
- embedded NUL before every DataFrame call, including a valid prefix and a
  structurally invalid frame to prove precedence;
- empty selector, missing column, wrong type, and index/value mismatch;
- result ownership after local input and selector storage is destroyed; and
- reachability/default use through `<fincore/metrics.hpp>`.

`tests/unit/ratios_test.cpp` retains `<fincore/metrics/ratios.hpp>` as its only
fincore include. Both test translation units receive compile uses before the
production declaration exists, so the retained RED must show the missing
symbol in both consumers. CTest must not run after the failed build.

## Exact implementation and acceptance scope

Task 157 changes exactly:

- `include/fincore/metrics/ratios.hpp`;
- `src/metrics/ratios.cpp`;
- `tests/unit/ratios_test.cpp`; and
- `tests/unit/metrics_umbrella_test.cpp`.

`ratios.cpp` and both test files are already registered, and the metrics facade
already includes the ratios leaf. No CMake, dependency, facade, or new test
file change is permitted.

Acceptance requires the mandatory Python oracle, genuine dual-consumer Ninja
`-k 0` RED, current build/registration/focused/related/full CTest, fresh
AppleClang headless, Homebrew LLVM 22 ASan+UBSan registration/runtime/
instrumentation/diagnostic evidence, local ImGui/ImPlot/demo compile-link-only
without running the demo or launching a GUI, a fresh invalid-ImGui configure
matched across physical line wraps, and exact dependency provenance from each
fresh build.

The phase does not add the dead Python parameters, Python adapters, literal
aliases, adjusted or conditional Sharpe, `mar_ratio`, `r_cubed_turtle`,
calendar-resampled metrics, EVT, providers, reports as a native subsystem,
plots, packages, or GUI runtime validation.
