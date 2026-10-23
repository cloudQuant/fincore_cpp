# Phase 55 Native XIRR Acceptance

## Scope

Adds `fincore::xirr(cashflows, amount_column = "cashflow")` and the shared
conventional-IRR solver that Phase 54 reuses. Three files changed; no
dependency or façade change.

## Python parity

Source: `fincore/performance/returns.py` 42–268. Clean detached oracle at
`4b8f31b` (`/tmp/fincore-phase55-python.GGx9A6`): module path asserted,
exit `0`, zero stderr bytes. Script SHA-256
`1896193f1ee6cd4690edbbbbe0f2a49a53a5bbcbe8328f22c3ecf15ea2ba0077`; raw
output SHA-256
`4f48c351529e4fe5a19652e38b562e218c85f77719ea75ea4fae9c9155412d8a`.

Verified: same-day aggregation, zero-coefficient dropping, unsorted-date
sorting, `/365` day counting, single-sign-transition certificate, scaled
log-gross NPV, bracket opposition, and `expm1` classification.

## Numerical policy

Deterministic bisection on the scaled objective to a `1e-15` relative
log-rate interval (200-iteration cap). All goldens agree with SciPy `brentq`
at 1e-12; bracket opposition is the only existence certificate, and
non-conventional streams return NaN instead of selecting a root.

## Test coverage

Three tests: conventional goldens, rejection/error classes, selector/NUL and
ownership.

## Gates (current tree)

- Full headless CTest: **579/579 passed**.
- LLVM 22 ASan+UBSan: **579/579 passed**, no reports.
- ImPlot compile/link unchanged.

## Limits

Native time is UTC nanoseconds, so calendar days are UTC days (explicit, not
a silent substitution); mixed-timezone inputs are impossible at this
boundary. Phase 54 builds MWR on this solver.
