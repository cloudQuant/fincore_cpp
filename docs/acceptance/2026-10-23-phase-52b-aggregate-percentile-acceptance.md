# Phase 52b Slice 3 Acceptance — Aggregate Consecutive Stats and Alpha Percentile Rank

## Scope

Adds `consecutive_stats` (six-run aggregate) and `alpha_percentile_rank`
(peer-universe percentile). Six files changed; no dependency or façade
change.

## Python parity

Sources: `metrics/consecutive.py` 69–138; `metrics/alpha_beta.py` 805–860.
Clean detached oracle at `4b8f31b` (`/tmp/fincore-phase52b3-python.UKksTC`):
module path asserted, exit `0`, zero stderr bytes. Script SHA-256
`5816ded8c37bf3114bb3d130f2cd882b026b844056d426a5360731658303bc98`; raw
output SHA-256
`259283123fa6155fea0024b082ac3247569741f50bc29f6d4ee1670e0e45e2cb`.

Goldens: six-run aggregate 3/1, 2/0, 1/0 on the ten-day January-2020
fixture; empty input NaN; alpha percentile rank `0.6666666666666666` (with
and without risk-free adjustment); short strategy and empty peer span NaN.

## Test coverage

Two tests: dated-fixture composition and empty/NUL validation; percentile
goldens, risk-free invariance, short strategy, empty peer span (span
operation), and ownership. The peer span includes a two-row peer that must be
skipped, and the fixture index is dated because integer timestamps collapse
into a single week.

## Gates (current tree)

- Full headless CTest: **572/572 passed**.
- LLVM 22 ASan+UBSan: **572/572 passed**, no reports.
- ImPlot compile/link unchanged.

## Limits

Peer frames are supplied as a span of owning frames; no list-of-Series
adapter is introduced. No percentile interpolation is needed (rank is a
strict count).
