# Phase 44 Native Cal-Treynor Ratio Design

## Source contract

`fincore/metrics/ratios.py::cal_treynor_ratio` (lines 606–700). After inner
alignment: fewer than two rows → NaN; beta at `risk_free`; beta zero,
negative, or NaN → NaN; otherwise `(annual_return - risk_free) / beta` with no
finite filter. `treynor_ratio` forwards to the same implementation and is not
a distinct numerical contract.

## Native decisions

- Strict inner alignment via `align_on_common_index`; both column selectors
  are NUL-checked before any DataFrame access.
- Reuses native `annual_return` (CAGR, NaN-as-zero compounding, raw count)
  and `beta` (finite-pair covariance/variance) unchanged.
- No 2D/DataFrame output form: the native leaf returns one scalar, matching
  the existing native ratio convention.

## Oracle evidence

Root `/tmp/fincore-phase44-python.Od17iK`, detached at `4b8f31b`, module path
asserted, exit `0`, zero stderr bytes, `FAILURES=0`. Script SHA-256
`617b033470e162960af92281edb63f8361b99867b2389b9e38d7eadc7fe0af29`; raw
output SHA-256
`b7a46f3edef5afbe712dd3f118e56f67b05a8d82db7d82e5d2aff25fe139b442`.

Fixtures: returns `[.01,.02,-.05,.03,.01,-.02,.04,.01,.02,-.03]`;
positive-beta benchmark `[.014,.023,-.059,.034,.0135,-.0255,.049,.014,.023,-.036]`;
plain benchmark `[-.04,-.03,-.02,-.01,-.005,.005,.01,.02,.03,.04]`.

Goldens: positive-beta ordinary `1.7905755807724069`, risk-free 0.001
`1.7893778338053357`, annualization 52 `0.2486869244876499`, NaN-pair case
`9.760631792660886`; zero-beta, negative-beta (inverse returns vs plain
benchmark), and short-input cases NaN; alias difference `0.0`.

## Tests

Five ratio tests plus the umbrella consumer: positive-beta goldens, beta
gates, NaN pairs, selectors, NUL/structural precedence, and lifetime
ownership.
