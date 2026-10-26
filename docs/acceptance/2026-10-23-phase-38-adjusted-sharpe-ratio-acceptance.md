# Phase 38 native adjusted Sharpe ratio acceptance

## Candidate, route, and exact scope

This record accepts the Phase 38 native C++23/DataFrame leaf
`adjusted_sharpe_ratio` on the `dev` branch of the primary checkout.

The reviewed design and executable plan are commit
`0d7dd51f3c8e0a2c6de7ebfa0a76d5aa31f56d9d` (`docs: plan phase 38 adjusted
sharpe ratio`). The implementation was preserved in commit
`e2038e07f2f3de05bb24a68bd5acba6910f01a37` (`wip: phase 38 adjusted sharpe
ratio implementation`) during the workspace move from the isolated
implementation worktree to the `dev` branch; test hardening followed in
`ce9943b` (`test: harden adjusted sharpe parity coverage`).

The route is the next narrow public, non-alias, dependency-closed scalar
ratio: annualized Sharpe corrected by biased skewness/kurtosis, sample-size
dampening, and a two-family clamp. It adds exactly four registered paths:

- `include/fincore/metrics/ratios.hpp`
- `src/metrics/ratios.cpp`
- `tests/unit/ratios_test.cpp`
- `tests/unit/metrics_umbrella_test.cpp`

No target, dependency, facade include, result type, Python adapter, provider,
report, plot, package, or GUI surface is added.

## Python mapping and exact native API

Python exposes `adjusted_sharpe_ratio(returns, risk_free=0.0)`; `period=` is
rejected with `TypeError`. The native API exposes only the meaningful inputs:

```cpp
double adjusted_sharpe_ratio(
    const dataframe& returns,
    double risk_free = 0.0,
    std::string_view column = returns_column);
```

Control flow, in literal order: embedded-NUL selector rejection before every
DataFrame access; `require_double_column`; raw-count `< 4` NaN gate; native
`sharpe_ratio(returns, risk_free, daily_annualization, column)`; only-NaN
Sharpe early return; private NaN-only biased moments in retained row order;
independent NaN-to-zero moment fallback; raw-count dampening
`min(1.0, n/50.0)`; nested `std::max(lower, std::min(upper, adjustment))`
clamp (`[0.9, 1.1]` for raw `n < 20`, `[0.8, 1.3]` otherwise); direct scalar
product.

The private moment helper deliberately diverges from public `skewness` and
`kurtosis`: it filters NaN only, performs no finite filtering, and applies no
clean-count threshold, reproducing SciPy `nan_policy="omit"`, `bias=True`,
Fisher excess kurtosis. Raw four with two clean values therefore yields a
valid adjusted Sharpe rather than the public-stats fallback.

## Clean detached source oracle

A fresh detached Python worktree was created at
`2354bb560788f372502151a8b1b53accf536a6c0` (the design's pinned revision)
under the durable root `/tmp/fincore-phase38-python.mZILzs`:

- pre-run and post-run `git status --ignored --short`: empty and identical
  (`clean_after=yes`);
- interpreter: `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`
  with `PYTHONPATH` and `FINCORE_PHASE38_PYTHON_SOURCE` bound to the detached
  root and `PYTHONDONTWRITEBYTECODE=1`;
- imported module asserted under the detached root:
  `fincore_file=/private/tmp/fincore-phase38-python.mZILzs/source/fincore/__init__.py`;
- exit status `0`; stderr size `0` bytes;
- script SHA-256
  `eb949181316d6ca8d5d20ee0da714ba64e62194f4bc3e0785ade7d36f5c2e7a6`;
- raw-output SHA-256
  `39c640cf7d2a68c98bad661bd6d8fa3ddd1933d898c3f221248c8caead7c085b`.

The oracle asserted every golden embedded in the design and exited with
`FAILURES=0`. All scalar function outputs matched the design's hex-float
goldens exactly, including `raw4_valid2=-0x1.05d0f5132296cp+2`,
`raw4_valid3=0x1.21538e16a68a4p+2`, all eight clamp outputs, the near-zero
variance case `0x1.37135779c8dcbp+56`, and `raw20_clean2=-0x1.374e4f1b6f5f0p+2`.
The informational clamp `multiplier` control fields were recomputed by the
acceptance script with a reconstructed control expression and differ from the
design's recorded reprs by at most one ulp (`1.2999999999999998` vs `1.3`;
`0.8999999999999999` vs `0.9`); the function outputs they describe matched
exactly, and the script compared them with a `1e-15` tolerance.

The design's original `/tmp` capture roots were no longer present on this
machine; this fresh clean run replaces them as acceptance evidence. Fixture
families are those documented in the design; the raw-four clean-count
fixtures are `[nan,nan,nan,nan]`, `[nan,nan,nan,.01]`, `[nan,nan,.01,-.02]`,
and `[nan,.01,-.02,.03]`, and the clamp fixtures are
`resize([.01,.03], n)` / `[-.001]*(n-1)+[.04]` for `n` in 19/20/49/50.

## Test matrix

`ratios_test.cpp` retains `<fincore/metrics/ratios.hpp>` as its only fincore
include and locks the function-pointer type with a direct
`std::is_same_v<decltype(&fincore::adjusted_sharpe_ratio), signature>` (no
`static_cast`). The `metrics_umbrella_test.cpp` consumer keeps the exact
signature assertion and a finite default call through the façade.

The hardening commit adds eleven tests covering: ordinary and risk-free
goldens; raw counts 0–4, all-NaN raw ten, and clean counts 0/1/2/3 with the
two clean-value goldens above; all four upper/lower clamp boundaries;
raw-count dampening on eighteen NaNs plus two clean values; nonconstant
near-zero variance (adjustment fallback equals base Sharpe); zero, positive,
and negative constants with exact NaN/+inf/−inf classes; four-value signed
infinity arrays and NaN/±infinity `risk_free`; duplicate/nonmonotonic labels
and the cancellation-order fixtures
`[-1e16,1.0,1e16,1.0]` vs `[1.0,1e16,1.0,-1e16]`
(`4.860555523805895e-16` vs `0.0`); named and bounded selectors on a frame
with no `returns` column; NUL-before-DataFrame precedence on valid and
malformed frames; empty/missing selectors, wrong type, and index/value
mismatch; and scalar ownership after frame and selector lifetimes end.

## Gate evidence

All evidence was taken from the current tree at test-hardening commit
`ce9943b` (working tree clean):

- Configure/build/CTest (Release, AppleClang): **490/490 tests passed**.
- Homebrew LLVM 22.1.8 ASan+UBSan (`-DFINCORE_ENABLE_ASAN=ON
  -DFINCORE_ENABLE_UBSAN=ON`, Debug): **490/490 tests passed**, no sanitizer
  reports.
- ImPlot compile/link-only (`-DFINCORE_BUILD_IMPLOT=ON`): `fincore_implot`
  built and the `ImPlotAdapterCompile` CMake script test passed. The demo was
  not executed; no GUI was launched.
- Invalid-ImGui expected failure: configure with
  `-DFINCORE_IMGUI_SOURCE_DIR=/tmp/definitely-not-imgui` exits `1`
  (`CMakeLists.txt:74`, configuring incomplete).

## Limits

This is a narrow metric-slice acceptance, not a full parity claim. It does
not authorize merges, rebases, releases, live data, provider calls, reports,
plots, packages, Python adapters, or GUI runtime validation. Later ratio
leaves (`mar_ratio`, `conditional_sharpe_ratio`, `burke_ratio`,
`cal_treynor_ratio`, `m_squared`, `sterling_ratio`, capture returns) remain
out of scope and are scheduled in the iteration roadmap.
