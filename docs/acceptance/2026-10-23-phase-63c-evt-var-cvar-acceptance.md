# Phase 63 Slice 3 Acceptance — EVT VaR/CVaR (GPD)

## Scope

Adds `evt_var`, `evt_cvar`, `extreme_risk`, and the `evt_model` enum for the
GPD path. Three files changed; no dependency change.

## Python parity

Source: `fincore/risk/evt.py` 33–700. Clean detached oracle at `4b8f31b`
(`/tmp/fincore-phase63b-python.agf0sS`), exit `0`, zero stderr; hashes in the
design. VaR/CVaR at alphas 0.05/0.1, extreme-risk fields, and validation
match within 5e-4 (parameters inherit the MLE tolerance band).

## Test coverage

Three tests: VaR/CVaR goldens, extreme-risk fields, alpha/GEV rejection.

## Gates (current tree)

- Full headless CTest: **627/627 passed**.
- LLVM 22 ASan+UBSan: **627/627 passed**, no reports.
- ImPlot compile/link unchanged.

## Limits

GEV (`gev_fit` and the `gev` model branches) is explicitly rejected for now
and remains Phase 63 slice 4; `block_size` is inert for GPD.
