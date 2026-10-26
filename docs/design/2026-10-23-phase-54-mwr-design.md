# Phase 54 Native MWR Design

## Source contract

`fincore/performance/returns.py::mwr` (244–268):

- finite 1-D cashflows;
- `periods=None` → times `0..n-1`; otherwise a positive finite scalar interval
  multiplying the index;
- delegates to `_irr`: at least two cashflows, equal-period aggregation,
  zero-coefficient dropping (times retained), exactly one sign transition
  after aggregation, scaled log-gross NPV bracket and root; non-conventional
  → NaN; min cashflows/length or non-finite values raise.

## Native decisions

- The cashflow frame is positional: the index is ignored, matching the
  Python ndarray/Series positional contract for MWR.
- Input classes map Python `ValueError` to `input_error` with the source
  messages: `at least two cashflows are required`, `cashflows must be
  finite`, `periods must be a positive finite interval`.
- Zero cashflows are dropped while retaining their positional times, so
  `[-1000, 0, 1100]` yields the two-period root `0.048808848170151624` rather
  than the one-period root.
- Reuses the Phase 55 `irr_from_flows` solver unchanged; no second root
  implementation.

## Oracle evidence

Root `/tmp/fincore-phase54-python.a86SSQ`, detached at `4b8f31b`, module
path asserted, exit `0`, zero stderr bytes. Script SHA-256
`077d376f7cd226cd9144ceb212264be213fee1ad11513a6867f7f79b0a8c0108`; raw
output SHA-256
`9f709faafefb6843797e3decc3e48c7d33ca1f77ffa6db1afeb29b1a6d449b6e`.

Goldens: two flows `0.10000000000000002`, half-period
`0.21000000000000005`, three flows `0.05691785736085275`, zero dropped
`0.048808848170151624`, loss `-0.5000000000000002`; non-conventional and
all-negative NaN; single flow, non-finite and invalid periods raise.

## Tests

Three performance tests: conventional goldens, rejection/error classes,
selector/NUL and ownership.
