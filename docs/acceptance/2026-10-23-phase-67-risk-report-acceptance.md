# Phase 67 Risk Spec and Validation Report Acceptance

## Scope

Adds `include/fincore/risk/specs.hpp` (`risk_model_spec` validation with the
`horizon` field, target/tail names), `include/fincore/risk/report.hpp`
(`risk_validation_report`, event/refit/diagnostics values,
`build_risk_validation_report`, deterministic `to_json`), and Phase 65
residual calibration helpers (`expected_exception_count`,
`basel_traffic_light`, `es_calibration_score`, status names) in
`backtesting.hpp`. `walk_forward_var` now validates through the shared spec,
reports `horizon != 1` as structured unsupported, and records the input
observation count. Thirteen paths changed/added plus the design record; no
dependency change.

## Python parity

Source: `fincore/risk/specs.py`, `report.py`, `calibration.py`,
`diagnostics.py`. Clean detached oracle at `4b8f31b`
(`/tmp/fincore-phase67-python.H7e4mx`), module resolved under the detached
root, Python 3.11.8 / NumPy 1.26.4 / pandas 3.0.3 / SciPy 1.17.1, exit `0`,
empty stderr, clean pre-status. Script SHA-256
`5679fa2b95cf81c74b0ceb06db955183ada60ee223a2d7283fa13dc1a4df4f89`; raw output
SHA-256 `68d9ae1be876f8020127d45c68b46a640296bf295f675935d6814f84b41842da`.

Matched fields:

- Spec validation: all sixteen rejection cases raise `input_error`; accepted
  values/enums match (`forecast_target`, `confidence_level`, `horizon`,
  `distribution`, `tail`, `sign_convention`, `window`, `refit_cadence`,
  `model_version`).
- Normal report (300-day fixture, window 50, cadence 5, confidence 0.99):
  250 events, 50 refits; first event `2021-02-20T00:00:00`, forecast
  `-0.015891935566622144`, realized `-0.00428182669496151`; last forecast
  `-0.016807926786139734`; first refit mean `0.000564337061249735`, std
  `0.007073865784005689`; diagnostics `normal`/`normal_ppf`/50/5/250; backtest
  0 exceptions, Kupiec `5.025167926750753`, inconclusive, green.
- Historical report (window 100, cadence 10, confidence 0.95): 200 events,
  20 refits; first forecast `-0.009674773367508039`, last
  `-0.009982395371701296`; historical refit parameters expose only
  `forecast`, `n_observations`, `quantile_method: "weibull"`; 19 exceptions,
  Kupiec `6.823658870683893`, fail, yellow.
- Small normal report (12-day fixture, window 5, cadence 2, confidence 0.9):
  7 events, 4 refits, first forecast `0.000513085814624476`; backtest 3
  exceptions, expected `0.6999999999999998`, Kupiec `5.0976812174202815`,
  Christoffersen `3.81908500976888`, inconclusive, yellow; `n_observations`
  renders as `5.0`.
- Unsupported/insufficient reports: empty events/refits, `backtest: null`,
  reason text, and insufficient `{n_observations: 50, window: 50}`.
- Calibration: expected exceptions `2.500000000000002`,
  `50.00000000000004`, `0.0`; traffic-light boundaries n=1 → 0/1, n=10 →
  1/3, n=250 c=0.99 → 5/10, n=250 c=0.975 → 11/17, n=1000 c=0.95 → 62/77;
  ES scores `-0.5`, NaN for a zero forecast, `4.333333333333334` for an
  empty tail.

The native small-report JSON was diffed against the Python `to_json()`
artifact after replacing the two input digests: the structure, key order,
indentation, string escaping, event/refit/diagnostic blocks and number
formatting are byte-identical; only numeric values whose inputs differ by
last-bit `sin`/`erfc` rounding differ (e.g. forecast
`0.0005130858146244749` vs `0.000513085814624476`). A synthetic formatting
test pins `50.0`, `-0.0`, `1e-05`, `1e+16`, `1234567890123456.8`,
`0.0001`, `1.2345678901234566e-07`, `-1500000000000000.0`, six/nine-digit
fractional timestamps and a pre-epoch timestamp.

## Declared divergences and boundaries

- `inputs_digest` is the Phase 66 native canonical SHA-256 recipe, not
  pandas-CSV bytes; the report carries it verbatim and claims no
  cross-language digest equality.
- `timestamp_timezone` is always `null`: the native index has no timezone
  metadata. Python's tz-aware IANA/offset token is therefore not reproduced
  (the oracle confirms `"UTC"` and `+00:00` for a localized index).
- `timestamp_index_name` is an explicit optional builder argument because
  the native DataFrame has no index-name field; `walk_forward_var` produces
  unnamed indexes, matching the source path.
- `write_json` is not ported as file I/O; callers write the returned
  deterministic string. `report/risk.py` projection and rendering remain
  Phase 88a/91.
- Report validation recomputes the backtest from the path. The native
  backtest result does not retain `aligned_index`; recomputation over the
  same index substitutes for the source's index-equality check.
- The normal fit-provenance check evaluates mean/std → forecast with
  `detail::inverse_standard_normal_cdf` at `rtol 1e-12, atol 1e-15`; all
  oracle fixtures pass.
- Existing Phase 65 results (`kupiec_pvalue`, `christoffersen_pvalue`) differ
  from SciPy in the last bits; the report serializes the native values.

## Test coverage

`risk_report_test.cpp` (8 tests): spec matrix, normal/historical/small
reports, exact unsupported JSON, insufficient report, named-index projection,
tamper rejection (confidence, exceptions, missing fits, cadence, path value,
fit mean, non-empty empty-status), float/timestamp/escaping/UTF-8 and
non-finite rejection. `risk_backtesting_test.cpp` gains calibration
thresholds/scores; `risk_walk_forward_test.cpp` gains the horizon status and
observation count.

## Gates (current tree)

- Full headless CTest: **644/644 passed**.
- LLVM 22 ASan+UBSan: **644/644 passed**, no sanitizer reports.
- ImPlot compile/link unchanged.

## Limits

This is a compute-only report artifact. It does not render HTML/XLSX, does
not project into the shared `ReportDocument` schema (Phase 88a/91), does not
reproduce pandas timezone tokens, and does not certify a regulatory model or
a GIPS/risk-approval claim.
