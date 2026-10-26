# Phase 50 Native Drawdown Table and Rank APIs Acceptance

## Scope

Adds `get_top_drawdowns`, `gen_drawdown_table` (with `drawdown_table_row`),
and `max_drawdown_period` to the existing drawdown leaf. Three files
changed; no dependency or façade change.

## Python parity

Source: `fincore/metrics/drawdown.py` lines 223–438. Clean detached oracle at
`4b8f31b` (`/tmp/fincore-phase50-python.ouAOjp`): module path asserted,
exit `0`, zero stderr bytes. Script SHA-256
`97b939aeaf2cc314193d98fba8aabaf264ddd634d4e5227bcc04dcaefb98c353`; raw
output SHA-256
`2c4c9992910c6be4e0035e1c2e78ab5ce8970010c0cda1067a17db3015153b75`.

Verified: iterative ranking order, peak/recovery boundaries, unrecovered
truncation, padded table rows, net-percent formula, inclusive business-day
duration, and max-period classes. Enumeration note: the rest of the Python
drawdown surface (underwater series, recovery days/weeks/months, second/third
maxima, detailed episodes) was already present and is reused unchanged.

## Test coverage

Five drawdown tests: oracle-exact windows (three ranked, one unrecovered),
table nets and durations plus padded monotone rows, unrecovered table,
period classes, and selector/NUL/ownership validation.

## Gates (current tree)

- Full headless CTest: **555/555 passed**.
- LLVM 22 ASan+UBSan: **555/555 passed**, no reports.
- ImPlot compile/link unchanged for these value types.

## Defect found during hardening

The first business-day implementation used 86.4e9 ns/day (1000× too small)
and returned 5001/2144 instead of 6/4. Corrected to 86.4e12; the exact
duration goldens now guard it.

## Limits

Dates are returned as timestamps, not formatted strings; rendering is
deferred to Iteration 6. The table pads to `top` rows exactly like the
pandas frame.
