# fincore C++ Phase 10 Brinson Attribution Design

## Purpose and bounded scope

Phase 10 ports the deterministic BHB (Brinson-Hood-Beebower) arithmetic in
Python `fincore.attribution.brinson` into a C++23,
cloudQuant/DataFrame-native attribution surface. It accepts four wide
timestamp-indexed tables of selected sector returns and weights, produces a
typed per-period attribution table, and exposes strongly typed single-period
and cumulative summaries. The work extends the native attribution story
started by Phase 9 without adding a pandas dependency, a Python runtime, or a
synthetic MultiIndex abstraction.

The Python module also offers a stateful `BrinsonAttribution` convenience
class with sector mapping, equal-weight fallbacks, and an explicitly
unimplemented `brinson_hood` branch. Those are not part of this phase. A
caller that needs sector aggregation must supply already-sectorized wide
tables. Style attribution, Fama-French regressions/providers, warning/drop
alignment, reports/tearsheets, packaging, and GUI-runtime behavior remain
separate work.

## Native table contract

Every input is the accepted `fincore::dataframe`
(`hmdf::StdDataFrame<fincore::timestamp>`). The caller supplies an explicit,
nonempty `std::span<const std::string_view>` of sector column names; the
library never guesses that every numeric metadata column represents a sector.

| Input | Schema | Meaning |
| --- | --- | --- |
| `portfolio_returns` | duplicate-free timestamp index and selected genuine `double` sector columns | each sector's portfolio-held return for every period |
| `benchmark_returns` | same index and selected sector columns | the comparison return for every sector and period |
| `portfolio_weights` | same index and selected sector columns | portfolio sector weights |
| `benchmark_weights` | same index and selected sector columns | benchmark sector weights |

All four timestamp sequences must be exactly equal, including their original
order; timestamps must be unique within each input. Selected names must be
nonempty and distinct. Each selected column must exist as a genuine `double`,
match its owning index length, and be available on every input. Nonselected
columns, including metadata and unrelated numeric columns, are ignored. No
input is sorted, reindexed, date-normalized, or silently intersected. A
malformed column, missing/wrongly typed sector, duplicate timestamp,
duplicate/empty sector selector, or unequal index sequence throws
`fincore::input_error`.

An empty, correctly typed schema is valid for the multi-period and cumulative
operations. It yields a typed-empty per-period result and zero cumulative
effects; single-period attribution rejects any row count other than one.

## Public API and output model

The phase creates `fincore/attribution/brinson.hpp` and an umbrella
`fincore/attribution.hpp`. The leaf header defines these output names and
value-owning summaries:

~~~cpp
inline constexpr std::string_view brinson_allocation_column{"allocation"};
inline constexpr std::string_view brinson_selection_column{"selection"};
inline constexpr std::string_view brinson_interaction_column{"interaction"};
inline constexpr std::string_view brinson_total_column{"total"};
inline constexpr std::string_view brinson_portfolio_return_column{
    "portfolio_return"};
inline constexpr std::string_view brinson_benchmark_return_column{
    "benchmark_return"};

struct brinson_period_result {
    double allocation;
    double selection;
    double interaction;
    double total;
    double portfolio_return;
    double benchmark_return;
};

struct brinson_cumulative_result {
    double allocation;
    double selection;
    double interaction;
    double total;
    double portfolio_cumulative;
    double benchmark_cumulative;
};
~~~

`brinson_results(...)` returns the daily `dataframe` with the exact input
timestamp index and the six ordered genuine-`double` columns above. It is the
native counterpart to Python's `brinson_results`; instead of adding a pandas
string `period` column, the period remains the normal timestamp index.
`brinson_attribution(...)` accepts exactly one input timestamp and returns a
`brinson_period_result`. `brinson_cumulative(...)` accepts any valid row
count and returns `brinson_cumulative_result`. All three receive the same
four frames plus the explicit sector-name span. No overload defaults to an
implicit all-column selection.

## Arithmetic and numerical boundaries

For each period `t` and selected sector `s`, Phase 10 evaluates the Python
BHB equations in the caller's selected sector order:

~~~text
allocation[t]  = sum_s((wp[t,s] - wb[t,s]) * rb[t,s])
selection[t]   = sum_s(wb[t,s] * (rp[t,s] - rb[t,s]))
interaction[t] = sum_s((wp[t,s] - wb[t,s]) * (rp[t,s] - rb[t,s]))
total[t]       = allocation[t] + selection[t] + interaction[t]
portfolio[t]   = sum_s(wp[t,s] * rp[t,s])
benchmark[t]   = sum_s(wb[t,s] * rb[t,s])
~~~

No return/weight normalization or sum-to-one validation is added: Python
accepts arbitrary finite weights and the formulas are meaningful for net,
gross, long/short, or partially invested inputs. Ordinary IEEE arithmetic is
preserved. In particular, NaN and signed infinities propagate through the
same direct multiplications and sums rather than being filtered or converted
to a warning. Native code deliberately does not reproduce NumPy's warning
emission; its deterministic numeric results are the compatibility boundary.

`brinson_cumulative` arithmetically sums the four period effects and
geometrically compounds the already-calculated portfolio and benchmark period
returns:

~~~text
portfolio_cumulative = product_t(1 + portfolio[t]) - 1
benchmark_cumulative = product_t(1 + benchmark[t]) - 1
~~~

For zero valid periods, the empty sum is zero and the empty product is one,
therefore both cumulative returns are zero, matching the Python source.

## Verification and integration boundary

Implementation stays focused in one attribution module and its public
umbrella:

- create `include/fincore/attribution/brinson.hpp` and
  `src/attribution/brinson.cpp`;
- create a public-header-only `tests/unit/brinson_test.cpp` and register it in
  the existing CMake library/test targets;
- later create `include/fincore/attribution.hpp` and a separate umbrella-only
  consumer test; and
- record a Phase 10 acceptance document rather than modifying accepted Phase
  9 claims.

The required Anaconda oracle uses Python rows
`rp=[[.10,.05,-.02],[.03,-.01,.04]]`,
`rb=[[.08,.04,-.01],[.02,0,.03]]`,
`wp=[[.50,.30,.20],[.45,.35,.20]]`, and
`wb=[[.40,.40,.20],[.40,.40,.20]]`. It yields per-period effects
`allocation=[.004,.001]`, `selection=[.010,.002]`,
`interaction=[.001,.001]`, `total=[.015,.004]`,
`portfolio_return=[.061,.018]`, and `benchmark_return=[.046,.014]`; its
cumulative values are allocation `.005`, selection `.012`, interaction `.002`,
total `.019`, portfolio `.080098`, and benchmark `.06064400000000014`.

Tests lock that oracle, output order/types/index preservation, single-row
delegation, empty schemas, arbitrary non-unit weights, NaN/infinity behavior,
unsorted identical indexes, unrelated metadata, and every documented input
error. Final acceptance repeats the established AppleClang headless,
Homebrew LLVM ASan+UBSan headless, local ImPlot/demo compile/link, and invalid
ImGui configuration gates without launching a GUI.
