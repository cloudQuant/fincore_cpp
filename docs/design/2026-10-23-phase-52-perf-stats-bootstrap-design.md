# Phase 52 Native Bootstrap Performance Stats Design

## Source contract

`fincore/metrics/perf_stats.py`:

- `calc_distribution_stats` returns mean, median, `np.std` (population,
  ddof=0), 5/25/75/95 linear percentiles, and IQR.
- `perf_stats_bootstrap(returns, return_stats=True)` bootstraps thirteen
  unary metrics with `np.random.RandomState().randint` index resampling and
  projects `mean/median/5%/95%`.
- `calc_bootstrap(func, ...)` is the generic callable-statistic kernel.

## Native mapping and decisions

- `calc_distribution_stats` is reproduced exactly (percentiles use NumPy's
  linear interpolation, including the midpoint lerp rounding branch).
- `perf_stats_bootstrap` evaluates the same thirteen registry entries in the
  oracle's order, reusing every accepted native metric leaf. Resampling uses
  `std::mt19937_64`; **NumPy/C++ sample-stream equality is not claimed** per
  the shared delivery contract. Determinism is native (same seed and sample
  count reproduce identical samples), and kernel parity is checked by
  recomputing each summary from its samples.
- `calc_bootstrap` is mapped to the already accepted
  `simulation::bootstrap_distribution` (callable statistic, seeded engine);
  no generic callback API is duplicated in the metrics layer to avoid a
  metrics→simulation dependency inversion.
- The bootstrap returns `(name, samples, summary)` value records in registry
  order, not a DataFrame; rendering/projection belongs to Iteration 6.

## Oracle evidence

Root `/tmp/fincore-phase52-python.Jw9gmD`, detached at `4b8f31b`, module path
asserted, exit `0`, zero stderr bytes. Script SHA-256
`6e3c0ebd861a2ef890778b8f338785f0a751612b990627350ca473b68ab99b32`; raw
output SHA-256
`ffee15af7681bfcc3961b3a2c032d2ddfc9206b69d74389d1b815fa48a9465b3`.

Distribution goldens for
`[.01,.02,-.05,.03,.01,-.02,.04,.01,.02,-.03]`: mean
`0.003999999999999999` (`0x1.0624dd2f1a9fbp-8`), median `0.01`, std
`0.02690724809414742`, p5 `-0.041`, p25 `-0.0125`, p75 `0.02`, p95
`0.03549999999999999`, IQR `0.0325`. Bootstrap index order: Annual return,
Cumulative returns, Annual volatility, Sharpe ratio, Calmar ratio,
Stability, Max drawdown, Omega ratio, Sortino ratio, Skew, Kurtosis,
Tail ratio, Daily value at risk; projected columns
`mean/median/5%/95%`.

## Tests

Five perf-stats tests: exact goldens, degenerate samples, bootstrap
determinism/order/seed sensitivity, single/empty inputs, selectors/NUL.
