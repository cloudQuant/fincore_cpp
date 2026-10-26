# fincore C++ Phase 4A Performance Statistics Design

## Purpose and bounded scope

Phase 4A migrates the deterministic, single-return-series part of Python
`fincore.metrics.perf_stats.perf_stats` to a native C++ public facade.  It is
the first summary-table API in the C++ project that matches the Python
performance-statistics key set rather than the deliberately smaller, legacy
`analysis_context::summary()` core summary.

The phase adds exactly two overloads of `fincore::perf_stats`: one for a
strategy return series and one for a strategy plus benchmark/factor return
series.  It does not add Python's bootstrap/distribution helpers, positions or
transactions arguments, report rendering, string-indexed DataFrames, or alter
the existing `analysis_context` or scalar metric contracts.  The Python
`positions`, `transactions`, and `turnover_denom` arguments are currently not
used by its deterministic function, and native portfolio-row schemas belong in
a later phase.

## Data representation and public API

Python returns an ordered `pd.Series` whose index is metric labels.  The native
`fincore::dataframe` intentionally has signed UTC-nanosecond timestamps as its
index, so it must not be repurposed as a synthetic string-indexed table.  The
native equivalent is a keyed `std::map<std::string, double>`, consistent with
the established `analysis_context::summary()` API.  Key lookup is the public
contract; callers must not rely on Python insertion order or map iteration
order.

`include/fincore/metrics/perf_stats.hpp` will declare:

```cpp
std::map<std::string, double> perf_stats(
    const dataframe& returns,
    frequency period = frequency::daily,
    std::string_view returns_column_name = returns_column);

std::map<std::string, double> perf_stats(
    const dataframe& returns,
    const dataframe& benchmark,
    frequency period = frequency::daily,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);
```

The no-benchmark overload returns exactly these 13 keys:

```
Annual return             Cumulative returns       Annual volatility
Sharpe ratio              Calmar ratio              Stability
Max drawdown              Omega ratio               Sortino ratio
Skew                      Kurtosis                  Tail ratio
Daily value at risk
```

The benchmark overload returns the same 13 keys plus `Alpha` and `Beta`.  It
uses native `benchmark` terminology while mapping to Python's
`factor_returns` argument.

## Computation, period, and error contract

Each entry validates its requested named double column before resolving
`period`; the benchmark overload validates both named columns.  Missing,
empty-named, wrong-typed, and index/column length-mismatched inputs therefore
raise the established `input_error`.  A valid frame with an unknown
`frequency` raises `input_error` from `annualization_factor`.  Empty genuine
double columns are structurally valid and return the complete key set; values
are the existing scalar delegates' normal NaN/defined results.

The implementation resolves `annualization_factor(period)` exactly once and
only composes accepted public scalar functions:

| Key(s) | Delegate / factor |
| --- | --- |
| Annual return, Annual volatility | `annual_return`, `annual_volatility`, requested factor |
| Cumulative returns | `cumulative_return` |
| Sharpe ratio | `sharpe_ratio(..., 0.0, requested factor)` |
| Calmar ratio | `calmar_ratio(..., 0.0, requested factor)` |
| Stability, Max drawdown | `stability_of_timeseries`, `max_drawdown` |
| Omega ratio | `omega_ratio(..., 0.0, 0.0, daily_annualization)` |
| Sortino ratio | `sortino_ratio(..., 0.0, requested factor)` |
| Skew, Kurtosis | `skewness`, `kurtosis` |
| Tail ratio | `tail_ratio` |
| Daily value at risk | `historical_value_at_risk(..., 0.05)` |
| Alpha, Beta | one `alpha_beta(..., 0.0, daily_annualization)` call |

The table deliberately preserves Python's quirks: `perf_stats` passes its
`period` to annual return/volatility, Sharpe, Calmar, and Sortino, but invokes
`alpha_beta` and `omega_ratio` with their independent default behavior.
Consequently Alpha remains daily-annualized when `period` is weekly, and Beta
is period-independent.  No new filtering or numerical formula is introduced;
NaN and infinity handling remains owned by each accepted scalar delegate.  For
benchmark input, the alpha/beta delegate retains the project-wide
common-timestamp, left-order alignment contract rather than reproducing
Python's occasional outer-shaped intermediate.

## Oracle and verification boundary

The local Anaconda oracle uses eight finite UTC business-day observations:

```
returns   = [.01, -.02, .015, 0, .03, -.01, .005, .02]
benchmark = [.004, -.01, .012, -.003, .02, -.008, .004, .014]
```

For daily period, selected exact goldens are `Annual return =
3.6713515622970077`, `Cumulative returns = 0.05015195340208978`, `Annual
volatility = 0.2571964229922337`, `Sharpe ratio = 6.123724356957944`,
`Calmar ratio = 183.56757811485073`, `Stability = 0.7123343398383044`,
`Skew = -0.2120878539880257`, `Kurtosis = -0.8532555879494659`, `Tail ratio =
1.6060606060606057`, `Daily value at risk = -0.0165`, `Alpha =
0.0417341829666007`, and `Beta = 1.4758151753979294`.  With weekly period,
`Annual return = 0.3744816290191584`, `Annual volatility =
0.11683321445547923`, `Sharpe ratio = 2.7817432013209333`, `Calmar ratio =
18.724081450957954`, `Sortino ratio = 5.70087712549569`, while Alpha and Beta
remain the daily values above.

Tests also assert the exact no-factor/factor key sets, custom column names,
wrong/missing/empty names and malformed columns, unknown frequency, empty
genuine double input, mismatched benchmark timestamps, and NaN/infinity
fixtures compared entry-by-entry to the public scalar delegates.  The public
metric umbrella consumes only `<fincore/metrics.hpp>`.  Final acceptance uses
fresh AppleClang headless CTest, Homebrew LLVM ASan+UBSan CTest, and the
existing ImPlot default-path/headless behavior without launching a GUI.
