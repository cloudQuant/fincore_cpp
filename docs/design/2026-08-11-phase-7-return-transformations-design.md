# fincore C++ Phase 7 Return Transformations and Calendar Aggregation Design

## Purpose and bounded scope

Phase 7 closes the remaining deterministic return-transformation gap between
the Python `fincore.metrics.returns` module and the existing native
DataFrame-boundary API. It adds Python-compatible nonzero starting-value
forms for cumulative returns, a native `normalize` operation, and a calendar
aggregation result that represents Python's MultiIndex labels explicitly in a
cloudQuant DataFrame.

This phase does not add rolling aggregation, benchmark alignment, positions,
transactions, round trips, bootstrap or Bayesian statistics, optimization,
simulation, reports/tearsheets, data providers, packaging/release, or a GUI
runtime. It also does not mutate the accepted behavior of the existing
zero-starting-value cumulative-return overloads.

## Public API and compatibility

`fincore/metrics/returns.hpp` keeps all current declarations and adds the
following surface:

~~~cpp
inline constexpr std::string_view return_aggregation_year_column{"year"};
inline constexpr std::string_view return_aggregation_period_column{"period"};

enum class return_aggregation_period {
    weekly,
    monthly,
    quarterly,
    yearly,
};

dataframe cumulative_returns(
    const dataframe& returns,
    double starting_value,
    std::string_view column = returns_column);

double cumulative_return(
    const dataframe& returns,
    double starting_value,
    std::string_view column = returns_column);

dataframe normalize(
    const dataframe& values,
    double starting_value = 1.0,
    std::string_view column = returns_column);

dataframe aggregate_returns(
    const dataframe& returns,
    return_aggregation_period period = return_aggregation_period::monthly,
    std::string_view column = returns_column);
~~~

The old `cumulative_returns(const dataframe&, std::string_view)` and
`cumulative_return(const dataframe&, std::string_view)` overloads remain
unchanged and retain their zero-starting-value behavior. The numeric overloads
are intentionally separate so existing calls with a column name cannot change
meaning or become ambiguous.

`metrics.hpp` already includes `returns.hpp`; no umbrella-header change is
needed. The phase uses no new dependency and no ImPlot API.

## Cumulative and normalization semantics

For a nonempty return series, compounding uses the established Python rule:

~~~text
wealth_0 = 1
wealth_i = wealth_(i-1) * (1 + (isnan(return_i) ? 0 : return_i))
~~~

With `starting_value == 0.0`, `cumulative_returns` emits `wealth_i - 1` and
`cumulative_return` emits final `wealth - 1`. With any nonzero starting value,
they emit `wealth_i * starting_value` and final `wealth * starting_value`,
respectively. Empty scalar cumulative output remains NaN; an empty frame
returns a typed empty frame. The existing cumulative-frame output name remains
`returns_column`, preserving the accepted API contract.

`normalize` validates the requested genuine-double, index-length-matched
column and emits a same-index, same-named single-double-column DataFrame. For
nonempty input it calculates:

~~~text
normalized_i = starting_value * values_i / values_0
~~~

If the first value is exactly positive or negative zero, all output values are
NaN, matching Python's documented zero-base outcome without introducing a
native warning channel. Empty input produces a typed empty output. Other NaN
and infinity arithmetic is ordinary IEEE arithmetic; it is not finite-filtered
or globally sanitized.

## Calendar aggregation DataFrame representation

Python `aggregate_returns` returns a Series indexed by either a one-level year
label or a MultiIndex `(calendar year, period)`. `StdDataFrame<timestamp>`
cannot expose a pandas MultiIndex directly, so native aggregation returns a
three-column DataFrame:

| Column | Type | Meaning |
| --- | --- | --- |
| `year` | `int` | Python calendar-year group label |
| `period` | `int` | ISO week, month, quarter, or `1` for yearly aggregation |
| caller `column` | `double` | Compounded group return |

The index is a canonical UTC timestamp for the start of the group represented
by that label. Monthly, quarterly, and yearly rows use the first day of the
month, quarter, and year. Weekly rows use the start of the actual ISO week,
clamped to January 1 of the *calendar-year label*. This makes the two Python
groups `(2020, 53)` and `(2021, 53)` distinct at a year boundary while keeping
the index a real timestamp. Results are ordered lexicographically by the
explicit `(year, period)` label, matching pandas `groupby` default ordering;
the weekly timestamp may therefore not be globally monotonic at an ISO
year-boundary edge.

The input column may not be named `year` or `period`, because those names are
reserved for the output label columns. Empty input emits an empty DataFrame
with all three typed columns. An invalid enum value raises `input_error`.

Input timestamps are interpreted as UTC. Unlike metrics requiring a daily
series, this grouping operation permits unordered, duplicate, and intraday
timestamps because Python `groupby` permits them. Group compounding consumes
observations in original frame order; groups are emitted in sorted label order.
NaN return observations contribute zero only to their group's wealth path;
infinities use ordinary IEEE arithmetic.

Weekly labels intentionally mirror Python exactly: the first key is the
calendar year of the observation and the second is the ISO week number. It is
not an ISO-year/week pair.

## Verification and implementation boundary

Anaconda-base oracle fixtures will cover nonzero/negative starting values,
NaN and infinity boundaries, zero normalization bases, custom column names,
typed empties, all four aggregation periods, leap-year/quarter boundaries,
and the weekly calendar-year/ISO-week boundary. Public-header RED tests will
first prove the declarations are absent, then public-only umbrella use will
prove the accepted facade and complete schema.

Implementation stays in `returns.hpp` and `returns.cpp`, using private C++23
chrono helpers for UTC labels. It must not change `calendar.hpp`, DataFrame
aliases, basic frequency/annualization semantics, or accepted slippage and
yearly metric behavior. Final acceptance will rerun fresh AppleClang core,
Homebrew LLVM ASan+UBSan core, default ImGui/ImPlot/demo compile/link, and the
invalid-ImGui negative configure gate without launching a GUI.
