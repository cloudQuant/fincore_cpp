# fincore C++ Phase 3A Rolling Scalar Metrics Design

## Purpose and bounded scope

Phase 3A ports the four Python rolling APIs whose results are one timestamped
numeric series and whose inputs need only the accepted one-double-column
`fincore::dataframe` contract:

- `roll_sharpe_ratio`
- `roll_max_drawdown`
- `rolling_volatility`
- `rolling_sharpe`

The phase intentionally does not port benchmark-paired rolling alpha, beta, or
capture metrics, nor the two-column alpha/beta and regression outputs. Those
families need a separate alignment and multi-column-output contract. It also
does not modify the accepted `rolling_engine`: that class remains its existing
native batch interface, while this phase adds a Python-shaped public metrics
facade without changing the engine's previously accepted semantics.

## Decision

Three approaches were considered. Exposing only `rolling_engine` would be
small, but it would not supply the Python API names, output shape, or NaN
rules. Porting all twelve Python rolling functions at once would combine paired
timestamp alignment and a two-column regression result with basic scalar
windows. The selected approach is a narrow scalar facade first. It fixes the
one-column window/index contract in public code, reuses accepted scalar metrics
where their semantics agree, and leaves paired/multi-column work for the next
phase.

## Public API and output shape

`include/fincore/metrics/rolling.hpp` will expose:

```cpp
dataframe roll_sharpe_ratio(
    const dataframe& returns,
    std::size_t window = static_cast<std::size_t>(daily_annualization),
    double risk_free = 0.0,
    frequency period = frequency::daily,
    std::optional<double> annualization = std::nullopt,
    std::string_view column = returns_column);

dataframe roll_max_drawdown(
    const dataframe& returns,
    std::size_t window = static_cast<std::size_t>(daily_annualization),
    std::string_view column = returns_column);

dataframe rolling_volatility(
    const dataframe& returns,
    std::size_t window,
    frequency period = frequency::daily,
    std::optional<double> annualization = std::nullopt,
    std::string_view column = returns_column);

dataframe rolling_sharpe(
    const dataframe& returns,
    std::size_t window,
    frequency period = frequency::daily,
    std::optional<double> annualization = std::nullopt,
    std::string_view column = returns_column);
```

Every result retains the requested input column name. `roll_*` functions emit
only complete windows and use each window's final input timestamp as the output
index. `rolling_*` functions retain the complete input index and fill the first
`window - 1` positions with `NaN`, matching pandas rolling-series shape. For a
nonempty input shorter than `window`, `roll_*` returns an empty series with that
column name and `rolling_*` returns a same-index all-`NaN` series.

## Computation and edge behavior

Each public function validates the one double column once, preserves supplied
row order, rejects `window == 0` with `input_error`, and does not sort, align,
deduplicate, or apply a global finite filter. An empty but genuine double
column is valid and produces an empty result for all four APIs, matching the
Python empty-Series shape; missing, wrong-typed, and index-length-mismatched
columns retain the established `input_error` boundary.

`roll_sharpe_ratio` materializes each complete window and delegates to the
accepted `sharpe_ratio` with `annualization_factor(period, annualization)`.
This intentionally retains its pandas-compatible `min_periods=1` behavior:
NaNs are ignored by the scalar Sharpe calculation when at least two usable
samples remain. `rolling_volatility` and `rolling_sharpe` differ: pandas
requires a full valid window for these default rolling calls, so any NaN within
a complete window emits `NaN`; otherwise they delegate to `annual_volatility`
or zero-risk-free `sharpe_ratio` with the resolved annualization factor.

`roll_max_drawdown` cannot blindly delegate to existing `max_drawdown` because
the Python rolling implementation uses NumPy cumulative products: after a NaN
return, cumulative wealth and running maximum are NaN for the rest of that
window, while `nanmin` retains the finite prefix. Its private per-window
algorithm therefore starts wealth at `1.0`, compounds raw returns, lets normal
C++ arithmetic propagate NaN/infinity, tracks the raw running maximum, and
takes the minimum non-NaN drawdown from that path. The existing scalar
`drawdown_series`/`max_drawdown` remains untouched.

## Verification boundary

Oracle tests use the local Python source through the required Anaconda command.
For UTC `2021-01-01` through `2021-01-06`, returns
`[.01, -.02, .03, 0, -.01, .02]`, and window `3`, the finite goldens include
rolling Sharpe `[4.205259864302774, 2.1026299321513866,
5.083911274417939, 3.464101615137753]`, rolling maximum drawdown
`[-.02, -.02, -.01, -.01]`, and full-index annual volatility
`[NaN, NaN, .3994996871087636, .3994996871087636,
.3304542328371662, .24248711305964288]`. Tests also pin the critical NaN
distinction: for `[.01, NaN, -.01]` at window `3`, `roll_sharpe_ratio` is
`0.0`, `roll_max_drawdown` is `0.0`, and both full-index
`rolling_*` results remain `NaN` at the completed window.

Consumer tests must use public headers only and cover short/window-one paths,
custom columns, no input reordering, frequency/custom annualization, malformed
columns, umbrella inclusion, fresh AppleClang headless CTest, Homebrew LLVM
ASan+UBSan CTest, and the retained invalid-ImGui diagnostic. Paired rolling
metrics, two-column rolling results, portfolio-wide tables, reports,
optimization/simulation, and positive ImPlot remain out of scope.
