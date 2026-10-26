# Phase 34 Native VaR Excess Return Design

## Scope decision

Phase 33 completed `common_sense_ratio` and left the remaining scalar metrics as
separate parity debt. Phase 34 selects `var_excess_return` as the smallest
useful dependency-closed missing public metric in the Python package.

The route audit established:

1. **Selected — `var_excess_return`.** Its successful path is a thin scalar
   composition of the already accepted public native `annual_return`,
   `annualization_factor`, and `historical_value_at_risk` leaves. It fits the
   existing risk header/source and already registered risk/facade tests, adds
   no CMake entry, and automatically reaches `<fincore/metrics.hpp>`.
2. **`loss_rate` is already native.** It is declared in `metrics/stats.hpp`,
   implemented in `metrics/stats.cpp`, exported by the metrics facade, and has
   dedicated tests. Its accepted native signed-infinity filtering difference
   is not a missing API.
3. **`mar_ratio` and adjusted/conditional Sharpe variants.** These remain
   useful but add more private arithmetic, cleaning, or percentile policy than
   the selected three-leaf composition.
4. **Weekly/monthly consecutive statistics.** pandas resampling semantics do
   not match the accepted native calendar buckets across gaps and year
   boundaries, so a thin wrapper would be misleading.
5. **EVT `hill_estimator`.** It opens a new result/header/source/CMake surface
   and requires new tail and threshold policy.

Phase 33's statement that `var_excess_return` was excluded was explicitly
phase-local. Phase 34 selects that debt without rewriting Phase 33 history.
This phase adds no embedded Python/pandas runtime, NumPy/SciPy dependency,
provider, network call, solver, source target, plot source, report, package, or
GUI runtime.

## Public API

Extend `<fincore/metrics/risk.hpp>` with exactly one overload:

```cpp
double var_excess_return(
    const dataframe& returns,
    double cutoff = 0.05,
    double risk_free = 0.0,
    frequency period = frequency::daily,
    std::optional<double> annualization = std::nullopt,
    std::string_view column = returns_column);
```

The native `frequency` enum replaces Python's free-form period string while
retaining the five accepted frequencies. The optional annualization override
preserves the Python call shape. Its value follows the accepted native
`annualization_factor` and `annual_return` leaves, not Python's intermediate
`num_years` division: when present, the override is returned without reading
the enum value and is used directly as the native annual-return exponent
factor. `<fincore/metrics.hpp>` already includes `risk.hpp`, and
`risk.hpp` already reaches `frequency`, `std::optional`, `dataframe`, and the
return constants through its accepted includes. No facade or build-system
change is needed.

This is deliberately a one-column scalar API. A multi-column or array-returning
overload would invent semantics absent from both the Python source and the
native metric surface.

## Exact composition and control flow

The Python source checks the raw observation count, computes annual return,
then historical VaR, then applies three undefined-result guards. The native
implementation preserves that visible order after its mandatory DataFrame
structural validation:

```text
reject embedded NUL in column
values = require_double_column(returns, column)
if values.size < 2:
    return NaN

factor = annualization_factor(period, annualization)
ann_ret = annual_return(returns, factor, column)
var_value = historical_value_at_risk(returns, cutoff, column)

if var_value == 0 or isnan(var_value) or isnan(ann_ret):
    return NaN
return (ann_ret - risk_free) / abs(var_value)
```

Each accepted numerical leaf is called exactly once. The implementation must
not copy compounding, annualization, percentile, or finite-input formulas, and
must not replace the direct IEEE quotient with a finite-only helper.

This order is observable. On a valid frame with at least two rows, an invalid
frequency without an override throws `input_error` before an invalid cutoff is
evaluated. An explicit annualization override makes the enum irrelevant, just
as Python ignores an invalid period string when an override is provided. On a
valid zero- or one-row double frame, the raw-length shortcut returns NaN before
period, cutoff, risk-free, or annualization values are interpreted.

## Validation and edge semantics

Before any DataFrame C-string boundary, `column.find('\0')` rejects an embedded
NUL with `fincore::input_error`. `require_double_column` then validates the
exact bounded selector, column presence, genuine-double type, and index/column
length before the raw `<2` shortcut. Thus missing, empty-selector, wrong-typed,
and short-column frames remain structural errors even when they contain fewer
than two rows. A legal bounded non-NUL-terminated `string_view` remains valid.

The public leaf policies define the remaining behavior:

- cutoff endpoints `0` and `1` are accepted, despite the Python docstring's
  informal `(0, 1)` wording;
- nonfinite or out-of-range cutoff returns NaN through public historical VaR;
- any NaN or signed-infinity return makes historical VaR NaN, so the final
  result is NaN even though annual return treats NaN as a zero return;
- zero VaR returns NaN;
- `risk_free` is not finite-validated: `+infinity` can produce `-infinity`,
  and NaN remains NaN through direct arithmetic;
- explicit annualization is delegated unchanged to `annual_return`; finite
  negative `-3` and NaN match Python for the oracle fixture, while native zero,
  positive infinity, and negative infinity deliberately differ from Python's
  `ZeroDivisionError` because the accepted native leaf has no `num_years`
  division; and
- a legitimate infinite annual return or quotient is preserved rather than
  clamped.

No DataFrame is returned, no index is transformed, and no input view or span
is retained after the call.

## Python source oracle

The mandatory Anaconda source oracle fixes the following outputs:

```text
daily_default             649.7873043614145
weekly                     22.994057986872573
explicit_7                  2.2423926838665866
bad_period_explicit_7       2.2423926838665866
cutoff_0                   -4.998700305323209
cutoff_1                   -9.997400610646418
zero_var                    nan
nan_return                  nan
pos_inf_return              nan
bad_cutoff                  nan
risk_free_pos_inf          -inf
one_row_bad_everything      nan
bad_period                  ValueError
bad_period_bad_cutoff       ValueError
annualization_0             ZeroDivisionError
annualization_neg_3         -0.8982857935175003
annualization_nan           nan
annualization_pos_inf       ZeroDivisionError
annualization_neg_inf       ZeroDivisionError
```

The finite default/weekly/override values and cutoff endpoints are Python
goldens. The native invalid-frequency exception type is `fincore::input_error`,
not Python `ValueError`. On the same four-row fixture, accepted native leaf
composition produces `0`, `-0.8982857935175003`, NaN, positive infinity, and
`-22.727272727272727` for annualization `0`, `-3`, NaN, positive infinity,
and negative infinity respectively. These values are a deliberate native-leaf
compatibility boundary rather than a new domain-validation layer.

## Mutation-resistant tests

The leaf and facade suites must lock:

- the exact six-parameter function-pointer signature and one-argument default
  call;
- daily, weekly, explicit-annualization, and both cutoff-endpoint goldens;
- direct parity with one call each to public `annualization_factor`,
  `annual_return`, and `historical_value_at_risk` over multiple finite cases;
- invalid frequency before invalid cutoff, and invalid frequency ignored by an
  explicit annualization override;
- valid multi-row annualization overrides `0`, `-3`, NaN, positive infinity,
  and negative infinity, killing zero/nonpositive/nonfinite rejection and
  finite-result-clamping mutants while documenting the Python divergence;
- zero/one raw row shortcut before all numerical parameters;
- zero VaR, invalid cutoff, NaN return, both signed-infinity returns, and NaN
  annual-return branches;
- direct `risk_free = +infinity` negative-infinity output and a finite-input
  positive-infinity quotient, proving no finite clamp;
- default/named selection, embedded-NUL rejection, legal bounded
  non-NUL-terminated selection, and structural errors before raw length; and
- visibility through `<fincore/metrics.hpp>`.

The leaf consumer includes only `<fincore/metrics/risk.hpp>` among fincore
headers. Leaf and facade compile uses are added before the production
declaration so a genuine missing-symbol compile RED is retained. No CTest
result from a failed build may be cited as RED evidence.

## Acceptance boundary

Acceptance requires current focused/related/full CTest plus fresh AppleClang
headless, Homebrew LLVM 22 ASan+UBSan with registration and diagnostics audits,
local ImGui/ImPlot/demo compile-link-only without launching a GUI, and a fresh
invalid-ImGui configuration failure matched across physical line wraps. Actual
FetchContent and local dependency provenance must be recorded.

The phase does not implement `mar_ratio`, adjusted or conditional Sharpe,
calendar-resampled consecutive statistics, EVT, provider/solver routes,
aliases, reports, or plots.
