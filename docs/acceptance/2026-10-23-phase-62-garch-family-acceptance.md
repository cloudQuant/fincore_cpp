# Phase 62 GARCH Family Acceptance

## Scope

Adds EGARCH, GJR-GARCH, `forecast_volatility`, `conditional_var`, and
`conditional_es` with their result types. Three files changed; no dependency
change.

## Python parity

Source: `fincore/risk/garch.py` 295–760. Clean detached oracle at `4b8f31b`
(`/tmp/fincore-phase62-python.KsCo7s`), exit `0`, zero stderr; hashes in the
design. Verified: EGARCH parameters within 2e-3, GJR within 1e-4 (gamma at
the 0 boundary), log-likelihoods, forecast paths, conditional VaR/ES for
EGARCH/GJR, GARCH VaR closed-form composition, invalid-alpha NaN, and error
classes.

## Optimizer note

Both family likelihoods are multimodal; deterministic multi-start sets are
required. A single-start EGARCH run reached loglik 745.4 at the alpha floor;
the multi-start reaches the oracle optimum 749.63. This is recorded in the
design and covered by the fit tests.

## Test coverage

Five family tests plus the Phase 61 tests: tolerance fits, forecasts,
conditional estimates, closed-form composition, invalid alpha, short input,
NUL, and horizon validation.

## Gates (current tree)

- Full headless CTest: **619/619 passed**.
- LLVM 22 ASan+UBSan: **619/619 passed**, no reports.
- ImPlot compile/link unchanged.

## Limits

String model selection is replaced by an enum; only the (1,1) orders are
ported. No `method="OLS"` (the source ignores it).
