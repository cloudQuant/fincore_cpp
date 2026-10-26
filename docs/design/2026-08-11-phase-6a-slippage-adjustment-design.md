# fincore C++ Phase 6A Slippage-Adjusted Returns Design

## Purpose and bounded scope

Phase 6A ports the small, deterministic basis-point penalty in Python
`fincore.metrics.transactions.adjust_returns_for_slippage`. It composes the
already accepted native daily transaction-volume aggregation with daily return
and position inputs. The result is a one-column, DataFrame-native adjusted
return series; it does not introduce an execution model, a liquidity model,
or a generic pandas arithmetic layer.

This phase deliberately excludes quadratic/market-impact slippage, transaction
mapping and symbols/SIDs, bar-data joins, liquidity or liquidation analysis,
round trips, attribution, reports/tearsheets, a complete Python facade,
packaging/release, and interactive GUI validation.

## Public surface and input contract

The existing public `transactions.hpp` owns the API, so callers do not need a
new module or an additional umbrella include:

~~~cpp
dataframe adjust_returns_for_slippage(
    const dataframe& returns,
    const dataframe& positions,
    const dataframe& transactions,
    double slippage_bps,
    std::string_view returns_column_name = returns_column,
    std::string_view amount_column_name = transaction_amount_column,
    std::string_view price_column_name = transaction_price_column);
~~~

`returns` requires the named genuine `double` column, length-matched to its
timestamp index. Its normalized UTC days must be strictly increasing and
unique. `positions` uses the accepted Phase 5A/5B contract: every genuine
`double` column is a holding, every selected column is length-matched, at
least one selected column is required, and its normalized UTC days are
strictly increasing and unique. Non-double metadata is ignored. This native
position selection is intentionally consistent with existing position and
turnover APIs rather than broadening the surface to arbitrary numeric types.

Raw transactions retain the Phase 5B contract: distinct nonempty named genuine
`double` amount and price columns, each index-length matched; duplicate and
intraday rows are valid. `slippage_bps` is ordinary IEEE input: it is not
finite-validated or clamped, so negative, NaN, and infinite values follow the
same arithmetic boundary as the Python formula.

The native result has one genuine `double` column named exactly
`returns_column_name`, including when the caller uses a custom name. It is
indexed by the ascending UTC-midnight union of return, position, and raw
transaction days. Valid empty typed inputs produce a typed empty result. A
return frame may be empty while valid positions or transactions contribute
union days; those result values are zero because no return/PnL pair exists.

## Arithmetic and missing-value semantics

The implementation first obtains daily `txn_volume` through the accepted
`get_txn_vol(transactions, amount_column_name, price_column_name)` public
operation. For each normalized day `d`, it computes:

~~~text
portfolio_value[d] = sum of all genuine-double position holdings, skipping NaN
pnl[d]             = portfolio_value[d] * returns[d]  (only when both exist)
slippage[d]        = txn_volume[d] * (0.0001 * slippage_bps)
adjusted_pnl[d]    = pnl.add(-slippage, fill_value=0)
adjusted_return[d] = returns[d] * adjusted_pnl[d] /
                     pnl[d].replace(0, NaN)
result[d]          = adjusted_return[d].fillna(0)
~~~

This is deliberately a date-keyed implementation of the Python Series
operations, not positional pairing. For `adjusted_pnl`, an absent or `NaN`
operand is treated as zero only when the other operand is present and
non-NaN; when both are absent/NaN, it remains `NaN`. Final replacement is
`NaN`-only: it changes an absent return, a NaN return, a zero PnL denominator,
or indeterminate arithmetic to `0.0`, but preserves a computed `+infinity` or
`-infinity`. For example, a finite nonzero return/PnL paired with infinite
traded value yields an infinite adjusted return, whereas zero PnL remains
zero even if there is a transaction penalty.

Transaction NaN contribution behavior remains exactly that of `get_txn_vol`:
NaN amount contributes neither shares nor volume; NaN price can retain a
share contribution but not a volume contribution. A daily `NaN` volume made
by ordinary IEEE arithmetic (such as `+infinity + -infinity`) participates as
a missing Series operand in `adjusted_pnl`; it is not globally finite-filtered.
No final `isfinite` sanitation is applied to adjusted returns.

## Oracle and verification boundary

The local Anaconda Python oracle uses returns `[.1, .2, NaN, .3]` on Jan 2,
3, 5, and 6; positions asset/cash `[100,900]`, `[200,800]`, `[300,700]` on
Jan 2, 4, and 5; and one 100-dollar transaction on each of Jan 2 through 5.
At 10 bps, the sorted union result is:

~~~text
Jan 2: 0.0999
Jan 3: 0
Jan 4: 0
Jan 5: 0
Jan 6: 0
~~~

The test matrix also pins custom names, typed empties, intraday UTC
normalization, returns/positions/transactions date mismatch, zero PnL,
NaN return and raw transaction contributions, NaN bps, an infinite-volume
penalty, malformed fields, nonnumeric/malformed positions, and duplicate or
nonascending normalized daily returns/positions. Fresh headless,
Homebrew-LLVM ASan+UBSan, and optional ImPlot compile/link gates remain
evidence-only; no GUI is launched.
