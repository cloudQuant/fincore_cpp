# Phase 62 residual design — RiskEstimate and forecast adapters

## Source contract

`fincore/risk/models.py`:

- `RiskEstimate(method, confidence_level, horizon, sign_convention,
  estimate, forecast_timestamp, inputs_digest, forecast=None,
  diagnostics, status)`: method in `historical|evt|garch`, confidence in
  `(0,1)`, horizon ≥ 1, an optional sorted/duplicate-free DatetimeIndex
  forecast, and status `ok|insufficient_data|failed`.
- `forecast_var` / `forecast_es` validate the returns index (duplicates and
  ascending order), drop NaNs, and dispatch:
  - historical: NumPy linear quantile at `1-confidence`; ES is the mean of
    returns `<=` the quantile (or the quantile when the tail is empty).
  - evt: `evt_var`/`evt_cvar` with forwarded model/tail/threshold/block
    arguments.
  - garch: `conditional_var`/`conditional_es` with the model, horizon and
    forwarded options; `converged` comes from the fit.
- Status is `insufficient_data` for fewer than two clean observations,
  `failed` for a nonconverged GARCH fit, otherwise `ok`. The timestamp is
  the last clean index label; the digest is SHA-256 of the cleaned frame's
  CSV; diagnostics record `n_observations`, `n_tail_observations`, or
  `kernel`/`alpha`/`horizon` plus a nonconvergence note.

## Native decisions

- `risk_forecast_config` owns method, confidence, horizon, GARCH model, EVT
  model/tail/threshold/block size; `forecast_var`/`forecast_es` take an
  explicit column (default `returns`).
- `risk_estimate` owns method/confidence/horizon/sign/estimate/timestamp/
  digest, an optional forecast series (the adapters never populate it, as in
  the source), numeric diagnostics, kernel text, note and status.
- Historical quantiles reuse `detail::linear_percentile` (NumPy linear).
  EVT/GARCH build a cleaned frame and call the accepted kernels.
- `inputs_digest` is a versioned native SHA-256 over big-endian timestamps
  and IEEE-754 values, not pandas CSV bytes; it is deterministic and
  input-sensitive natively. No cross-language equality is claimed.
- Empty cleaned input raises `input_error` instead of the source's
  `IndexError` from `np.quantile` on an empty array; duplicates and
  unsorted indexes raise `input_error`.

## Oracle evidence

Root `/tmp/fincore-phase68-python.qV1AHd`, detached at `4b8f31b`, Python
3.11.8 / NumPy 1.26.4 / pandas 3.0.3 / SciPy 1.17.1, exit `0`, empty
stderr, clean pre-status. Models script SHA-256
`175fc63da6863bcbb02a73b6fecfc9c015bf093788e23f608e92e7c30bd27d96`; raw
output SHA-256
`6e7102ae430a229e00de63a6b966efbfb0422c948a526172fd69baafb1331eb0`.

Sine+shock 300-day fixture: historical VaR/ES at 0.95 are
`-0.012382826870905906` / `-0.02864328433487541` (15 tail points); at 0.99
`-0.04327816187664599` / `-0.04765377824534192` (3 tail points). GARCH
VaR/ES at 0.95 are `-0.016328834797036625` / `-0.020477017599398892`; at
0.99 `-0.02309418266356838` / `-0.02645818569485599`.

Heavy-tail 1200-day fixture: EVT VaR/ES at 0.95 are
`-0.017763810864321817` / `-0.023324642899925165`; at 0.99
`-0.026566608938194285` / `-0.0332668744363668`.

Tolerance budgets: historical exact; EVT 1e-6; GARCH 2e-5 (the GARCH fit
itself follows the Phase 61 objective-first contract); statuses and
diagnostics exact.

## Tests

`risk_models_test.cpp` (6 tests): historical oracle values/tail counts,
digest stability; EVT and GARCH oracle values/kernels; single-observation
status, NaN dropping, empty rejection; invalid method/confidence/horizon/
column/index; digest input sensitivity.
