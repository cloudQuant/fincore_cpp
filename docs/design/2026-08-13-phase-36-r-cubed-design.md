# Phase 36 Native R-Cubed Design

## Scope decision

Phase 35 completed `trading_value_at_risk`. A fresh whole-package route audit
ranked the remaining successful local calculations and selected `r_cubed` as
the smallest remaining **Empyrical-registered, non-alias metric calculation
whose numerical leaf is already native**.

That wording is intentional. This is not a claim that `r_cubed` is the
smallest absent Python name or utility anywhere in the package:

- `stock_market_correlation`, `bond_market_correlation`, and
  `futures_market_correlation` are compatibility names for correlation logic
  already represented by native `safe_correlation`;
- `annualized_cumulative_return`, `cum_returns`, `cum_returns_final`,
  `value_at_risk`, and cross-module stats wrappers similarly map to existing
  canonical native symbols; and
- `simulation.base.annualize` is a smaller standalone utility, but is neither
  exported by the simulation facade nor registered on `Empyrical` and is
  outside this metric-route criterion.

The ranked non-alias metric candidates are:

1. **Selected — `r_cubed`.** The native public `safe_correlation` already
   owns alignment, pair filtering, minimum-sample, variance-threshold, and
   Pearson-correlation semantics. The new metric is one public-leaf call and
   a signed cube in the existing stats header/source.
2. **`kappa_three_ratio`.** It requires NaN-only cleaning, a third-order lower
   partial moment, a `1e-30` boundary, an infinity policy, and decisions for
   three unused Python compatibility parameters, although reports consume it.
3. **`adjusted_sharpe_ratio`.** It composes three existing leaves but also
   depends on raw sample-count dampening, NaN moment fallbacks, and two clamp
   stages.
4. **`mar_ratio`.** It adds raw-length, maximum-drawdown, no-drawdown,
   annualization, NaN-only mean, invalid-period/override, and final-infinity
   ordering.
5. **`conditional_sharpe_ratio`.** It opens percentile, tail-subset,
   sample-standard-deviation, and nonfinite percentile policies.

Weekly/monthly consecutive metrics still require pandas resampling semantics
that are not a thin mapping to the accepted native calendar buckets. Hill EVT
requires a new result/API/source surface and threshold/tail policy. Solver,
provider, report, plot, package, and GUI paths remain outside this slice.

This phase adds no embedded Python/pandas runtime, NumPy/SciPy dependency,
source target, facade include, alias, provider, network call, solver, report,
plot source, package, or GUI runtime.

## Python source contract

At Python source HEAD `1fd5f33e84bef2ec22e23eb9319f6e363084324b`,
`fincore/metrics/stats.py` defines the successful path as:

```python
def r_cubed(returns, factor_returns):
    returns_aligned, factor_aligned = aligned_series(
        returns, factor_returns)
    ret_arr = np.asarray(returns_aligned, dtype=float)
    fac_arr = np.asarray(factor_aligned, dtype=float)
    mask = ~(np.isnan(ret_arr) | np.isnan(fac_arr))
    if mask.sum() < 2:
        return np.nan
    correlation = _safe_correlation(ret_arr[mask], fac_arr[mask])
    return correlation**3 if not np.isnan(correlation) else np.nan
```

The symbol is present in `metrics.stats.__all__` and is listed as metadata in
`DUAL_RETURNS_FACTOR_REGISTRY`. The current `Empyrical` implementation does
not consume that dual registry; facade reachability comes from its explicit
`r_cubed` dual method, which fills both stored inputs. The symbol is not a
top-level flat `fincore` export and is not consumed by the current report or
analysis context.

The source result is **signed**; its mathematical range is `[-1, 1]`, subject
to ordinary floating-point rounding at the endpoints. A negative correlation
must produce a negative cube; this is not R-squared, CAPM R-squared, an
absolute correlation, or a nonnegative score. Some existing Python test prose
suggests `[0, 1]`, but the implementation and mathematical contract are
unambiguously signed. Native code must not add a range clamp.

For unique pandas indexes, outer alignment followed by pairwise NaN masking is
equivalent to using the common timestamp intersection. Equal-length NumPy
arrays are treated as already positionally aligned. The native core accepts
DataFrames only and therefore uses their timestamp indexes; a future array
adapter would be responsible for synthesizing positional indexes.

## Public native API

Extend `<fincore/metrics/stats.hpp>` with exactly one declaration:

```cpp
double r_cubed(
    const dataframe& returns,
    const dataframe& benchmark,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);
```

The result and both inputs use existing native types. No result struct,
selector object, overload, automatic-input API, compatibility alias, or
Python-container overload is added. `<fincore/metrics.hpp>` already includes
`stats.hpp` and must not change.

## Native control flow

The definition belongs in the existing `src/metrics/stats.cpp` target:

```text
reject embedded NUL in returns selector
reject embedded NUL in benchmark selector
correlation = safe_correlation(
    returns, benchmark, returns selector, benchmark selector) exactly once
if correlation is NaN:
    return NaN
return correlation * correlation * correlation
```

Both NUL checks occur before any DataFrame call. The wrapper must not copy
alignment, filtering, mean, variance, covariance, or Pearson formulas. It must
not delegate through `capm_r_squared`, take a square root or cube root, use
`std::abs`, use a square, use `std::pow`, or clamp the result.

The public `safe_correlation` leaf retains its accepted order:

1. validate the returns selector and genuine-double/index shape;
2. validate the benchmark selector and genuine-double/index shape;
3. align on common timestamps in left-input order;
4. retain only pairs for which both values are finite;
5. reject fewer than two pairs, nonfinite means/variances/standard deviations,
   and population standard deviations below `1e-15`; and
6. return a finite Pearson correlation or NaN, including NaN for overflow in
   otherwise finite input arithmetic.

The R-cubed wrapper does not add another raw-length or variance policy.

## Validation, alignment, and ownership

The explicit NUL checks prevent a bounded selector such as
`"strategy\0suffix"` from being truncated by a downstream C-string boundary
into the valid prefix `"strategy"`. A legal bounded, non-NUL-terminated
`string_view` remains supported because the DataFrame helper owns an exact
`std::string` copy before lookup.

Both selector NUL checks precede every DataFrame operation. Once they pass,
structural errors retain the public leaf order: returns-side empty/missing/
wrong-type/malformed columns are observed before benchmark-side errors.

Unique, nonmonotonic timestamp indexes are positional only for iteration; the
calculation uses the common timestamp keys. Duplicate timestamps inherit the
existing native alignment policy: left rows are retained in left order and a
right-side duplicate resolves to the first inserted value. This phase does not
attempt pandas duplicate-index broadcast/reindex parity.

The result is a scalar. No input span, selector view, index, or DataFrame
storage is retained after the call.

## Native finite-value compatibility boundary

Python `r_cubed` removes NaN pairs but leaves signed infinity for NumPy's
correlation arithmetic, which normally returns NaN and may emit a
`RuntimeWarning`. Native `safe_correlation` already removes every nonfinite
pair. Phase 36 deliberately inherits that accepted native leaf policy rather
than introducing a third filtering implementation.

For the fixed signed-infinity fixture

```text
returns   = [0.01, +inf, -0.02, 0.03]
benchmark = [0.005, 0.1, -0.01, -0.01]
```

Python returns NaN with one `invalid value encountered in subtract` warning.
Native filtering retains three finite pairs, for which
`safe_correlation` is approximately `0.1147078669352809` and `r_cubed` is
approximately `0.0015093140386221175`; tests use an explicit tolerance.
Negative infinity at the same position has the same native result. This
divergence must be documented and mutation-locked; the wrapper must not
reintroduce Python's infinity behavior.

NumPy warning counts and messages are source evidence only. C++ does not gain
a warning channel. Lowest-bit correlation differences caused by reduction
order are accepted within explicit numeric tolerances.

## Mandatory Python source oracle

The fixed Python oracle produces:

```text
ordinary_positive=0.9407744656599973; warnings=0; messages=-
ordinary_negative=-0.9407744656599973; warnings=0; messages=-
perfect_positive=1.0; warnings=0; messages=-
perfect_negative=-1.0; warnings=0; messages=-
pairwise_nan=1.0; warnings=0; messages=-
constant=nan; warnings=0; messages=-
tiny_variation=nan; warnings=0; messages=-
empty=nan; warnings=0; messages=-
one_pair=nan; warnings=0; messages=-
positive_inf_pair=nan; warnings=1; messages=invalid value encountered in subtract
negative_inf_pair=nan; warnings=1; messages=invalid value encountered in subtract
huge_pair=nan; warnings=1; messages=overflow encountered in multiply
misaligned_series=-0.9999999999999987; warnings=0; messages=-
one_common_series=nan; warnings=0; messages=-
```

Native tests use the same finite and index fixtures where the accepted native
policy matches, and separately lock the intentional signed-infinity
divergence. They do not assert Python warning emission from C++.

## Mutation-resistant test design

The leaf and metrics-facade consumers must lock:

- the exact four-parameter function-pointer signature and a two-argument
  default-column call in both translation units;
- ordinary positive and negative non-unit Python goldens;
- perfect positive `+1` and perfect negative `-1`, killing a square,
  absolute-value, CAPM-R-squared, and sign-loss substitution;
- direct parity with exactly one public `safe_correlation` result cubed;
- a finite two-point endpoint-rounding fixture with identical
  `{-10.0, -0.3}` columns: on both target compilers the public leaf is about
  `1.0000000000000002`, so the result must be strictly greater than `1.0` and
  exactly follow `correlation * correlation * correlation`; this kills an
  otherwise-surviving `[-1, 1]` clamp mutant;
- unique-index timestamp intersection, including nonmonotonic input order;
- pairwise NaN removal, no common timestamps, one remaining finite pair,
  valid typed-empty inputs, constant input, and below-`1e-15` variation;
- symmetric maximum-finite inputs whose native variance arithmetic overflows
  and therefore returns NaN;
- accepted native positive- and negative-infinity filtering, with a finite
  result that differs from Python;
- default and named columns plus two legal bounded nonterminated selectors;
- embedded NUL in each selector, with a valid prefix column and the other
  frame structurally invalid so both pre-DataFrame checks are observable;
- empty selectors, missing columns, wrong types, and index/column mismatch on
  each side in public-leaf order;
- scalar ownership after local input and selector storage is destroyed; and
- reachability and default use through `<fincore/metrics.hpp>`.

`tests/unit/stats_test.cpp` removes its direct `<fincore/error.hpp>` include so
`<fincore/metrics/stats.hpp>` remains its only fincore include. Leaf and facade
compile uses are added before the production declaration so a genuine missing-
symbol dual-translation-unit RED is retained. No CTest result from the failed
build may be cited as RED evidence.

## Exact implementation scope

Task 153 modifies exactly:

- `include/fincore/metrics/stats.hpp`;
- `src/metrics/stats.cpp`;
- `tests/unit/stats_test.cpp`; and
- `tests/unit/metrics_umbrella_test.cpp`.

`stats.cpp` is already in the library target, both test files are already
registered, and the metrics facade already includes the leaf. No CMake,
dependency, facade, source-list, or new-test-file change is permitted.

## Acceptance boundary

Acceptance requires current build, registration, focused/related/full CTest,
fresh AppleClang headless, Homebrew LLVM 22 ASan+UBSan with registration,
instrumentation, loaded-runtime, warning, and diagnostics audits, local
ImGui/ImPlot/demo compile-link-only without invoking the demo or launching a
GUI, and a fresh invalid-ImGui configuration failure matched across physical
line wraps. Actual FetchContent and local dependency provenance must be
recorded from each fresh build.

The phase does not add compatibility aliases, `simulation.base.annualize`,
`kappa_three_ratio`, adjusted/conditional Sharpe, `mar_ratio`, calendar-
resampled consecutive statistics, EVT, solver/provider routes, reports, plots,
packages, or Python container adapters.
