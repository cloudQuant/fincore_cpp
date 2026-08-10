# fincore C++ Phase 2C Path Metrics Design

## Purpose and bounded scope

Phase 2C extends the accepted C++23, DataFrame-native return-path surface with
two Python-reference families that do not require a new portfolio-wide table
model: detailed drawdown episodes and daily consecutive-return metrics.  Every
input remains a validated, one-double-column
`hmdf::StdDataFrame<fincore::timestamp>` whose index is signed UTC Unix
nanoseconds.  The local Python source at
`/Users/yunjinqi/Documents/new_projects/fincore` remains the numerical and
edge-case oracle.

The phase deliberately does not add positions, transactions, round trips,
attribution, simulation, reports, a dynamic façade, or a second DataFrame
type.  It also does not claim the Python weekly/monthly consecutive functions
or `gen_drawdown_table`: both require an explicit calendar-bin and nullable
multi-column table contract.  Those concerns belong to a later phase rather
than being hidden behind a synthetic timestamp or sentinel value.

## Representation decision

Three representations were considered for detailed drawdowns: a `dataframe`
with a fabricated row index and nullable-date sentinel, an untyped map/list
matching Python dictionaries, and native typed value objects.  The selected
form is native typed value objects for outputs that are Python lists, dicts,
or tuples, while `dataframe` continues to be the only time-series transport
format.  This preserves `std::optional<timestamp>` for an unrecovered period;
using a timestamp sentinel would be incorrect because all `int64_t` values,
including the old candidate sentinel, are valid timestamps under the accepted
calendar boundary.

```cpp
struct drawdown_period {
    timestamp start;                         // first negative underwater sample
    timestamp valley;                        // first sample at the period minimum
    std::optional<timestamp> recovery;       // first non-underwater sample, if any
    double value;                            // most-negative underwater value
    std::size_t duration_periods;            // start -> valley observations
    std::optional<std::size_t> recovery_duration_periods;
};

struct drawdown_window {
    timestamp peak;                          // preceding running-maximum sample
    timestamp valley;
    std::optional<timestamp> recovery;
};
```

These types are ordinary C++ values with no ownership or DataFrame lifetime
coupling.  Output ordering is chronological for all periods; severity ranking
uses a stable chronological tie break.

## Drawdown semantics

`drawdown.hpp` keeps the already accepted `drawdown_series` and
`max_drawdown` untouched, then gains `get_all_drawdowns`,
`get_all_drawdowns_detailed`, and `max_drawdown_window`.  The detailed
detector follows Python `_identify_drawdown_periods`: compound NaN returns as
zero, construct wealth from the first observed return rather than adding a
synthetic pre-observation point, compare wealth with its running maximum, and
split contiguous strictly negative underwater segments.  A recovered segment
ends at the first zero/positive underwater sample; an open final segment has
`recovery == std::nullopt`.  The earliest trough wins a tie.

This intentionally preserves a Python edge distinction: the existing scalar
`max_drawdown` includes an initial portfolio value and therefore can report an
initial loss, while detailed periods start at the first observed cumulative
wealth and can have no period for a one-observation initial loss.  The native
documentation and tests record that source-compatible distinction rather than
silently changing accepted scalar behavior.

`max_drawdown_days`, weeks, months, recovery variants, and second/third
drawdown helpers are added in a follow-up task using the same private
underwater path.  For timestamp-indexed native DataFrames, days means elapsed
UTC 24-hour days from the relevant peak/valley/recovery timestamps; weeks and
months divide that value by 7 and 30, matching the Python helpers.  Empty
inputs return `NaN` for scalar duration/rank results.  A nonempty path with no
underwater period preserves Python's zero-duration behavior.  Missing recovery
returns `NaN`.  Inputs retain their supplied row order; callers must supply a
chronological path for chronological duration meaning.

## Daily consecutive metrics

Add `metrics/consecutive.hpp` and a matching source file.  It supplies the
Python-compatible daily APIs `max_consecutive_up_days`,
`max_consecutive_down_days`, `max_consecutive_gain`,
`max_consecutive_loss`, `max_single_day_gain`, `max_single_day_loss`, and the
six corresponding single-day/run date accessors.  Date accessors return
`std::optional<timestamp>` instead of pandas `None`/`NaT`.

A private one-pass run scanner preserves frame order and implements strict
positive/negative predicates: zero and NaN break both run kinds, while signed
infinity follows ordinary comparison behavior.  Empty numeric results are
`NaN`; a nonempty path with no qualifying up/down sample yields zero for the
count and `NaN` for the cumulative gain/loss, matching Python.  Equal-length
and equal-value ties select the earliest run/sample, matching pandas
`idxmax`/group order.  The native batch type, if exposed, is explicitly named
`daily_consecutive_statistics`; the Python `consecutive_stats` name remains
deferred because it promises weekly and monthly fields that this phase does
not yet implement.

## Validation and acceptance boundary

Every public function calls `require_double_column`, preserving existing
`input_error` behavior for absent, empty, wrong-typed, or short columns.  No
function sorts input, normalizes indexes, or applies an unrequested finite
filter.  Tests obtain fixed Python goldens using the required Anaconda command
and cover multiple recovered/open drawdowns, the initial-loss distinction,
timestamp durations, rank ties, NaN handling, empty/structural errors, zeros,
earliest ties, and optional dates.

Phase 2C is accepted only after public-header/umbrella coverage, focused path
tests, fresh AppleClang headless CTest, Homebrew LLVM ASan+UBSan CTest, and a
scope review.  The positive ImPlot build/demo remains a separate open gate
until a real local Dear ImGui checkout is supplied.  Completing this phase is
not full fincore parity or project completion.
