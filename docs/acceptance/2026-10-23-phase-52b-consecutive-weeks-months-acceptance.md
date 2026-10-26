# Phase 52b Slice 1 Acceptance — Weekly/Monthly Consecutive Runs

## Scope

Adds the four weekly/monthly consecutive-run leaves. Three files changed; no
dependency or façade change. Phase 52b remains open for the residual ledger
(`r_cubed_turtle`, rolling volatility/Sortino, aligned rolling alpha/beta).

## Python parity

Source: `fincore/metrics/consecutive.py` lines 52–70, 252–330. Clean detached
oracle at `4b8f31b` (`/tmp/fincore-phase52b-python.k4bDem`): module path
asserted, exit `0`, zero stderr bytes. Script SHA-256
`ad4e9678ddb9d68480d1af190644f18213cd46f4502f8669691271903f869b7d`; raw
output SHA-256
`84a01b24661e51faeb12fa27c098c418fdc513e445cc48e438626b14a743261b`.

Goldens: five-month fixture up/down months 2/1 and up/down weeks 8/5;
six-week fixture up/down weeks 2/2; empty input NaN. Weekly buckets are
Sunday-ending and monthly buckets are calendar months, matching pandas
`W`/`ME` resampling.

## Test coverage

Two consecutive tests with goldens, empty inputs, selectors, NUL rejection,
and ownership.

## Gates (current tree)

- Full headless CTest: **566/566 passed**.
- LLVM 22 ASan+UBSan: **566/566 passed**, no reports.
- ImPlot compile/link unchanged.

## Limits

No resample parameters; the source calendar is fixed. This slice does not
claim metrics-domain completion.
