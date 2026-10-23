# Phase 40 Native MAR Ratio Design

## Python contract (`fincore/metrics/ratios.py`, lines 428–475)

```python
if len(returns) < 2: return nan
max_dd = max_drawdown(returns)
if max_dd >= 0: return nan
ann_factor = annualization_factor(period, annualization)   # default 252
clean = returns[~isnan(returns)]
if len(clean) < 1: return nan
result = mean(clean) * ann_factor / abs(max_dd)
if isinf(result): return nan
return result
```

The native signature drops `period` in favor of the effective
`annualization` factor, matching the accepted native calmar convention.
`risk_free` is absent because Python has no risk-free parameter here.

## Native control flow

Implementation in `src/metrics/ratios.cpp`, immediately after `calmar_ratio`:

1. embedded-NUL selector rejection with the exact message;
2. `require_double_column`;
3. raw size `< 2` → NaN;
4. `max_drawdown(returns, column) >= 0.0` → NaN (NaN drawdown falls through
   and yields NaN at the division, as Python does);
5. NaN-only `valid_values`; empty → NaN;
6. `mean(clean) * annualization / std::fabs(maximum_drawdown)`;
7. `std::isinf(result)` → NaN.

No `finite_quotient` helper: it would also map zero denominators and NaN
inputs in ways that differ from the Python flow (Python only maps infinity).

## Clean detached oracle

Detached worktree at `4b8f31b` under `/tmp/fincore-phase40-python.Zz93mf`;
module path asserted under the detached root; exit `0`, zero stderr bytes
(warnings captured in-script), `FAILURES=0`. Script SHA-256
`8b380b39f8ebfb8e3f62a1c52d54b9db58546f8f9182c49ffc08cadb9fd91c73`, raw
output SHA-256
`0ef7f495a5204774de79590cf05da37d6997f17d1f73b4bacf3077e0a6443850`.

Key goldens: `ordinary=20.160000000000007`
(`0x1.428f5c28f5c2bp+4`), annualization 52
`4.160000000000002`, NaN-mixed `-2.185751579730772e-15`,
negative-mean `-85.56004481716624`, zero-mean `0.0` (`0x0.0p+0`),
monotone/flat/all-NaN/one-clean/±infinity → NaN.

## Test matrix

Nine tests in `ratios_test.cpp`: signature + default call, ordinary and
custom-annualization goldens, raw-count and drawdown gates, NaN cleaning,
nonfinite annualization, signed zero and negative mean, nonfinite samples,
named/bounded selectors, NUL/structural precedence, and ownership after
lifetimes. The umbrella consumer gains the adjusted signature check only;
no façade change.

## Risks

- NaN-mixed drawdown parity depends on the accepted native `drawdown_series`
  NaN behavior; the oracle's `-2.185751579730772e-15` is the cross-check.
- Annualization is a free scalar by design; nonfinite values are mapped by
  the `isinf` rule exactly as in Python (NaN annualization propagates NaN).
