# fincore C++ Phase 2B Calendar Metrics Design

## Purpose and scope

Phase 2B adds calendar-aware metric operations to the accepted C++23,
DataFrame-native core. It closes the most immediate gap between the existing
scalar metrics and the Python reference's `yearly.py` / calendar-grouped
`alpha_beta.py` APIs without changing the public tabular representation:
every time series remains a one-column
`hmdf::StdDataFrame<std::int64_t>`.

The foundation design already defines `fincore::timestamp` as signed UTC Unix
nanoseconds. This phase makes that contract executable using C++20/23
`std::chrono` calendar arithmetic; it does **not** replace the alias with
`hmdf::DateTime`, add a second DataFrame type, depend on a time-zone database,
or reinterpret existing integer indexes. The local Python implementation at
`/Users/yunjinqi/Documents/new_projects/fincore` remains the oracle.

The phase is intentionally limited to calendar years. Weekly, monthly, and
quarterly aggregation require a separately specified native composite-key
output convention. Deep drawdown tables, consecutive runs, positions,
transactions, round trips, Bayesian/bootstrap metrics, attribution, reporting,
optimization, simulation, and a comprehensive facade remain later work.

## UTC timestamp boundary

Add `fincore/calendar.hpp` with the following thin public construction and
inspection helpers:

```cpp
timestamp utc_timestamp(int year, unsigned month = 1, unsigned day = 1,
                        unsigned hour = 0, unsigned minute = 0,
                        unsigned second = 0, unsigned nanosecond = 0);
int utc_year(timestamp value) noexcept;
```

`utc_timestamp` interprets all components in UTC and returns the exact signed
nanoseconds since `1970-01-01T00:00:00Z`. It throws `input_error` for an
invalid civil date/time or a representational overflow. `utc_year` obtains the
UTC civil year by flooring an epoch-nanosecond timestamp to a `sys_days` value;
it does not consult host local time or DST. Negative/pre-epoch timestamps are
valid. Internally, a non-public `year_start_timestamp(year)` makes output
indexes at `YYYY-01-01T00:00:00Z`.

This gives callers a safe way to build calendar-aware DataFrames while keeping
the existing raw timestamp API fully compatible. It also means current
non-calendar metrics still treat indexes solely as sortable alignment keys.

## Native yearly-result representation

Python returns a `Series` indexed by integer calendar year. The native result
is a one-column `dataframe`, whose index is that year's UTC start timestamp:

| Python year key | Native index |
| --- | --- |
| `2019` | `2019-01-01T00:00:00Z` in Unix nanoseconds |
| `2020` | `2020-01-01T00:00:00Z` in Unix nanoseconds |

The output is ordered by increasing calendar year. Input observations inside a
year retain their original frame order, matching pandas `groupby` behavior for
an otherwise unsorted `DatetimeIndex`; no metric silently reorders a return
path. Empty input produces an empty, correctly typed DataFrame with the fixed
result-column name.

The result columns are `annual_return`, `annual_volatility`, `sharpe_ratio`,
`max_drawdown`, `annual_active_return`, `information_ratio`, `alpha`, and
`beta`. These names are part of the native DataFrame contract, rather than a
runtime-dynamic Series name.

## Yearly metric surface

Add `fincore/metrics/yearly.hpp`:

```cpp
dataframe annual_return_by_year(
    const dataframe&, frequency period = frequency::daily,
    std::optional<double> annualization = std::nullopt,
    std::string_view column = returns_column);
dataframe annual_volatility_by_year(
    const dataframe&, frequency period = frequency::daily,
    std::optional<double> annualization = std::nullopt,
    std::string_view column = returns_column);
dataframe sharpe_ratio_by_year(
    const dataframe&, double risk_free = 0.0,
    frequency period = frequency::daily,
    std::optional<double> annualization = std::nullopt,
    std::string_view column = returns_column);
dataframe max_drawdown_by_year(
    const dataframe&, std::string_view column = returns_column);
double annual_active_return(
    const dataframe&, const dataframe&, frequency period = frequency::daily,
    std::optional<double> annualization = std::nullopt,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);
dataframe annual_active_return_by_year(
    const dataframe&, const dataframe&, frequency period = frequency::daily,
    std::optional<double> annualization = std::nullopt,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);
dataframe information_ratio_by_year(
    const dataframe&, const dataframe&, frequency period = frequency::daily,
    std::optional<double> annualization = std::nullopt,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);
```

The public `frequency` plus optional annualization maps directly to Python's
`period` / optional `annualization`: `annualization_factor` resolves the
effective finite positive value once and each group delegates to accepted
public scalar metric functions. This preserves their established `NaN`,
signed-infinity, and column-validation behavior.

For paired APIs, first call `align_on_common_index`, then group the aligned
frames by the UTC year of their common timestamp. No API pairs by position and
no group is made from an unmatched observation. `annual_active_return` first
aligns and applies `annual_return(strategy) - annual_return(benchmark)` to
the complete aligned path. Per-year active and information results use the
same alignment before grouping.

## Annual alpha and beta

Extend `fincore/metrics/alpha_beta.hpp` with native `dataframe`
`annual_alpha` and `annual_beta` APIs. They use the same year-start index and
`alpha` / `beta` result columns. Both call `align_on_common_index` before
year grouping and then delegate each group to the existing public scalar
functions. `annual_alpha` accepts the frequency/optional annualization needed
by alpha; `annual_beta` retains Python-compatible risk-free, frequency and
optional annualization arguments even though beta is scale-independent and the
last two do not change its formula.

```cpp
dataframe annual_alpha(
    const dataframe&, const dataframe&, double risk_free = 0.0,
    frequency period = frequency::daily,
    std::optional<double> annualization = std::nullopt,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);
dataframe annual_beta(
    const dataframe&, const dataframe&, double risk_free = 0.0,
    frequency period = frequency::daily,
    std::optional<double> annualization = std::nullopt,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);
```

All paired calls retain the existing finite-pair behavior inside `alpha` and
`beta`. In particular, year partitioning happens before the scalar calculation
so non-finite pairs from one year cannot affect another year.

## Error handling and oracle evidence

- Missing, wrongly typed, malformed, or empty column names retain
  `input_error` behavior from `require_double_column` and alignment helpers.
- An unrecognized `frequency` and invalid date components retain
  `annualization_factor` / `utc_timestamp` structural validation. An explicit
  annualization is intentionally passed through exactly as the existing native
  and Python `annualization_factor` contracts do; the delegated scalar metric
  determines its numerical result (for example, information ratio returns
  `NaN` for non-finite or non-positive annualization). No new global validation
  rule is inferred.
- Numerical underdetermination within a group is delegated to the accepted
  scalar metric and remains `NaN` or intentionally signed infinity exactly as
  that metric defines.
- Date grouping and annual golden values are generated with the required
  Anaconda command from the local Python checkout, recorded as literals in
  focused C++ tests, and include a pre-epoch conversion case, cross-year
  returns, unmatched paired timestamps, and non-finite paired samples.

## Acceptance boundary

Phase 2B is accepted only after a public-headers compilation test, focused
calendar/yearly/annual-alpha-beta tests, a fresh headless C++23 CTest run, a
fresh Homebrew LLVM ASan+UBSan run, and a scope review. The optional ImPlot
positive build remains an independent open gate until a genuine local Dear
ImGui checkout is supplied. Completing this phase must not be represented as
full fincore parity or full project completion.
