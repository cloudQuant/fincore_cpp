# Phase 62 residual RiskEstimate Acceptance

## Scope

Completes the Phase 62 row with `risk_forecast_config`, `risk_estimate`,
`forecast_var` and `forecast_es` in `include/fincore/risk/models.hpp` and
`src/risk/models.cpp`, plus `tests/unit/risk_models_test.cpp`. Four new
paths and two CMake list entries; no dependency change.

## Python parity

Source: `fincore/risk/models.py`. Clean detached oracle at `4b8f31b`
(`/tmp/fincore-phase68-python.qV1AHd`), Python 3.11.8 / NumPy 1.26.4 /
pandas 3.0.3 / SciPy 1.17.1, exit `0`, empty stderr, clean pre-status.
Script SHA-256
`175fc63da6863bcbb02a73b6fecfc9c015bf093788e23f608e92e7c30bd27d96`; raw output
SHA-256 `6e7102ae430a229e00de63a6b966efbfb0422c948a526172fd69baafb1331eb0`.

- Historical VaR/ES at 0.95/0.99 match exactly (`-0.012382826870905906`,
  `-0.02864328433487541`, `-0.04327816187664599`,
  `-0.04765377824534192`) including 15/3 tail counts, timestamp, status and
  sign convention.
- EVT (GPD) VaR/ES match within 1e-6 at 0.95/0.99; kernel names
  `evt_var`/`evt_cvar` and alpha diagnostics match.
- GARCH VaR/ES match within 2e-5 (`-0.016322022357161695` vs
  `-0.016328834797036625`, etc.); `conditional_var`/`conditional_es`
  kernels, horizon diagnostic and converged status match.
- A single observation returns status `insufficient_data` while still
  computing the estimate and timestamp; NaN observations are dropped before
  the count (2 clean observations → `n_observations = 2`).

## Declared divergences

- `inputs_digest` is the native canonical SHA-256 over timestamps/IEEE-754
  values, not the pandas CSV digest; deterministic and input-sensitive
  natively, not cross-language equal.
- Empty cleaned input raises `input_error` instead of the source's
  `IndexError` from `np.quantile`; duplicate/unsorted indexes raise
  `input_error`.
- The optional `forecast` series is modelled but the adapters never populate
  it, matching the source (which always leaves it `None`).
- Diagnostics are a numeric map plus `kernel`/`note` text; the source uses
  a mixed int/string mapping. `method` is an enum natively and a string in
  Python.

## Test coverage

`risk_models_test.cpp` (6 tests): historical values/tail counts/digest
stability, EVT/GARCH oracle budgets and kernels, insufficient-data and NaN
dropping, empty/invalid method/confidence/horizon/column/duplicate/unsorted
rejections, and digest sensitivity.

## Gates (current tree)

- Full headless CTest: **661/661 passed**.
- LLVM 22 ASan+UBSan: **661/661 passed**, no sanitizer reports.
- ImPlot compile/link unchanged.

## Limits

The adapters are forecast containers, not production risk approval; GARCH
results follow the Phase 61 objective-first fit contract, and no rendering,
runtime registration or regulatory claim is added.
