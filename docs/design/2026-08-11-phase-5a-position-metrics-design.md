# fincore C++ Phase 5A Position Metrics Design

## Purpose and bounded scope

Phase 5A introduces the numeric, timestamp-indexed core of Python
fincore.metrics.positions as DataFrame-native C++ APIs: get_percent_alloc,
get_long_short_pos, get_max_median_position_concentration, and gross_lev.
These functions are a useful next step after multi-factor rolling metrics
because their natural input and output is a real multi-column
hmdf::StdDataFrame<fincore::timestamp>.

The phase deliberately excludes string/SID row schemas and MultiIndex output:
extract_pos, stack_positions, top-holdings ranking, sector/style/cap/volume
mappings, transactions, liquidation, round trips, portfolio attribution,
reports, and external data stay for later slices. No generic pandas
compatibility type, automatic string-column conversion, or change to existing
scalar metrics is introduced.

## Input discovery and public API

A native positions frame has a timestamp index and one or more genuine double
data columns. Every double column is a numeric position field; their
deterministic cloudQuant/DataFrame insertion order is preserved. Non-double
metadata columns are ignored, because they cannot participate in currency
arithmetic. Every selected double column must match the index length; no
double columns is an input_error.

The new public header declares:

~~~cpp
inline constexpr std::string_view cash_column{"cash"};

dataframe get_percent_alloc(const dataframe& positions);

dataframe get_long_short_pos(
    const dataframe& positions,
    std::string_view cash_column_name = cash_column);

dataframe get_max_median_position_concentration(
    const dataframe& positions,
    std::string_view cash_column_name = cash_column);

dataframe gross_lev(
    const dataframe& positions,
    std::string_view cash_column_name = cash_column);
~~~

The single-factor-like native functions return standard DataFrames:

- get_percent_alloc returns the same timestamp index and the same discovered
  double columns in the same order. Each row is divided by that row's sum.
- get_long_short_pos returns one DataFrame with columns "long" and "short":
  positive holdings summed per row and absolute negative holdings summed per
  row. This combines Python's pair of same-index Series into the native
  multi-column table without a synthetic container. If the named cash column
  exists it is excluded; absence is allowed.
- gross_lev returns a one-column "gross_lev" DataFrame. It requires a genuine
  named cash double column, excludes it from absolute exposure, divides by the
  sum of all numeric position columns, and replaces infinite results with NaN.
- get_max_median_position_concentration also requires cash and returns the
  exact ordered double schema "max_long", "median_long", "median_short",
  "max_short". It computes allocation first and excludes cash before row-wise
  positive/negative statistics.

## Arithmetic, NaN, and structural rules

The implementation mirrors the relevant pandas row reductions rather than
adding a global finite filter. Row sums skip NaN values; all-NaN numeric rows
therefore have a zero row sum. Ordinary IEEE arithmetic then applies.
Allocation and gross-leverage results replace positive or negative infinity
with NaN, while NaN stays NaN. Long/short sums treat NaN holdings as zero
through the same where(..., 0).sum(axis=1) behavior; signed infinity remains
ordinary arithmetic. Concentration ignores NaN allocations when finding
extrema and medians; an empty positive or negative side is NaN.

Every cash name must be nonempty. gross_lev and concentration validate that
the requested cash field exists, is double, and matches the index before
calculation. get_long_short_pos validates its nonempty cash-name argument but
does not require the column to exist. Valid zero-row position frames retain
their expected double output schema. Missing, wrong-typed, malformed, or
otherwise structurally invalid required inputs throw input_error.

The source uses private helpers in positions.cpp to enumerate and validate the
frame's double columns and to load an ordered multi-column result. It does not
broaden dataframe.hpp, modify DataFrame upstream, or use private cloudQuant
storage.

## Oracle and verification boundary

The local Anaconda oracle fixture is:

~~~text
index = [10, 20, 30]
A     = [10, -5, NaN]
B     = [-5, -2, 3]
cash  = [95, 100, 0]
~~~

Allocation columns are A = [.1, -.053763440860215055, NaN],
B = [-.05, -.021505376344086023, 1], and
cash = [.95, 1.075268817204301, 0]. Long/short values are
long = [10, 0, 3] and short = [5, 7, 0]; gross leverage is
[.15, .07526881720430108, 1]. Concentration columns are:

~~~text
max_long     = [.1, NaN, 1]
median_long  = [.1, NaN, 1]
median_short = [-.05, -.03763440860215054, NaN]
max_short    = [-.05, -.053763440860215055, NaN]
~~~

The zero-net-value fixture A=[10,10], cash=[-10,0] produces allocation
A=[NaN,1], cash=[NaN,0], gross leverage [NaN,1], and all-NaN concentration
values on the first row. Tests also cover deterministic column order, custom
cash names, absent cash behavior for long/short only, empty typed outputs,
non-double metadata exclusion, no numeric positions, malformed numeric
columns, and public-umbrella consumption. Final acceptance retains the
existing non-GUI ImPlot compile boundary and makes no interactive visual
claim.
