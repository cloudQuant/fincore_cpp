# Phase 63 Slice 2 Acceptance — EVT Hill and GPD Fits

## Scope

Adds `hill_estimator` and `gpd_fit` (MLE/PWM) with EVT enums and owning
result types. Five new/changed paths plus CMake registrations.

## Python parity

Source: `fincore/risk/evt.py` 104–300. Clean detached oracle at `4b8f31b`
(`/tmp/fincore-phase63b-python.agf0sS`), exit `0`, zero stderr; hashes in the
design. Hill and PWM match exactly; MLE reproduces the source's
minimize-log-likelihood behaviour (shape at the 1.0 bound) and is documented
as a source defect instead of being silently corrected.

## Test coverage

Four tests: Hill both tails, PWM goldens, MLE bound behaviour, validation
(threshold/exceedance/tail/NUL).

## Gates (current tree)

- Full headless CTest: **624/624 passed**.
- LLVM 22 ASan+UBSan: **624/624 passed**, no reports.
- ImPlot compile/link unchanged.

## Limits

`evt_var`, `evt_cvar`, `gev_fit`, and `extreme_risk` remain for Phase 63
slice 3; no GEV or ES surface is claimed yet.
