# Phase 52 Native Bootstrap Performance Stats Acceptance

## Scope

Adds `calc_distribution_stats` and `perf_stats_bootstrap` plus their value
types. Three files changed; no dependency or façade change. The generic
`calc_bootstrap` maps to the accepted `simulation::bootstrap_distribution`.

## Python parity

Source: `fincore/metrics/perf_stats.py` lines 147–306. Clean detached oracle
at `4b8f31b` (`/tmp/fincore-phase52-python.Jw9gmD`): module path asserted,
exit `0`, zero stderr bytes. Script SHA-256
`6e3c0ebd861a2ef890778b8f338785f0a751612b990627350ca473b68ab99b32`; raw
output SHA-256
`ffee15af7681bfcc3961b3a2c032d2ddfc9206b69d74389d1b815fa48a9465b3`.

Distribution stats match the oracle to 1e-16 or better, including the
population standard deviation and linear percentiles. Metric names and
registry order match the oracle's DataFrame index. RNG streams are native by
contract; determinism and summary recomputation are asserted.

## Test coverage

Five perf-stats tests: distribution goldens, degenerate samples, bootstrap
determinism/order/seed sensitivity, single-sample and empty input, and
selector/NUL validation.

## Gates (current tree)

- Full headless CTest: **564/564 passed**.
- LLVM 22 ASan+UBSan: **564/564 passed**, no reports.
- ImPlot compile/link unchanged.

## Limits

NumPy/C++ same-seed sample-stream equality is explicitly not claimed. The
bootstrap returns ordered value records, not a DataFrame; the four-column
projection is a renderer concern.
