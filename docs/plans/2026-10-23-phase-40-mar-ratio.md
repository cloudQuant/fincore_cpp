# Phase 40 Native MAR Ratio Plan

**Goal:** Add the native `mar_ratio` scalar: annualized NaN-only mean return
divided by the absolute maximum drawdown, with the Python contract's
non-positive-drawdown gate and nonfinite-result mapping.

**Baseline:** `dev` head after Phase 39 acceptance (oracle pin `4b8f31b`).
Python source: `fincore/metrics/ratios.py::mar_ratio` (lines 428–475).

**Files:** `include/fincore/metrics/ratios.hpp`,
`src/metrics/ratios.cpp`, `tests/unit/ratios_test.cpp`,
`tests/unit/metrics_umbrella_test.cpp`.

## Native API

```cpp
double mar_ratio(const dataframe& returns,
                 double annualization = daily_annualization,
                 std::string_view column = returns_column);
```

Python's `period`/`annualization` pair maps to the single meaningful
`annualization` input, matching the native calmar convention. No `risk_free`,
`period`, alias, overload, or result type.

## Exact control flow

```text
NUL selector check (exact message) -> require_double_column
raw size < 2 -> NaN
max_drawdown(returns, column) >= 0 -> NaN        (NaN drawdown continues)
NaN-only clean values; empty -> NaN
result = mean(clean) * annualization / fabs(max_drawdown)
isinf(result) -> NaN
```

Reuse native `max_drawdown`; do not recompute drawdown or change the
drawdown leaf. Python's `isinf` mapping is the only nonfinite normalization.
Order, duplicate/nonmonotonic labels, scalar ownership, and structural
validation precedence follow the established ratios-leaf rules.

## Tests

Exact signature `double (*)(const dataframe&, double, std::string_view)`;
ordinary `20.160000000000007` and annualization-52 `4.160000000000002`
goldens; raw 0/1; monotone and flat no-drawdown NaN; all-NaN and one-clean
NaN; NaN-mixed `-2.185751579730772e-15`; infinite/NaN annualization NaN;
signed-zero result; negative-mean `-85.56004481716624`; ±infinity samples;
named/bounded selectors; NUL precedence, structural errors; lifetime.

## Gates

Clean detached Python oracle at `4b8f31b` (exit 0, zero stderr, zero
FAILURES); clean-tree build; full CTest; LLVM 22 ASan+UBSan; ImPlot
compile/link re-run; acceptance record + README ledger.
