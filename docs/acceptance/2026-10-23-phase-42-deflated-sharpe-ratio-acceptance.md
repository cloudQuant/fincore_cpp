# Phase 42 Native Deflated Sharpe Ratio Acceptance

## Scope

Adds `fincore::deflated_sharpe_ratio(returns, risk_free = 0.0,
num_trials = 1, column = "returns")`. Four files changed; no target,
dependency, provider, report, plot, or façade change. Private special
functions stay in `ratios.cpp`.

## Python parity

Source: `fincore/metrics/ratios.py` lines 1020–1162. Clean detached oracle at
`4b8f31b` (`/tmp/fincore-phase42-python.pgJZ2z`): module path asserted,
exit `0`, zero stderr bytes, `FAILURES=0`. Script SHA-256
`c1d8c29267316cbe6013e28c7fc4c16fa1e322e63d42a32be69b30ed0cf69411`; raw
output SHA-256
`fe97296148d06b5230e1954ab8b1eb60134c07f2e120427699f15f8157e679fe`.

Verified: clean-count gate, bias-corrected moments, `sr_hat` fallback at the
`1e-15` deviation threshold, expected-maximum-Sharpe expression, exact
nonpositive-variance 0/1 classes, CDF output, and trial counts below one
normalized to one.

## Test coverage

Eight new ratio tests and the umbrella consumer: signature, trial-count
goldens (1/10/100), trial normalization, clean-count gates, zero and
nonpositive variance, nonfinite returns/risk-free, selectors, NUL and
structural precedence, and lifetime ownership.

## Gates (current tree)

- Full headless CTest: **514/514 passed**.
- LLVM 22 ASan+UBSan: **514/514 passed**, no reports.
- ImPlot compile/link: `fincore_implot` rebuilt successfully; demo not run.

## Limits

Special-function parity is bounded by the declared 1e-9 absolute error budget
(Acklam + Newton refinement); all other arithmetic follows the source
operation order. No `period`/annualization parameters are exposed because the
Python formula does not use them.
