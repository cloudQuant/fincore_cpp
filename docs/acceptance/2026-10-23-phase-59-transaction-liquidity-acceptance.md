# Phase 59 Native Transaction Liquidity Acceptance

## Scope

Adds the three liquidity kernels and the `trade_record` value type to the
existing transactions leaf. Three files changed; no dependency change.

## Python parity

Source: `fincore/portfolio/transactions.py` 63–223. Clean detached oracle at
`4b8f31b` (`/tmp/fincore-phase59-python.xbOTIp`): module path asserted,
exit `0`, zero stderr bytes. Script SHA-256
`6bc5e62924473be499be017a0824e9ef81ac85fbfe023ee4af498987a6385310`; raw
output SHA-256
`0b6c65d6405f138e02e7e4a69dd8ebb24e8bd290ae635a87443306a7f8de1630`.

Verified: shifted rolling mean dollar volume, zero-mean NaN, dropped warm-up
rows, cash-inclusive allocation with cash excluded from output, ticker
maximum selection with allocation percent, low-liquidity daily aggregation,
and both `last_n_days` filters. All values match at 1e-12.

## Test coverage

Three tests: days/maximums matrix, low-liquidity paths, and validation
(bar consumption, window, index mismatch, NUL).

## Gates (current tree)

- Full headless CTest: **596/596 passed**.
- LLVM 22 ASan+UBSan: **596/596 passed**, no reports.
- ImPlot compile/link unchanged.

## Limits

Wide market-data frames must share the positions index; the long transaction
ledger is a `trade_record` span. Ties select the earliest date (goldens avoid
ties).
