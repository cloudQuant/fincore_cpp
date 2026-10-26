# Phase 65 Risk Backtesting Acceptance

## Scope

Adds `kupiec_lr`, `christoffersen_lr`, `backtest_var`, `backtest_es`, and
their enums/result struct. Five new/changed paths plus CMake registrations;
no dependency change (SHA-256 is internal).

## Python parity

Source: `fincore/risk/backtesting.py`. Clean detached oracle at `4b8f31b`
(`/tmp/fincore-phase65-python.wu18Ed`), exit `0`, zero stderr; hashes in the
design. LR statistics, VaR backtest fields/statuses, and ES
exception/realized/forecast fields match exactly or within 1e-20.

## Declared divergences

- Native `inputs_digest` is SHA-256 over a canonical binary encoding, not
  the pandas CSV bytes; deterministic and input-sensitive natively, not
  cross-language equal.
- Bootstrap z-scores use `mt19937_64`, not NumPy `default_rng`; only native
  determinism is asserted.

## Test coverage

Three tests: exact LR values, VaR statuses/validation, ES determinism and
digest stability.

## Gates (current tree)

- Full headless CTest: **631/631 passed**.
- LLVM 22 ASan+UBSan: **631/631 passed**, no reports.
- ImPlot compile/link unchanged.
