# fincore C++ Phase 4B Multi-column Rolling Metrics Design

## Purpose and bounded scope

Phase 4B completes the remaining public metric functions in Python
`fincore.metrics.rolling`: `roll_alpha_beta`, `rolling_beta`, and
`rolling_regression`. The phase makes the two-result rolling APIs real
`fincore::dataframe` values and adds the native replacement for Python's
DataFrame-of-factors `rolling_beta` branch.

This is a narrow metric-layer migration. It does not change
`rolling_engine`, `analysis_context`, existing scalar metric contracts,
timestamp representation, ImPlot rendering, portfolio tables, reports, or
Python's ndarray facade. The C++ public surface keeps the established named
double-column convention: a `fincore::dataframe` is a
`hmdf::StdDataFrame<fincore::timestamp>`, not a dynamically typed pandas
object.

## Public API and data shape

`include/fincore/metrics/rolling.hpp` will expose the following additions:

```cpp
dataframe roll_alpha_beta(
    const dataframe& returns,
    const dataframe& benchmark,
    std::size_t window = 252U,
    double risk_free = 0.0,
    frequency period = frequency::daily,
    std::optional<double> annualization = std::nullopt,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);

dataframe rolling_beta(
    const dataframe& returns,
    const dataframe& factor_returns,
    std::size_t rolling_window = 126U,
    std::string_view returns_column_name = returns_column,
    std::string_view factor_column_name = benchmark_column);

dataframe rolling_beta(
    const dataframe& returns,
    const dataframe& factor_returns,
    std::span<const std::string_view> factor_column_names,
    std::size_t rolling_window = 126U,
    std::string_view returns_column_name = returns_column);

dataframe rolling_regression(
    const dataframe& returns,
    const dataframe& benchmark,
    std::size_t rolling_window = 126U,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);
```

The single-factor `rolling_beta` overload is the ergonomic counterpart to the
existing paired functions. It returns one full-index double column named by
the requested returns column, consistent with `rolling_volatility`,
`rolling_sharpe`, and `roll_beta`. The multi-factor overload takes an
explicit, non-empty ordered span of factor column names and returns one
full-index double column for each requested factor, preserving that supplied
order and each factor name. It deliberately does not infer every numeric
column in a heterogeneous DataFrame: accidental numeric metadata must not
become a market factor merely because it is `double`.

Both two-column functions return precisely the double columns `"alpha"` and
`"beta"`, in that load order. `roll_alpha_beta` contains one row at each
complete aligned-window endpoint. `rolling_regression` is compact: it keeps
only complete endpoints whose calculated alpha and beta are not NaN. A short
but structurally valid input therefore returns a typed zero-row two-column
DataFrame for both functions. `rolling_beta` instead retains every aligned
timestamp and writes leading/invalid windows as NaN.

## Alignment, numerical ownership, and errors

All three operations first validate their named double columns, then align
each paired input through the accepted `align_on_common_index` helper. The
native contract retains only common timestamps in the left/returns frame's
supplied order. This intentionally differs from the Python `aligned_series`
Series/Series branch, which may form an outer-shaped intermediate before
rolling; that behavior has already been excluded from native paired metric
contracts. Multi-factor beta aligns every explicitly selected factor against
the same returns frame; valid selected columns share their factor frame index,
so each result has the same native aligned index.

`roll_alpha_beta` resolves `annualization_factor(period, annualization)`
once and delegates every complete pair window to the accepted public
`alpha_beta`. It owns no new alpha/beta formula or finite-value filter.

`rolling_beta` and `rolling_regression` mirror Python's fixed-width pandas
rolling covariance/variance behavior rather than delegating to scalar
`beta`, whose established contract drops NaN pairs. A window is valid only
when every returns/factor pair is finite; a zero factor variance produces NaN.
The shared private window calculation uses the same arithmetic beta ratio as
sample covariance divided by sample variance, then derives the regression
intercept as `mean(returns) - beta * mean(factor)`. The beta ratio is
invariant to the covariance/variance denominator choice. The helper is local
to `rolling.cpp` and does not broaden the scalar API.

`rolling_window == 0`, empty or duplicate factor-name lists, empty factor
names, missing columns, non-double columns, and index/column length mismatch
raise `input_error`. Non-selected heterogeneous columns are outside the
explicit multi-factor contract. No new global non-finite policy is created.

## Python oracle and test boundary

The local Anaconda oracle uses the accepted six-observation finite fixture:

```text
index     = [10, 20, 30, 40, 50, 60]
returns   = [.01, -.02, .03, 0, -.01, .02]
benchmark = [.005, -.01, .02, .01, -.02, .01]
window    = 3
```

For `roll_alpha_beta`, endpoints `[30, 40, 50, 60]` have alpha values
`[-.3431833620809164, -.8357653939371649, 1.6310200676280466,
1.313133510455331]` and beta values `[1.6666666666666667,
1.5714285714285716, .846153846153846, .6666666666666666]`.
`rolling_beta` retains `[10, 20, 30, 40, 50, 60]` with leading NaN/NaN and
beta values `[1.6666666666666659, 1.571428571428571,
.8461538461538458, .6666666666666666]`. `rolling_regression` uses the
endpoint index and has alpha `[-.0016666666666666644,
-.007142857142857138, .0038461538461538455, .0033333333333333322]` with the
same rolling beta values. A two-column `{"market": benchmark,
"inverse": -benchmark}` factor frame must return both full-index beta
columns with exact opposite finite values.

Tests must additionally prove native left-order common-index alignment,
short/no-common typed shapes, custom names, window-zero rejection, duplicate
or malformed selected factor columns, constant-factor and NaN/infinity
windows, and public-umbrella consumption. The final acceptance remains
compile/test-only for ImPlot; no GUI runtime claim is introduced.
