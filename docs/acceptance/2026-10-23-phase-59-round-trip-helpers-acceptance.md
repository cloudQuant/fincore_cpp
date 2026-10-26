# Portfolio Round-Trip Helpers Acceptance — Sectors and Closing Trades

## Scope

Ports two remaining portfolio round-trip helpers with typed native inputs:

- `apply_sector_mappings_to_round_trips(round_trips, mappings)` copies the
  round-trip frame and adds a `sector` string column resolved by the `symbol`
  column. Unmapped symbols receive an empty string (the source emits NaN);
  an existing `sector` column is replaced, and a frame without `symbol` is
  copied without a sector column.
- `add_closing_transactions(positions, transactions)` appends one closing
  `round_trip_transaction` per still-open asset at the final position
  timestamp plus one second. The closing amount is the negative sum of that
  symbol's transaction amounts; the price is the final dollar position
  divided by that amount. Assets whose net amount is below `1e-6` are
  skipped, and zero-amount rows are removed, matching the source.

## Evidence

The typed transaction ledger replaces the pandas frame because the native
`dataframe` index is unique and cannot hold several transactions at one
timestamp. Prices are carried explicitly instead of being inferred.

Tests (`RoundTrips.AppliesSectorMappingsToSymbols`,
`RoundTrips.AddsClosingTransactionsAtFinalTimestamp`):

- A/B map to Technology/Financials, C stays empty; the `pnl` column is
  preserved; a frame without symbols gains no sector column.
- Positions A=100, B=-50, C=0; transaction sums A=+10, B=-5 produce closing
  trades `A: -10 @ 10` and `B: +5 @ 10` at `final + 1s`, while C (zero
  amount) is skipped and original transactions stay in order.

## Declared gaps

`agg_all_long_short` (arbitrary caller-supplied statistic functions over a
pandas round-trip frame) remains unresolved, as do the two attribution
helpers. NaN sector labels are represented as empty strings.

## Gates

Full headless CTest: **718/718 passed**. Sanitizer not re-run; visual
NOT_RUN.
