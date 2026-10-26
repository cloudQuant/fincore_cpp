# fincore C++ Phase 6B Quadratic Slippage Penalty Design

## Purpose and bounded scope

Phase 6B ports the deterministic quadratic volume-share penalty in Python
fincore.metrics.transactions.apply_slippage_penalty. It accepts a daily return
series plus a raw or daily transaction-with-bar-data table containing amount,
price, and bar volume. It scales transaction size to a simulated capital base,
aggregates its quadratic costs by UTC day, and subtracts the daily cost from
the return measured against compounded simulated portfolio value.

This phase does not create transaction-to-market-data joins. It therefore does
not implement symbols/SIDs, daily_txns_with_bar_data, liquidity or liquidation
metrics, market data providers, square-root or other impact models, round
trips, attribution, reports, packaging, or a GUI runtime.

## Public surface and native inputs

The existing public transactions header receives one new constant and API:

~~~cpp
inline constexpr std::string_view transaction_bar_volume_column{"volume"};

dataframe apply_slippage_penalty(
    const dataframe& returns,
    const dataframe& transactions_with_bar_data,
    double simulate_starting_capital,
    double backtest_starting_capital,
    double impact = 0.1,
    std::string_view returns_column_name = returns_column,
    std::string_view amount_column_name = transaction_amount_column,
    std::string_view price_column_name = transaction_price_column,
    std::string_view volume_column_name = transaction_bar_volume_column);
~~~

Returns require the requested genuine double column and a strictly increasing,
unique UTC-day index. The output preserves only those normalized return days
and uses exactly returns_column_name as its one genuine double column. An empty
typed return series produces a typed empty output.

The transaction-with-bar-data frame is row-oriented rather than a strict daily
series: duplicate, unordered, and intraday timestamps are allowed, because
one day may contain multiple traded names. It requires distinct, nonempty,
genuine double amount, price, and volume columns, each length-matched to the
index. Unrelated fields are ignored. Unlike the older raw transaction volume
operation, this input volume is external bar volume, not the output name
txn_volume. The name transaction_bar_volume_column makes that distinction
explicit.

backtest_starting_capital must not be exactly zero and otherwise follows
ordinary IEEE arithmetic; zero is a native input_error counterpart to Python
float division by zero. simulate_starting_capital and impact are deliberately
not finite-validated or clamped, matching the natural propagation/fill
behavior of the Python expression.

## Calculation and missing-value semantics

Let m equal simulate_starting_capital / backtest_starting_capital. For every
transaction row i, calculate:

~~~text
simulated_shares_i  = abs(m * amount_i)
simulated_dollars_i = price_i * simulated_shares_i
volume_share_i      = simulated_shares_i / volume_i
penalty_i           = volume_share_i^2 * impact * simulated_dollars_i
~~~

Normalize each raw row to UTC midnight and sum penalty_i by day while skipping
only NaN contributions, matching pandas resample("D").sum(). Ordinary signed
infinity participates in the sum; a NaN daily total caused by normal IEEE
arithmetic is later treated as a missing daily penalty and becomes zero.
Transaction days not present in returns do not appear in the output.

For each return in its validated input order, compound a private wealth factor:

~~~text
wealth_d = wealth_previous * (1 + (isnan(return_d) ? 0 : return_d))
portfolio_value_d = (wealth_d * backtest_starting_capital) * m
portfolio_value_d = NaN when it is exactly zero
daily_penalty_d = 0 when absent or NaN
adjusted_d = return_d - daily_penalty_d / portfolio_value_d
result_d = return_d when adjusted_d is NaN; otherwise adjusted_d
~~~

This directly preserves Python cum_returns behavior: NaN return is zero only
for the wealth path and remains NaN in the final output; a zero portfolio
value falls back to the original return; nonfinite adjusted values are not
globally sanitized. A finite return with nonzero portfolio value and infinite
penalty can therefore produce negative infinity.

## Oracle and verification boundary

The original capacity fixture has returns [1, 1, 1], 100000 shares at price 1
each day, and bar volumes [1000000, 2000000, 3000000]. With simulated and
backtest capital both 1000000 and impact 1, expected adjusted returns are:

~~~text
[0.9995, 0.9999375, 0.9999861111111111]
~~~

Additional Anaconda oracle values pin capital scaling and impact:
simulate 10000000 gives [0.95, 0.99375, 0.9986111111111111], while impact 0.1
gives [0.99995, 0.99999375, 0.9999986111111111].

A separate unordered duplicate-intraday fixture uses returns [.1, .2, NaN,
-1, .3], Jan 2 duplicate trades, and Jan 3/6 trades. With both capitals 1000
and impact .1 it yields
[.09981818181818182, .1999810606060606, NaN, -1, .3]. Tests additionally pin
zero bar volume/infinite cost, NaN transaction fields, NaN impact, typed
empties, custom names, exact-zero backtest capital rejection, and all
structural field/date failures. Fresh compiler and optional ImPlot gates are
evidence-only; no GUI is launched.
