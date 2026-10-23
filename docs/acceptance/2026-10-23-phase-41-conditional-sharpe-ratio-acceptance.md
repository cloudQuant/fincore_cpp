# Phase 41 Native Conditional Sharpe Ratio Acceptance

## Scope

Adds `fincore::conditional_sharpe_ratio(returns, cutoff = 0.05,
risk_free = 0.0, annualization = 252, column = "returns")`. Four files
changed; no target, dependency, provider, report, plot, or façade change.

## Python parity

Source: `fincore/metrics/ratios.py::conditional_sharpe_ratio` (lines
314–367). Clean detached oracle at `4b8f31b`
(`/tmp/fincore-phase41-python.cGyqNr`): module path asserted, exit `0`,
zero stderr bytes, `FAILURES=0`, post-run checkout clean. Script SHA-256
`3b34fa6cb9cbcaa4689acd76346bda856311c23eda2eb202f266e680df82a4c1`; raw
output SHA-256
`8dd9cbb47a3415c6cfa86783fc797e58b93fc975e1688f51e4874c92819b6902`.

Verified: percentile linear interpolation and equality boundary (`<=`), left
tail in raw order, tail-size gate, NaN propagation, ±infinity classification,
infinite annualization returning ±infinity, and the exact defaults.

## Test coverage

Seven new ratio tests and the umbrella consumer: five-parameter signature,
six finite goldens, count/cutoff/tail gates, duplicate boundaries,
non-finite classes, selectors, NUL/structural precedence, and lifetime
ownership.

## Gates (current tree)

- Full headless CTest: **506/506 passed**.
- LLVM 22 ASan+UBSan: **506/506 passed**, no reports.
- ImPlot compile/link unchanged and re-run.

## Limits

`period` maps to the native annualization factor; no `period` string API.
No other ratio leaf, adapter, report, plot, or release action is added.
