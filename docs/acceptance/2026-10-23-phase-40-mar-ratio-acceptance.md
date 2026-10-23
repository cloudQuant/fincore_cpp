# Phase 40 Native MAR Ratio Acceptance

## Scope

Adds `fincore::mar_ratio(returns, annualization = 252, column = "returns")`
to the existing ratios leaf. Files changed: `include/fincore/metrics/ratios.hpp`,
`src/metrics/ratios.cpp`, `tests/unit/ratios_test.cpp`,
`tests/unit/metrics_umbrella_test.cpp`. No target, dependency, or façade
change.

## Python parity

Contract from `fincore/metrics/ratios.py::mar_ratio` (lines 428–475):
raw `< 2` → NaN; `max_drawdown >= 0` → NaN; NaN-only mean times the
annualization factor divided by `|max_drawdown|`; a non-empty clean sample is
required; infinite results map to NaN.

Clean detached oracle at `4b8f31b`
(`/tmp/fincore-phase40-python.Zz93mf`): imported module path asserted,
exit `0`, zero stderr bytes, `FAILURES=0`; script SHA-256
`8b380b39f8ebfb8e3f62a1c52d54b9db58546f8f9182c49ffc08cadb9fd91c73`;
raw-output SHA-256
`0ef7f495a5204774de79590cf05da37d6997f17d1f73b4bacf3077e0a6443850`.
Post-run checkout status identical to pre-run.

Goldens: ordinary `20.160000000000007`, annualization 52
`4.160000000000002`, NaN-mixed `-2.185751579730772e-15`, negative mean
`-85.56004481716624`, zero mean `0.0`; monotone, flat, all-NaN, one-clean,
infinite/NaN annualization, and ±infinity samples all NaN.

## Test coverage

Nine ratio tests and the umbrella consumer cover the exact signature, both
annualization goldens, raw-count and drawdown gates, NaN cleaning,
nonfinite mapping, signed zero, selectors (named/bounded), NUL and structural
precedence, and ownership after frame/selector lifetimes end.

## Gates (current tree)

- Full headless CTest: **499/499 passed**.
- Homebrew LLVM 22.1.8 ASan+UBSan: **499/499 passed**, no reports.
- ImPlot compile/link: `fincore_implot` incremental build succeeded; demo
  not executed, no GUI launched.

## Limits

No `risk_free` or `period` parameters are added; Python's `period` maps to
the native annualization factor. This phase does not add `conditional_sharpe`,
`burke_ratio`, `cal_treynor_ratio`, `m_squared`, `sterling_ratio`,
capture-return metrics, adapters, reports, plots, or release actions.
