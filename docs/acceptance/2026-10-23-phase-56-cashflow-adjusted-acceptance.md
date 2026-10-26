# Phase 56 Native Cashflow-Adjusted Returns Acceptance

## Scope

Adds `cashflow_adjusted_returns`, `cashflow_adjusted_twr`, and the
`fee_treatment` enum in a new cashflows leaf. Five new/changed paths plus
CMake registrations; no dependency change.

## Python parity

Source: `fincore/performance/cashflows.py` 276–391. Clean detached oracle at
`4b8f31b` (`/tmp/fincore-phase56-python.wbwqZX`): module path asserted,
exit `0`, zero stderr bytes. Script SHA-256
`2afe1cb8b91955e3431bd88f5408ae6e6326bad2c455535de351846b9af777b1`; raw
output SHA-256
`35a29dd803e1b8a094a6f77defa30e38ea021b2b04ea9b917bf095df9d8c645a`.

Verified: mixed-timing ledger, gross/net fee treatment, uniform series
timing, same-day aggregation, terminal-zero loss, and the opening-date
flow/fee rejection. Returns and TWR goldens match at 1e-12.

## Declared scope limits

- Currency/FX parameters are not exposed; the native boundary is
  single-currency, so non-reporting-currency flows cannot be silently
  converted.
- Ledger timing is a numeric column (`0` end, `1` start), the explicit
  native encoding of the Python per-row metadata.
- Duplicate ledger/fee dates are summed (a superset of the source's
  unique-index constraint), documented rather than hidden.

## Test coverage

Four tests: ledger paths, gross fees, uniform/same-day, terminal loss and
validation errors. A first terminal-zero fixture was corrected to
`[100, 110, 0]` because only the final valuation may be zero.

## Gates (current tree)

- Full headless CTest: **586/586 passed**.
- LLVM 22 ASan+UBSan: **586/586 passed**, no reports.
- ImPlot compile/link unchanged.
