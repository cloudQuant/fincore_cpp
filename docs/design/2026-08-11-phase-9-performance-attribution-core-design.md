# fincore C++ Phase 9 Performance Attribution Core Design

## Purpose and bounded scope

Phase 9 ports the deterministic, table-oriented core of Python
`fincore.metrics.perf_attrib` to the accepted C++23 and cloudQuant/DataFrame
foundation.  It makes factor exposure and return attribution a native
DataFrame operation: wide daily positions and factor returns are combined with
long-form per-asset factor loadings to produce two typed output tables and a
native summary record.

`StdDataFrame<timestamp>` has one concrete index type.  Python's source uses a
`(datetime, ticker)` MultiIndex for positions and factor loadings, so the C++
boundary must not pretend that a timestamp index is a MultiIndex.  The native
model instead stores the date in the normal DataFrame index and the ticker in
one explicit `std::string` column.  This keeps all calculation data in the
local cloudQuant/DataFrame dependency, has no pandas/Python runtime at
execution time, and makes table schemas inspectable by a normal C++ caller.

This phase intentionally covers only deterministic attribution.  It does not
add Python's warning-producing `align_and_warn` wrapper, transaction-turnover
diagnostics, market-data joins, factor-provider adapters, Brinson/style
attribution, Bayesian/bootstrap simulation, reporting/tearsheets, packaging,
or a GUI runtime.  Those require separate contracts and remain later native
slices.  The existing ImPlot adapter remains a compile/link integration and is
not changed by this numeric phase.

## Native table model

All dates are raw `fincore::timestamp` values; Phase 9 neither floors them to
UTC days nor sorts/reindexes them.  `returns`, `positions`, and
`factor_returns` must carry the same non-duplicated index sequence in the same
order.  The returned exposure and attribution tables preserve that exact
returns index.

| Input | Native schema | Meaning |
| --- | --- | --- |
| `returns` | one selected genuine `double` column, timestamp index | non-cumulative strategy returns |
| `positions` | timestamp index, one or more genuine `double` asset columns, optional genuine `double` cash column | daily holdings or daily weights; asset column names are symbols |
| `factor_returns` | timestamp index, one or more genuine `double` columns | daily factor return columns; their ordered names define the factor set |
| `factor_loadings` | timestamp index, genuine `std::string` `symbol` column, genuine `double` columns named for every factor | long-form loading rows keyed by `(timestamp, symbol)` |

The native default names are:

~~~cpp
inline constexpr std::string_view attribution_symbol_column{"symbol"};
inline constexpr std::string_view attribution_total_returns_column{"total_returns"};
inline constexpr std::string_view attribution_common_returns_column{"common_returns"};
inline constexpr std::string_view attribution_specific_returns_column{"specific_returns"};
inline constexpr std::string_view attribution_tilt_returns_column{"tilt_returns"};
inline constexpr std::string_view attribution_timing_returns_column{"timing_returns"};
inline constexpr std::string_view attribution_factor_column{"factor"};
~~~

`cash` is optional, but if a column with the chosen cash name exists it must be
a genuine `double` column with the index length.  It is excluded from exposure
calculation.  Every other genuine `double` position column is an asset and
must have a loading row for each date.  Non-double position metadata is
ignored.  All genuine `double` factor-return columns are factors; factor names
must not be empty, duplicate the loadings-symbol name, or collide with the five
reserved attribution-result columns.  Each factor must occur as a genuine
`double` factor-loading column.  Extra loading columns are ignored.

Each `(timestamp, symbol)` loading key must be unique.  A missing required key,
a malformed column length, a wrong requested type, a duplicate key, an empty
required name, an absent factor, or unequal/misaligned date indexes is an
`fincore::input_error`.  This is intentionally stricter than the deferred
Python warning-and-drop layer: Phase 9 never silently removes an asset or
date.

## Public API

The new `fincore/metrics/perf_attrib.hpp` public header will define two
value-owning result records and the following operations:

~~~cpp
struct perf_attribution_result {
    dataframe risk_exposures;
    dataframe attribution;
};

struct perf_attribution_statistics {
    std::map<std::string, double> summary;
    dataframe factor_summary;
};

dataframe normalize_attribution_positions(
    const dataframe& positions,
    bool positions_in_dollars = true,
    std::string_view cash_column_name = cash_column);

dataframe compute_exposures(
    const dataframe& positions,
    const dataframe& factor_loadings,
    std::span<const std::string_view> factor_column_names,
    std::string_view cash_column_name = cash_column,
    std::string_view symbol_column_name = attribution_symbol_column);

perf_attribution_result perf_attrib_core(
    const dataframe& returns,
    const dataframe& normalized_positions,
    const dataframe& factor_returns,
    const dataframe& factor_loadings,
    std::string_view returns_column_name = returns_column,
    std::string_view cash_column_name = cash_column,
    std::string_view symbol_column_name = attribution_symbol_column);

perf_attribution_result perf_attrib(
    const dataframe& returns,
    const dataframe& positions,
    const dataframe& factor_returns,
    const dataframe& factor_loadings,
    bool positions_in_dollars = true,
    std::string_view returns_column_name = returns_column,
    std::string_view cash_column_name = cash_column,
    std::string_view symbol_column_name = attribution_symbol_column);

perf_attribution_statistics create_perf_attrib_stats(
    const perf_attribution_result& result,
    double annualization = daily_annualization);

dataframe cumulative_returns_less_costs(
    const dataframe& returns,
    std::string_view returns_column_name = returns_column);

dataframe cumulative_returns_less_costs(
    const dataframe& returns,
    const dataframe& costs,
    std::string_view returns_column_name = returns_column,
    std::string_view costs_column_name = returns_column);
~~~

`normalize_attribution_positions` returns a wide timestamp-indexed table of
non-cash double asset columns in their original frame order.  When
`positions_in_dollars` is true, every row is divided by the sum of the absolute
non-NaN asset holdings from that same row, exactly matching the Python
performance-attribution normalization rather than the existing net-value
position-allocation helper.  A zero gross total yields ordinary IEEE division
results (NaN columns) and no warning.  When false, values are retained as
weights.  The function does not create a synthetic MultiIndex or use a generic
position metric whose denominator has different semantics.

`compute_exposures` expects the wide weights form (usually the output of
`normalize_attribution_positions`) and emits a timestamp-indexed DataFrame
whose double columns are the requested `factor_column_names` in their supplied
order.  That nonempty list must have distinct nonempty names and every name
must identify a genuine double factor-loading column.  It prevents an unrelated
numeric loading metadata column from becoming a factor merely because the
underlying heterogeneous DataFrame can store it.  `perf_attrib_core` derives
this list from factor-return double columns and delegates its exposure stage to
`compute_exposures`; `perf_attrib` normalizes only when the caller specifies
dollar positions and then delegates exactly once to the core.  This preserves
an efficient public path for callers that already have weights.

## Attribution arithmetic and output schemas

For every date `t` and factor `f`, the exposure is the sum over assets `s`:

~~~text
exposure[t, f] = sum_s(weight[t, s] * loading[t, s, f])
contribution[t, f] = exposure[t, f] * factor_return[t, f]
common[t] = sum_f(contribution[t, f])
specific[t] = strategy_return[t] - common[t]
tilt[f] = mean_t(exposure[t, f])
tilt_return[t] = sum_f(tilt[f] * factor_return[t, f])
timing[t] = common[t] - tilt_return[t]
~~~

The first output, `risk_exposures`, contains exactly the ordered factor double
columns.  The second, `attribution`, contains those same factor contribution
columns first, followed by the exact ordered double columns:

~~~text
total_returns, common_returns, specific_returns, tilt_returns, timing_returns
~~~

Both use the returns index.  The structure avoids a scalar-series wrapper and
keeps every public result in the accepted DataFrame type.

NaN handling follows the corresponding pandas arithmetic where it materially
affects deterministic outputs: row exposure sums skip NaN products and produce
`0.0` when no product is present; factor contribution and common/tilt row sums
skip NaN terms and similarly produce `0.0` for an all-NaN sum; factor tilt
means skip NaNs and produce NaN for an empty valid sample.  Infinities and
their normal IEEE combinations are not filtered, clamped, or converted to
diagnostics.  This behavior will be locked with the required Anaconda-base
Python oracle, including zero-gross normalization and non-finite fixtures.

`create_perf_attrib_stats` returns a `std::map` using Python's seven summary
labels:

~~~text
Annualized Specific Return
Annualized Common Return
Annualized Total Return
Specific Sharpe Ratio
Cumulative Specific Return
Cumulative Common Return
Total Returns
~~~

Its `factor_summary` must use an ordinal `{0, 1, ...}` index because native
DataFrame indexes are timestamps.  It has the ordered `std::string` column
`factor`, then the three double columns `Average Risk Factor Exposure`,
`Annualized Return`, and `Cumulative Return`; factor rows preserve exposure
column order.  The implementation delegates scalar metrics to the accepted
`annual_return`, `sharpe_ratio`, and `cumulative_return` APIs, so this module
does not reproduce or fork their finite-value and annualization semantics.

`cumulative_returns_less_costs(returns)` delegates to accepted
`cumulative_returns`.  The two-frame overload requires the same exact index
sequence, subtracts the selected double costs column row-by-row, and delegates
the resulting series to that same accepted API.  It does not add a new
alignment policy or alter existing return behavior.

## Implementation and verification boundary

The implementation will be a focused metric module:

- create `include/fincore/metrics/perf_attrib.hpp`;
- create `src/metrics/perf_attrib.cpp` with private schema/key/exposure helpers;
- create `tests/unit/perf_attrib_test.cpp` using only public headers;
- register the source and test in existing CMake files; and
- add `perf_attrib.hpp` to the public `metrics.hpp` umbrella plus an umbrella
  consumer assertion.

The first test change will be public-header RED: building the registered test
must fail solely because the new header/API does not yet exist.  Before native
implementation, the mandated command
`/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python` will record exact
Python fixtures for two dates/two assets/two factors, long/short weights,
time-varying exposures with nonzero tilt/timing, cash removal and dollar
normalization, cost subtraction, factor-summary values, NaNs, and zero-gross
rows.

Native tests must assert names, order, index, concrete column types, and exact
or tolerance-bounded numeric values.  They must cover typed-empty valid
schemas, custom return/cash/symbol names, unsorted-but-identically-indexed
inputs, malformed dimensions, mismatched indexes, missing/duplicate loading
keys, factor-name collisions, wrong double/string types, and unrelated
metadata.  They will verify public delegation by comparing the seven summary
values to independently called accepted scalar APIs.

Acceptance will use the established gates: fresh AppleClang C++23 headless
core, fresh Homebrew LLVM ASan+UBSan headless core, default local
ImGui/ImPlot/demo compile/link without launching a GUI, and the existing
invalid-ImGui CMake diagnostic.  It will explicitly distinguish those build
gates from the deferred visualization runtime and from full fincore/Python
parity.
