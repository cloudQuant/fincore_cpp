# Phase 41 Native Conditional Sharpe Ratio Design

## Source contract

`fincore/metrics/ratios.py::conditional_sharpe_ratio` (lines 314–367):

- raw length `< 2` → NaN; cutoff outside `(0, 1)` (including NaN) → NaN;
- `cutoff_value = np.percentile(returns, cutoff*100)` — NumPy linear
  interpolation over the full input, NaN-propagating;
- tail is `returns[returns <= cutoff_value]` in original order;
- tail length `< 2` → NaN;
- excess mean = tail mean − risk_free; sample standard deviation (ddof=1);
- `std == 0` or non-finite excess mean/std → NaN;
- result = excess/std × √annualization (no post-result finite filter, so an
  infinite annualization yields ±infinity).

## Native decisions

- The percentile is computed on a sorted copy; the original row order is used
  for the tail and the returned scalar owns all storage.
- NumPy's `_lerp` rounding form is reproduced: `lo + diff*t` below the
  midpoint and `hi - diff*(1-t)` at or above it, so the cutoff matches the
  oracle bit-for-bit on the fixtures.
- Any NaN in the raw column short-circuits to NaN, which is the observed
  NumPy percentile outcome and avoids emitting a computed garbage cutoff.
- No `finite_quotient` helper: Python does not map zero denominators; only
  the pre-division `std == 0` check applies.

## Clean detached oracle

Root `/tmp/fincore-phase41-python.cGyqNr`, detached at `4b8f31b`, module path
asserted, exit `0`, zero stderr bytes (warnings captured in-script),
`FAILURES=0`, post-run checkout clean. Script SHA-256
`3b34fa6cb9cbcaa4689acd76346bda856311c23eda2eb202f266e680df82a4c1`; raw
output SHA-256
`8dd9cbb47a3415c6cfa86783fc797e58b93fc975e1688f51e4874c92819b6902`.

Finite goldens: cutoff 0.2 `-44.89988864128729`, cutoff 0.5
`-7.227274904005733`, cutoff 0.2 with risk-free 0.001
`-46.02238585731947`, annualization 52 `-20.396078054371134`,
duplicate-boundary cutoff 0.6 `36.66060555964672`, and an infinity excluded
from the tail `-33.67491648096547`.

Observed classes: the default cutoff on the ten-value fixture selects a
single observation (`cutoff_value=-0.041`) and returns NaN; all-NaN, NaN
mixed, equal values, subordinate tails, and ±infinity-at-tail cases return
NaN; infinite annualization returns finite-sign infinity.

## Tests (committed with the leaf)

Seven tests in `ratios_test.cpp` plus the umbrella consumer: signature and
defaults, finite goldens, count/cutoff/tail gates, duplicate equality
boundaries, NaN/infinity and annualization classes, selectors, NUL/structural
precedence, and lifetime ownership.
