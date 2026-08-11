# fincore C++ Phase 8 Round-Trip Core Design

## Purpose and bounded scope

Phase 8 adds the first native, DataFrame-backed round-trip trading boundary:
it consolidates same-symbol executions that are consecutive in direction and
time, then reconstructs completed long and short trades with the Python
module's FIFO matching rule.  This is the natural next layer above the
accepted transaction and position metrics: it keeps execution rows in a
cloudQuant DataFrame and produces a typed, mixed-column DataFrame rather than
requiring pandas grouping, a MultiIndex, or Python objects.

The phase deliberately implements the deterministic core only.  It does not
add portfolio-value-normalized round-trip returns, synthetic end-of-period
closing trades, sector mappings, dynamically configured summary statistics,
tearsheets, factor attribution, Bayesian simulation, market-data joins, or a
GUI runtime.  Those need different input contracts and will remain separate
native slices.

## Public API and native table model

The new `fincore/metrics/round_trips.hpp` header exports the following
constants and operations:

~~~cpp
inline constexpr std::string_view transaction_symbol_column{"symbol"};
inline constexpr std::string_view round_trip_open_datetime_column{"open_dt"};
inline constexpr std::string_view round_trip_close_datetime_column{"close_dt"};
inline constexpr std::string_view round_trip_duration_column{"duration"};
inline constexpr std::string_view round_trip_pnl_column{"pnl"};
inline constexpr std::string_view round_trip_return_column{"rt_returns"};
inline constexpr std::string_view round_trip_long_column{"long"};

inline constexpr std::chrono::nanoseconds default_round_trip_max_delta{
    std::chrono::hours{8}};

dataframe groupby_consecutive_transactions(
    const dataframe& transactions,
    std::chrono::nanoseconds max_delta = default_round_trip_max_delta,
    std::string_view symbol_column_name = transaction_symbol_column,
    std::string_view amount_column_name = transaction_amount_column,
    std::string_view price_column_name = transaction_price_column);

dataframe extract_round_trips(
    const dataframe& transactions,
    std::chrono::nanoseconds max_delta = default_round_trip_max_delta,
    std::string_view symbol_column_name = transaction_symbol_column,
    std::string_view amount_column_name = transaction_amount_column,
    std::string_view price_column_name = transaction_price_column);
~~~

`groupby_consecutive_transactions` emits the original start timestamp as its
index and exactly the requested amount (`double`), symbol (`std::string`), and
price (`double`) columns, in that order.  With defaults this is `amount`,
`symbol`, `price`; custom input names are preserved in this consolidated
output.  The requested symbol, amount, and price names must be nonempty and
distinct.
The three required input columns must respectively be a genuine
`std::string`, `double`, and `double` column whose lengths equal the DataFrame
index length.  Unrelated input columns are ignored.  Typed-empty valid input
produces a typed-empty result with the same three columns.

Python's `extract_round_trips` returns a pandas DataFrame with a RangeIndex
and timestamp-valued fields.  A `StdDataFrame<timestamp>` has one concrete
index type, so the native result uses the sequential ordinal index
`{0, 1, ...}` and exposes the event fields as typed columns.  Its exact base
schema is:

| Column | Native type | Meaning |
| --- | --- | --- |
| `pnl` | `double` | matched trade profit and loss |
| `open_dt` | `timestamp` | opening execution timestamp |
| `close_dt` | `timestamp` | closing execution timestamp |
| `long` | `bool` | true for a long trade closed by a sell, false for a short trade closed by a buy |
| `rt_returns` | `double` | `pnl / invested` or zero for exact zero investment |
| `symbol` | `std::string` | source execution symbol |
| `duration` | `timestamp` | `close_dt - open_dt`, measured in nanoseconds |

The output index is an ordinal row identifier, not a UTC timestamp.  Valid
input which yields no completed round trip returns a typed-empty DataFrame
with this full schema.

`metrics.hpp` will include `metrics/round_trips.hpp`; the new header will
include the existing transactions header for the accepted `amount` and
`price` defaults.  No generic string-column helper is added to `dataframe.hpp`:
the round-trip module performs its own private structural validation to keep
the new contract local.

## Execution consolidation semantics

The implementation follows the Python `groupby_consecutive` model as a
deterministic native operation:

1. Partition rows by symbol in ascending string order.  Within a symbol,
   process executions by ascending timestamp, retaining original row order for
   exact timestamp ties.
2. A new block begins at the first row, whenever the boolean direction
   `amount > 0` changes, or whenever the elapsed timestamp gap is strictly
   greater than `max_delta`.  A gap exactly equal to `max_delta` remains in the
   same block.
3. For each block, emit the first timestamp and symbol, sum its signed amounts,
   and calculate the signed-amount VWAP:

   ~~~text
   price = sum(amount_i * price_i) / sum(amount_i)
   ~~~

   An exactly zero summed amount emits a NaN VWAP, mirroring the numerical
   Python result without adding a warning channel.  Other NaN and infinity
   arithmetic remains ordinary IEEE arithmetic and is neither filtered nor
   clamped.

The output is ordered by symbol then by that symbol's consolidated blocks;
its index is therefore not required to be globally timestamp-sorted across
different symbols.  Negative `max_delta` is accepted as a duration value and
uses the same strict `gap > max_delta` comparison rather than being silently
normalized.

## FIFO round-trip semantics

`extract_round_trips` first calls the same consolidation logic and then
processes each symbol independently in timestamp/block order.  Each open lot
stores its signed price, remaining absolute quantity, and open timestamp.  An
execution in the same direction appends a lot.  An opposite-direction
execution closes quantities from the front of the queue (FIFO):

~~~text
matched  = min(remaining_close_quantity, open_quantity)
pnl     += -(closing_signed_price + opening_signed_price) * matched
invested += abs(opening_signed_price) * matched
~~~

One result row is emitted for each consolidated closing execution that matches
at least one open lot.  If that execution crosses through zero, its unmatched
remainder becomes a new open lot in the opposite direction.  The result's
`open_dt` is the first lot consumed by the closing execution, `close_dt` is
the closing block timestamp, and `long` is derived from the closing signed
price exactly as in the Python implementation.  This preserves partial fills,
crossing long-to-short and short-to-long transactions, and multiple open lots
consumed by one close.

An execution whose consolidated price is negative is ignored, matching the
Python routine's negative-price skip without exposing Python warnings in the
C++ API.  No finite-value sanitation, date-flooring, transaction-to-position
join, or artificial closing transaction is introduced.

## Verification and implementation boundary

The implementation will use the required Anaconda-base Python oracle to pin
same-direction VWAP consolidation, exact eight-hour behavior, multiple
symbols, FIFO partial fills, long/short crossings, zero-investment output,
negative-price skipping, duplicate timestamps, typed empties, custom column
names, and structural error cases.  Public-header RED tests must precede
production changes.  A public-only `metrics.hpp` umbrella consumer will prove
that the finished API is usable without internal headers.

Final acceptance will rerun fresh AppleClang core, Homebrew LLVM ASan+UBSan
core, the default local ImGui/ImPlot/demo compile/link gate, and the existing
invalid-ImGui configure gate.  It will not launch a GUI or claim full
fincore/Python parity.
