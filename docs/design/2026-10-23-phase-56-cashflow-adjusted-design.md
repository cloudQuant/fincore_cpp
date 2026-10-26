# Phase 56 Native Cashflow-Adjusted Returns Design

## Source contract

`fincore/performance/cashflows.py` 276–391. Key rules:

- Valuations: timezone-aware, unique, increasing DatetimeIndex; at least two
  rows; finite; non-negative with all but the last strictly positive.
- Cashflows: either a Series with one net flow per valuation date (scalar
  timing policy) or a ledger frame with exactly `amount` and `timing`
  (`"start"`/`"end"` per row) and duplicate dates; ledger dates must exist in
  the valuation index.
- Fees: timezone-consistent, unique dates, finite, reindexed to valuations.
- First valuation is an opening observation; nonzero flows or fees there are
  rejected.
- Per-period returns:
  `end`: `(V_end + gross_fee - end_flow) / (V_start + start_flow) - 1`;
  `start`: `(V_end + gross_fee) / (V_start + start_flow) - 1` where the start
  flow is included on both sides as in the source.
- Validation: finite intermediate capitals, positive capital base,
  non-negative adjusted closing, finite factors/returns, and representability
  guard when adjusted closing is positive but the return factor underflows to
  exactly `-1`.
- TWR compounds factors and rejects non-finite or unrepresentable totals.

## Native decisions

- Ledger timing is encoded numerically (`0` end, `1` start) because the
  native column model is numeric; this is the explicit representation of the
  Python per-row metadata, not an implicit default.
- Duplicate ledger and fee dates are summed; this is a superset of the Python
  unique-index constraint and is documented rather than hidden.
- Currency/FX parameters are not exposed; the boundary is single-currency, so
  non-reporting-currency inputs cannot be silently mis-converted. This is a
  declared scope limit, not parity for the FX branch.
- Invalid classes map Python `ValueError` to `input_error` with the source
  messages.

## Oracle evidence

Root `/tmp/fincore-phase56-python.wbwqZX`, detached at `4b8f31b`, exit `0`,
zero stderr bytes. Script SHA-256
`2afe1cb8b91955e3431bd88f5408ae6e6326bad2c455535de351846b9af777b1`; raw
output SHA-256
`35a29dd803e1b8a094a6f77defa30e38ea021b2b04ea9b917bf095df9d8c645a`.

Goldens: mixed ledger returns `[0, 0, 0.0714285714285714,
0.04166666666666674]`, TWR `0.1160714285714286`; gross fee first period
`0.004999999999999893`, TWR `0.12165178571428581`; uniform end TWR
`0.12103174603174605`; uniform start `0.1160714285714286`; same-day
aggregation TWR `0.13636363636363646`; terminal zero TWR `-1.0`.

## Tests

Four tests: ledger paths, gross fees, uniform/same-day, terminal loss and
validation errors. The first terminal-zero fixture incorrectly had zeros in
non-terminal positions and was corrected to `[100, 110, 0]`, matching the
source's "terminal zero is allowed" rule.
