# Phase 45 Native M-Squared Measure Design

## Source contract

`fincore/metrics/ratios.py::m_squared` (lines 748–826): inner alignment;
fewer than two rows → NaN; annualized portfolio return and annualized
portfolio/benchmark volatilities; zero, negative, or NaN portfolio volatility
→ NaN; result `(ann - risk_free) * (bench_vol / port_vol) + risk_free`.

## Native decisions

- Reuses native `annual_return` (CAGR with NaN-as-zero compounding and raw
  count) and `annual_volatility` (NaN-dropping sample deviation), which
  already match the Python kernels.
- Strict inner alignment through `align_on_common_index`; both selectors are
  NUL-checked first.
- No result filtering: the only gate is portfolio volatility.

Fixture discovery note: the first candidate "constant 0.01" series does not
produce zero volatility in Python because the floating-point mean is not
exactly `0.01`; the zero-volatility fixture is therefore an exact-zero series.

## Oracle evidence

Same clean detached run as Phase 44
(`/tmp/fincore-phase44-python.Od17iK`, `4b8f31b`, exit `0`, zero stderr,
`FAILURES=0`; script SHA-256
`617b033470e162960af92281edb63f8361b99867b2389b9e38d7eadc7fe0af29`, raw
output SHA-256
`b7a46f3edef5afbe712dd3f118e56f67b05a8d82db7d82e5d2aff25fe139b442`).

Goldens (plain benchmark): ordinary `1.3665828992102713`, risk-free 0.001
`1.3666687682792138`, annualization 52 `0.1898000296169657`, zero-volatility
NaN, NaN-pair `8.357805155252677`. Control fields: beta
`-0.07438016528925619`, annual return `1.4949531328399344`, portfolio vol
`0.45024437808816675`, benchmark vol `0.41158231254513356`.

## Tests

Six ratio tests plus the umbrella consumer: goldens, volatility and length
gates, NaN pairs, selectors, NUL/structural precedence, and lifetime
ownership.
