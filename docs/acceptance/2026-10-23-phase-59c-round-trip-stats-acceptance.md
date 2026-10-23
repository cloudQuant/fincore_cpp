# Phase 59c Round-Trip Statistics Acceptance

## Scope

Adds `gen_round_trip_stats` with the five stat tables. Three files changed;
no dependency change. The helper functions (`agg_all_long_short`,
`groupby_consecutive`, `add_closing_transactions`) are recorded as mappings to
the aggregation engine and accepted native pipeline leaves.

## Python parity

Source: `fincore/portfolio/round_trips.py` 387–470 and `_presentation.py`.
Clean detached oracle at `4b8f31b` (`/tmp/fincore-phase59c-python.oSidR4`):
module path asserted, exit `0`, zero stderr bytes. Script SHA-256
`f2b5bd2aae5805211f20cc443c6cc96454af630233f1c35d3cbb02b212e2db64`; raw
output SHA-256
`8636f6d2e6acabfd3437f82a1f317cd1ef1cd74a69ccb7b0f5c5dd6f2154c26d`.

Verified: all PNL/SUMMARY/DURATION/RETURNS values, group order and omissions,
NaN classes for absent losses, symbol tables in ascending order, duration
days conversion, and the empty-input guard.

## Test coverage

One round-trip test with exact goldens, including the empty frame. An initial
implementation routed the returns family to the PnL vector; the test caught
it and the group collector now carries both series.

## Gates (current tree)

- Full headless CTest: **602/602 passed**.
- LLVM 22 ASan+UBSan: **602/602 passed**, no reports.
- ImPlot compile/link unchanged.

## Limits

Durations are reported in days as double (the oracle converts Timedeltas).
`apply_sector_mappings_to_round_trips` is deferred to the sector-exposure
slice; no callable stats-dictionary API is exposed.
