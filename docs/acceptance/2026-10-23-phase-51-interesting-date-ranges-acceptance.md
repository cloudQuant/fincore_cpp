# Phase 51 Native Interesting Date Ranges Acceptance

## Scope

Adds `fincore::extract_interesting_date_ranges(returns, column)` returning
ordered owning `named_returns` frames. Three files changed; no dependency or
façade change.

## Python parity

Source: `fincore/metrics/timing.py` lines 281–307 and
`_historical_periods.py`. Clean detached oracle at `4b8f31b`
(`/tmp/fincore-phase51-python.brssWS`): module path asserted, exit `0`, zero
stderr bytes. Script SHA-256
`52443772f9bab3bbeded4f601c490c62bddb22c7980dd37f751bf5801b987aad`; raw
output SHA-256
`47e34fd12bf4433b826711f2661d447c260443a5d36e6b29e94c1a98805ee4db`.

Recorded divergence: the Python tz-aware path returns zero ranges because
its naive-label slice raises inside a caught block; the native UTC index
applies the documented calendar. Goldens are from the tz-naive working path
and match exactly (window names, order, row counts, boundary values).

## Test coverage

Four timing tests: pinned order/boundaries, New Normal and empty input,
selectors/NUL precedence, and ownership after source destruction.

## Gates (current tree)

- Full headless CTest: **559/559 passed**.
- LLVM 22 ASan+UBSan: **559/559 passed**, no reports.
- ImPlot compile/link unchanged.

## Limits

The divergence from the tz-aware Python behaviour is documented, not hidden;
no tz-normalization parameter is exposed.
