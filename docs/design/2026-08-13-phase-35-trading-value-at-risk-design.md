# Phase 35 Native Trading Value at Risk Design

## Scope decision

Phase 34 completed `var_excess_return`. A fresh whole-package route audit first
considered `mar_ratio`, then found a smaller genuine public parity gap:
`trading_value_at_risk` in `fincore.metrics.risk`.

The final ranking is:

1. **Selected — `trading_value_at_risk`.** The Python successful path is only
   a raw-length guard, arithmetic mean, sample standard deviation, and direct
   subtraction. The existing risk header/source and already registered
   leaf/facade tests are sufficient, so the slice adds no CMake entry, target,
   facade include, or dependency.
2. **`loss_rate` is already native.** It is declared in `metrics/stats.hpp`,
   defined in `metrics/stats.cpp`, exported by `<fincore/metrics.hpp>`, and
   tested. Its accepted native signed-infinity filtering policy is not a
   missing API.
3. **`mar_ratio`.** It remains the next fallback, but adds public drawdown and
   annualization composition, NaN-only cleaning, no-drawdown ordering,
   invalid-frequency/override behavior, and a final infinity clamp.
4. **`r_cubed`, `kappa_three_ratio`, and adjusted/conditional Sharpe.** These
   require paired alignment or additional moment, lower-partial-moment,
   percentile, cleaning, and clamp policies.
5. **Weekly/monthly consecutive statistics and EVT.** pandas resampling is not
   a thin mapping to accepted native calendar buckets, while EVT opens a new
   result/header/source/CMake surface and tail/threshold policy.

Python exposes `trading_value_at_risk` through `fincore.metrics.risk`, the lazy
`metrics.risk_module`, and the `Empyrical` classmethod registry. It is not a
top-level flat `fincore` symbol, is not in the dual-returns registry, and is not
consumed by current reports or analysis context. Phase 35 therefore adds the
single native risk leaf and obtains metrics-facade reachability through the
already accepted `risk.hpp` include.

This phase adds no embedded Python/pandas runtime, NumPy/SciPy dependency,
provider, network call, solver, source target, alias, report, plot source,
package, or GUI runtime.

## Python source contract

At Python source HEAD `1fd5f33e84bef2ec22e23eb9319f6e363084324b`,
`fincore/metrics/risk.py` defines:

```python
def trading_value_at_risk(returns, period=None, sigma=2.0):
    if len(returns) < 2:
        return np.nan
    mean_ret = np.mean(returns)
    std_ret = np.std(returns, ddof=1)
    return mean_ret - sigma * std_ret
```

The source never reads `period`; instrumentation observes `np.mean` once and
`np.std(..., ddof=1)` once only after the raw-length guard. It does not clean
NaN or signed infinity, validate `sigma`, clamp a nonfinite result, or emit an
application diagnostic. NumPy `RuntimeWarning` messages are arithmetic runtime
artifacts rather than part of the scalar return API.

Python has a container-dependent wrinkle: NumPy arrays propagate raw NaN,
whereas a pandas Series may dispatch reductions through pandas and skip NaN.
The native one-column DataFrame API deliberately maps its raw column span to
the NumPy-array path. It does not reproduce pandas container dispatch.

## Public native API

Extend `<fincore/metrics/risk.hpp>` with exactly one declaration:

```cpp
double trading_value_at_risk(
    const dataframe& returns,
    double sigma = 2.0,
    std::string_view column = returns_column);
```

The native API omits Python's unused `period`. Exposing a `frequency` value
would invent validation and state absent from the source; exposing an ignored
native parameter would make the core API misleading. A future language adapter
may retain Python's call shape, accept `period`, and discard it before invoking
this native leaf.

This is deliberately a one-column scalar API. A multi-column, array, pandas,
or automatic-returns overload would broaden the selected native API surface.
The existing `<fincore/metrics.hpp>` facade already includes `risk.hpp` and
must not change.

## Native control flow

The definition belongs in the existing `src/metrics/risk.cpp` target and uses
the accepted internal numeric helpers:

```text
reject embedded NUL in column
values = require_double_column(returns, column)
if values.size < 2:
    return NaN

average = detail::mean(values)
variance = detail::sample_variance(values)
standard_deviation = sqrt(variance)
return average - sigma * standard_deviation
```

The definition directly calls `detail::mean` once for the returned mean stage
and `detail::sample_variance` once for the sample-variance stage. The accepted
sample-variance helper internally performs its own mean pass. No production
helper is changed merely to fuse those passes.

The implementation must not use `detail::finite_values`, `annual_volatility`,
`historical_value_at_risk`, population variance, an annualization factor, a
zero-standard-deviation guard, a finite-only result helper, or a private
NumPy-compatible reduction.

## Validation and ownership

Before any DataFrame C-string boundary, `column.find('\0')` rejects an embedded
NUL with `fincore::input_error`. `require_double_column` then validates the
exact bounded selector, column presence, genuine-double type, and index/column
length before the raw `<2` shortcut.

Consequently:

- a valid zero- or one-row genuine-double column returns NaN before `sigma`
  affects arithmetic;
- an empty selector, missing column, wrong type, or malformed column length is
  still a structural error even for a short frame;
- a legal bounded non-NUL-terminated `string_view` is accepted; and
- an embedded-NUL selector cannot be truncated into a valid DataFrame column.

The result is a scalar. No input span, selector view, index, or DataFrame
storage is retained after the call.

## Arithmetic and compatibility boundaries

The native leaf preserves direct C++/IEEE behavior:

- sample variance uses denominator `n - 1`, killing a population-variance
  substitution;
- a constant series has standard deviation zero and returns its mean for any
  finite `sigma`;
- `sigma` may be negative, zero, NaN, positive infinity, or negative infinity;
- raw NaN is not filtered and propagates to NaN;
- raw signed infinity is not filtered and normally yields NaN through the
  variance arithmetic;
- zero times signed infinity remains NaN rather than receiving a zero-variance
  shortcut;
- a legitimate final signed infinity is preserved rather than clamped; and
- overflow, cancellation, and reduction order follow the existing native
  `detail::mean` and `detail::sample_variance` helpers.

The last point is an accepted native arithmetic boundary. NumPy may use a
different internal reduction order for extreme values, but the native API does
not add a Python/pandas compatibility reduction. Python warning counts and
texts are retained as source evidence only; C++ does not gain a warning
channel.

## Mandatory Python source oracle

The NumPy-array source oracle fixes these values and warning observations:

```text
ordinary_default=-0.043665562901805; warnings=0; messages=-
ordinary_sigma_1=-0.018499448117569168; warnings=0; messages=-
ordinary_sigma_0=0.006666666666666665; warnings=0; messages=-
ordinary_sigma_neg3=0.08216501101937417; warnings=0; messages=-
ordinary_sigma_nan=nan; warnings=0; messages=-
ordinary_sigma_pos_inf=-inf; warnings=0; messages=-
ordinary_sigma_neg_inf=inf; warnings=0; messages=-
bad_period_ignored=-0.018499448117569168; warnings=0; messages=-
empty_bad_sigma=nan; warnings=0; messages=-
single_bad_sigma=nan; warnings=0; messages=-
flat_positive=0.1; warnings=0; messages=-
flat_positive_sigma_pos_inf=nan; warnings=1; messages=invalid value encountered in scalar multiply
flat_zero_sigma_pos_inf=nan; warnings=1; messages=invalid value encountered in scalar multiply
nan_mixed=nan; warnings=0; messages=-
pos_inf_mixed=nan; warnings=1; messages=invalid value encountered in subtract
neg_inf_mixed=nan; warnings=1; messages=invalid value encountered in subtract
both_inf=nan; warnings=2; messages=invalid value encountered in reduce | invalid value encountered in reduce
max_symmetric=-inf; warnings=1; messages=overflow encountered in multiply
max_symmetric_sigma_neg2=inf; warnings=1; messages=overflow encountered in multiply
```

Native tests compare finite values with appropriate tolerance and lock result
classification/sign for NaN and infinity. They do not assert Python warning
emission from C++.

## Mutation-resistant test design

The leaf and metrics-facade consumers must lock:

- the exact three-parameter function-pointer signature and one-argument
  default call;
- ordinary default, `sigma = 1`, `0`, and `-3` oracle values;
- an independent finite mean/sample-standard-deviation calculation that
  distinguishes `ddof=1` from `ddof=0`;
- valid zero- and one-row typed columns returning NaN before numerical
  arithmetic;
- finite `sigma` signs plus NaN and both signed infinities;
- constant nonzero and zero inputs, including `zero * infinity -> NaN`;
- raw NaN and raw signed infinity in fixtures that retain at least two finite
  observations after a hypothetical filter, so a `finite_values` mutant would
  return a finite number and fail;
- symmetric maximum finite inputs preserving negative/positive infinity for
  positive/negative `sigma`;
- default and named columns, a legal bounded nonterminated selector, embedded
  NUL, empty selector, missing column, wrong type, and index/column mismatch;
- structural validation before the raw-short shortcut; and
- reachability and default use through `<fincore/metrics.hpp>`.

The leaf test includes only `<fincore/metrics/risk.hpp>` among fincore headers.
Leaf and facade compile uses are added before the production declaration so a
genuine missing-symbol dual-translation-unit RED is retained. No CTest result
from a failed build may be cited as RED evidence.

## Acceptance boundary

Acceptance requires current build, registration, focused/related/full CTest,
fresh AppleClang headless, Homebrew LLVM 22 ASan+UBSan with registration,
instrumentation, runtime, warning, and diagnostics audits, local
ImGui/ImPlot/demo compile-link-only without invoking the demo or launching a
GUI, and a fresh invalid-ImGui configuration failure matched across physical
line wraps. Actual FetchContent and local dependency provenance must be
recorded from each fresh build.

The phase does not implement `mar_ratio`, `r_cubed`, `kappa_three_ratio`,
adjusted or conditional Sharpe, calendar-resampled consecutive statistics,
EVT, provider/solver routes, aliases, reports, plots, or Python container
adapters.
