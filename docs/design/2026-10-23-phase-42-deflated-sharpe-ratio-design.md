# Phase 42 Native Deflated Sharpe Ratio Design

## Source contract

`fincore/metrics/ratios.py` lines 1020–1162. The Python implementation:

1. drops NaN only; requires at least three clean observations;
2. subtracts `risk_free` per observation;
3. `sr_hat = mean(excess)/std(excess, ddof=1)` when the sample standard
   deviation exceeds `1e-15`, otherwise `0`;
4. bias-corrected sample skewness and excess kurtosis (zero when the sample
   is too short or the deviation is zero), with ordinary kurtosis defined as
   excess + 3;
5. `sr_var = 1 - gamma3*sr_hat + (gamma4-1)/4*sr_hat^2`;
6. `N = max(num_trials, 1)`; `N <= 1` gives `sr_star = 0`; otherwise the
   Euler-gamma expected-maximum expression with `norm.ppf`;
7. nonpositive `sr_var` returns exactly `1` or `0` according to
   `sr_hat > sr_star`;
8. otherwise the standard-normal CDF of the deflated z statistic.

## Private special functions

`normal_cdf` uses `0.5*erfc(-z/sqrt(2))`. `normal_ppf` uses Acklam's rational
approximation plus one Newton refinement step against the CDF. The declared
error budget is 1e-9 absolute on the returned probability; the oracle
tolerances in tests are 1e-9 for values near one and tighter absolute
tolerances for small probabilities. No public API and no dependency change.

## Oracle evidence

Root `/tmp/fincore-phase42-python.pgJZ2z`, detached at `4b8f31b`, module path
asserted, exit `0`, zero stderr bytes (warnings captured in-script),
`FAILURES=0`. Script SHA-256
`c1d8c29267316cbe6013e28c7fc4c16fa1e322e63d42a32be69b30ed0cf69411`; raw
output SHA-256
`fe97296148d06b5230e1954ab8b1eb60134c07f2e120427699f15f8157e679fe`.

Goldens: trials 1 `0.6548645197338817`, trials 10
`7.615960789032017e-06`, trials 100 `3.161694901787256e-13`, risk-free 0.001
`0.6192133374972018`, three clean values `0.6343497465427029`, all-equal
`0.5`, nonpositive variance `0.0` / `1.0`.

Branch coverage note: the `sr_var <= 0` branch is exercised by
`[-0.2,-0.2,-0.2,-0.1,-0.1]` (returns 0) and `[0.2,0.2,0.2,0.1,0.1]`
(returns 1). The control print for the ordinary trials-10 case records
`sr_hat`, `g3`, `g4`, `var`, `z`, and the SciPy CDF.

## Tests

Eight ratio tests plus the umbrella consumer: signature, trial goldens,
trial-count normalization, clean-count gates, zero/nonpositive variance,
nonfinite inputs, selectors, NUL/structural precedence, and lifetime
ownership.
