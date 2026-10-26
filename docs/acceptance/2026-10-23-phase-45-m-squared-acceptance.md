# Phase 45 Native M-Squared Measure Acceptance

## Scope

Adds `fincore::m_squared(returns, benchmark, risk_free = 0.0,
annualization = 252, returns_column, benchmark_column)`. Four files changed;
no new dependency or façade change.

## Python parity

Source: `fincore/metrics/ratios.py::m_squared` (lines 748–826). Clean
detached oracle at `4b8f31b` shared with Phase 44
(`/tmp/fincore-phase44-python.Od17iK`): exit `0`, zero stderr bytes,
`FAILURES=0`. Script/raw hashes as recorded in the Phase 44 acceptance.

Verified: plain-benchmark goldens, risk-free and annualization variants,
exact-zero portfolio-volatility NaN gate, short input, and NaN-pair
alignment. The constant-0.01 candidate is documented as *not* zero-volatility
because floating-point mean error produces a tiny positive deviation.

## Test coverage

Six ratio tests plus the umbrella consumer: goldens, volatility/length gates,
NaN pairs, selectors, NUL/structural precedence, and lifetime ownership.
Focused suite at the batch head: 96/96.

## Gates (batch head, shared with Phase 44)

- Full headless CTest: **537/537 passed**.
- LLVM 22 ASan+UBSan: **537/537 passed**, no reports.
- ImPlot compile/link unchanged for this scalar.

## Limits

Scalar output only. No `alignment` string policy or timezone normalization
parameters are exposed; the native inner alignment is the only behaviour.
