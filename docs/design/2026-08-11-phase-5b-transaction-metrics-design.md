# fincore C++ Phase 5B Transaction Metrics Design

## Purpose and bounded scope

Phase 5B ports the smallest useful, DataFrame-native numerical core of Python
fincore.metrics.transactions: daily transaction aggregation (get_txn_vol) and
portfolio turnover (get_turnover). It follows Phase 5A's multi-column
positions contract while adding an explicit raw-transaction table contract.

This phase deliberately excludes transaction symbols/SIDs, row mapping and
frame construction, bar-data joins, liquidity and liquidation analysis,
slippage models, round trips, performance attribution, reports, and GUI
runtime. It does not introduce a generic pandas compatibility layer, a
MultiIndex type, or an external market-data dependency.

## Input shapes and public API

Raw transactions are a DataFrame with a timestamp index that may contain
duplicate, intraday records. Each relevant record has genuine double amount
and price columns; both are required, distinct, nonempty names and must match
the index length. Unrelated columns, including non-double metadata and
unrelated malformed fields, are intentionally ignored.

Daily positions use the Phase 5A convention: every genuine double column is a
numeric holding and every selected column matches the index. For turnover,
each UTC calendar day must occur at most once and the supplied day index must
be strictly increasing. This makes the first-day actual-gross-book rule and
two-day rolling denominator unambiguous. The native public header declares:

~~~cpp
inline constexpr std::string_view transaction_amount_column{"amount"};
inline constexpr std::string_view transaction_price_column{"price"};

enum class turnover_denominator {
    actual_gross_book,
    portfolio_value,
};

dataframe get_txn_vol(
    const dataframe& transactions,
    std::string_view amount_column_name = transaction_amount_column,
    std::string_view price_column_name = transaction_price_column);

dataframe get_turnover(
    const dataframe& positions,
    const dataframe& transactions,
    turnover_denominator denominator = turnover_denominator::actual_gross_book,
    std::string_view cash_column_name = cash_column,
    std::string_view amount_column_name = transaction_amount_column,
    std::string_view price_column_name = transaction_price_column);
~~~

get_txn_vol returns ordered UTC-midnight timestamps and exact double columns
txn_volume then txn_shares. For each day it sums abs(amount) * price and
abs(amount), respectively. Days are ascending regardless of raw transaction
input order. A valid zero-row transaction frame returns typed empty columns.

## Turnover and numerical rules

get_turnover returns one double column named turnover on the ascending union
of daily position dates and transaction dates. It first aggregates raw
transactions exactly as get_txn_vol. NaN transaction values are skipped by the
same sum behavior used by pandas groupby; signed infinity is ordinary IEEE
arithmetic at this stage.

With actual_gross_book, a genuine named double cash column is required and is
excluded from daily absolute exposure. The first denominator is half that
day's gross book; each subsequent denominator is the average of the previous
and current daily gross books. With portfolio_value, no cash column is
required and the denominator is the row sum of all selected numeric holdings.
Both position reductions skip NaN values and preserve ordinary signed-infinity
arithmetic. Any final turnover NaN, positive infinity, or negative infinity
becomes zero, matching the Python function's replace-plus-fill behavior.

Unknown enum values, invalid required names, missing/wrong-typed/malformed
amount or price fields, no numeric position columns, invalid cash for
actual-gross-book, and duplicate or nonascending normalized position dates
throw input_error. No generic dataframe.hpp expansion is introduced: all
selection, daily normalization, validation, and output construction helpers
remain private to transactions.cpp.

## Oracle and verification boundary

The local Anaconda oracle uses raw transactions at 2024-01-02 09:30,
2024-01-02 15:45, 2024-01-03 10:00, and 2024-01-05 11:00 with amount
[10, -4, NaN, 3] and price [5, 10, 7, +infinity]. Its daily transaction
output is, in ascending order:

~~~text
2024-01-02: txn_volume=90, txn_shares=14
2024-01-03: txn_volume=0,  txn_shares=0
2024-01-05: txn_volume=+infinity, txn_shares=3
~~~

For daily positions asset=[100,-50,20], cash=[900,1050,980] on Jan 2, 3,
and 4, actual-gross-book denominators are [50,75,35]. The output date union
Jan 2 through Jan 5 has actual-gross-book turnover [1.8,0,0,0] and
portfolio-value turnover [.09,0,0,0]. Tests also cover custom names,
normalization and sorting, valid typed empty results, duplicate/out-of-order
position days, structural failures, cash distinction, NaN/infinity handling,
public-umbrella consumption, and fresh non-GUI build gates.
