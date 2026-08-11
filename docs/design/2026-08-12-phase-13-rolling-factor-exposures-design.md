# Phase 13 Rolling Factor Exposures Design

**Date:** 2026-08-12

## Decision

Phase 13 adds the deterministic, DataFrame-native rolling-exposure slice of
Python `fincore.attribution.fama_french.FamaFrenchModel.get_factor_exposures`.
It builds on the accepted Phase 12 factor-regression core rather than adding a
stateful Fama-French model, a provider, or another least-squares solver.

The native surface accepts an explicit return column, an explicit ordered
factor-name span, and either an expanding history or a positive fixed trailing
window.  It returns one typed DataFrame over the complete supplied return
index: `alpha` and each requested factor have a `NaN` prefix until the
intercept-plus-factor window has enough observations; every eligible row is
then obtained by a direct call to the public `fit_factor_regression` leaf.

This keeps the useful rolling behavior of the Python source while replacing its
positional pandas slices and stateful method receiver with an explicit,
strictly validated native boundary.

## Public shape

The leaf header is `include/fincore/attribution/factor_exposures.hpp`; the
existing `include/fincore/attribution.hpp` umbrella will later export it next
to Brinson and factor regression.

```cpp
dataframe rolling_factor_exposures(
    const dataframe& returns,
    const dataframe& factor_returns,
    std::span<const std::string_view> factor_column_names,
    std::optional<std::size_t> rolling_window = std::nullopt,
    std::string_view returns_column_name = returns_column);
```

The result uses the raw input return index, including nonmonotonic order, and
has genuine `double` columns in exact order: `alpha` first, followed by the
caller-supplied factor names.  `alpha` is the accepted
`factor_regression_alpha_name`; no duplicate constant is introduced.  A
typed-empty valid input produces the same typed-empty output schema.

## Window and validation contract

Both frames must have duplicate-free raw timestamp indexes that are exactly
equal in length, value, and order.  The explicit factor span has the same
nonempty/no-empty/no-duplicate/no-reserved-`alpha` contract as Phase 12.
Selected return and factor columns must exist, be genuine `double`, have exact
owning index lengths, and consist entirely of finite values.  Unselected,
including malformed, metadata is ignored.  These whole-frame validations occur
before output construction; the operation never drops, aligns, sorts,
deduplicates, or otherwise repairs observations.

`rolling_window == std::nullopt` means expanding history: row `t` uses raw
rows `[0, t]`.  A provided positive `rolling_window` means a trailing window:
row `t` uses `[max(0, t + 1 - window), t]`.  A zero window throws
`fincore::input_error`; a positive window shorter than `K + 1` is valid but
has no eligible fit and therefore yields an all-`NaN` typed output.  A row is
eligible exactly when its selected window has at least `K + 1` observations.

For every eligible row, the implementation materializes only that selected
return/factor slice in raw order and delegates to public
`fit_factor_regression`.  It maps coefficient row zero to `alpha`, then maps
the remaining rows to supplied factor columns in order.  An eligible
rank-deficient window throws `input_error` under Phase 12's strict QR contract;
the native API does not silently use Python/NumPy's minimum-norm solution.
`NaN` is output only for the ordinary insufficient-history prefix, never for a
validated eligible window.

## Source oracle and scope

The actual local Python source was run through the required Anaconda command
on the existing eight-row MKT/SMB/HML fixture.  It emits three leading `NaN`
rows for three factors, then, for example, at row four alpha
`0.002333333333333203`, MKT `1.483333333333333`, SMB
`-0.5333333333333329`, and HML `0.13333333333333303`.  Its final expanding row
matches the Phase 12 whole-sample coefficients, while `rolling_window=4`
produces a distinct final alpha `0.0008977272727272866`, MKT
`1.4931818181818164`, SMB `-0.4850000000000014`, HML
`0.23818181818181863`.  The phase will retain the complete command and all
values as test evidence.

This slice deliberately excludes a model class, Python's `newey_west_lags`,
standard errors/p-values, risk-free adjustment, factor provider/cache, factor
prediction, attribution decomposition, idiosyncratic risk, generic rolling
metric changes, external numerical dependencies, reports/release work, full
Python parity, and GUI runtime.  It neither changes Phase 12's solver nor
weakens its strict input/rank semantics.

## Verification

The leaf begins with a public-header-only RED test and exact source-derived
Anaconda goldens for expanding and fixed windows.  Tests prove schema/order,
raw-index retention (including successful common nonmonotonic order), NaN
prefix placement, direct equality against independently materialized public
`fit_factor_regression` windows, typed-empty/short-window behavior, strict
names/types/index/finite/rank/window errors, ignored metadata, and output
ownership.  A later umbrella-only C++23 consumer proves public attribution
export.  Acceptance retains fresh AppleClang, LLVM ASan+UBSan, ImPlot
compile/link-only, and invalid-ImGui gates without starting a GUI.
