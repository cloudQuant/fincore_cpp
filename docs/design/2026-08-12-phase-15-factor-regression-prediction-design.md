# Phase 15 Factor-Regression Prediction Design

**Date:** 2026-08-12

## Decision

Phase 15 adds a deterministic, DataFrame-native prediction consumer for the
accepted Phase 12 factor-regression result.  It is the narrow native analogue
of Python `fincore.attribution.fama_french.FamaFrenchModel.predict`: a fitted
intercept and ordered factor coefficients are applied to a new table of factor
returns.

Python keeps the fitted alpha/betas and factor selection in a mutable model
receiver.  Native code deliberately does not add that receiver.  The caller
passes the value-owning `factor_regression_result` returned by
`fit_factor_regression` plus an explicit ordered factor-name span.  That makes
the operation stateless, reproducible, and independent of a fixed
three-/five-factor vocabulary or provider state.

## Public shape

The new leaf is `include/fincore/attribution/factor_prediction.hpp` and is
later exported by `include/fincore/attribution.hpp`.

```cpp
inline constexpr std::string_view factor_prediction_column{"predicted_returns"};

dataframe factor_regression_predictions(
    const factor_regression_result& regression,
    const dataframe& factor_returns,
    std::span<const std::string_view> factor_column_names);
```

The output has the **exact raw index and row order of `factor_returns`** and
one genuine `double` column named `predicted_returns`; it has no additional
columns.  Unlike the Python `ndarray`, this preserves the supplied timestamp
association without aligning, sorting, resampling, deduplicating, or joining.
Duplicate and non-monotonic prediction indexes are valid: prediction is a
row-local transform and performs no index matching.

For each input row `i`, with selected factor names `f[0..K-1]`, the result is

```text
prediction[i] = alpha + sum(coefficients[f[j]] * factor_returns[f[j]][i])
```

where `alpha` and each beta are bound to the public coefficient table in the
same explicit caller order.  The calculation uses ordinary IEEE double
arithmetic but rejects a non-finite product, running sum, or final value with
`fincore::input_error`; it never publishes accidental infinity or NaN from an
otherwise finite request.

## Model and selector boundary

The explicit factor-name span is nonempty.  Names must be nonempty, contain no
embedded NUL, be distinct as full `std::string_view` values, and not equal the
reserved Phase 12 `alpha` label.  The leaf owns each validated selector before
passing it to a DataFrame C-string API; a bounded, non-NUL-terminated view is
therefore valid, while an embedded-NUL selector throws `input_error`.

The `factor_regression_result` is treated as an immutable fitted-model value,
even though its public fields can be manually changed.  The prediction leaf
therefore validates only the state it consumes:

- `regression.alpha` is finite and exactly agrees with coefficient row zero;
- `coefficients` has exactly two columns: genuine `std::string` `factor` and
  genuine `double` `coefficient`, both matching the coefficient index length;
- the coefficient index is ordinal `{0, ..., K}`; labels are exactly `alpha`
  followed by the caller's factor names in caller order; and every coefficient
  is finite.

`r_squared`, `fitted_returns`, and `residuals` are intentionally not read or
validated: Python prediction likewise consumes only fitted alpha/betas.  This
keeps a manually materialized but structurally valid coefficient result usable
and avoids creating a hidden dependency on historical training rows.

Selected prediction-factor columns must exist, be genuine `double`, match the
prediction frame index length, and contain only finite values.  Unselected
metadata is irrelevant.  A valid fitted result plus a selected empty factor
frame yields a typed-empty `predicted_returns` column; this mirrors a zero-row
matrix multiply and is the only empty result supported by the leaf.

All output vectors are newly materialized.  Later mutation or destruction of
the source factor table, model result, or selector storage cannot change it.

## Source oracle

The local Python source is run through the required Anaconda environment on
the existing eight-row Phase 12 MKT/SMB/HML fixture.  After
`FamaFrenchModel("3factor").fit(returns, factors)`, its `predict(factors)`
values are:

```text
0.0068203580214942685
-0.02992443386996989
0.0533598324629174
-0.015348761635518682
0.029356958651646607
-0.0227002927762671
0.05139868826161392
-0.0009623491159163553
```

The native test compares finite values with a `1e-12` tolerance and separately
proves its defined raw-index preservation, model-schema validation, and
selected-name ordering that Python's stateful receiver does not expose.

## Scope and verification

The leaf begins with a public-header-only RED test and a complete source-oracle
record.  GREEN tests prove source values, exact one-column schema, raw
non-monotonic/duplicate index retention, typed-empty output, rowwise
coefficient algebra, selector/model validation, finite arithmetic, ignored
metadata, and ownership.  A later attribution-umbrella-only C++23 consumer
proves export without a leaf/private include.

This phase excludes a stateful Fama-French model, standard-factor-set
selection, refitting, QR/OLS changes, coefficient estimation, rolling
exposures, factor attribution, residual or idiosyncratic-risk helpers,
Newey-West statistics, p-values, providers/caches, Python embedding, reports,
releases, full Python dict parity, and GUI runtime.  Fresh AppleClang,
Homebrew LLVM ASan+UBSan, ImPlot compile/link-only, and invalid-ImGui
configuration evidence are retained at acceptance; no GUI is started.
