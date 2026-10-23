# Phase 58 Native Capacity Assessment Acceptance

## Scope

Adds `capacity_config`, `capacity_assessment`, and `assess_liquidity` in a new
`portfolio` leaf, composing the Phase 59 kernels. Five new/changed paths plus
CMake registrations.

## Python parity

Source: `fincore/portfolio/capacity.py`. Clean detached oracle at `4b8f31b`
(`/tmp/fincore-phase58-python.J7tNIo`): module path asserted, exit `0`, zero
stderr bytes. Script SHA-256
`20f8e038125827251506d64c329cf919bf51a44bf4ace0ead02c92710c92e742`; raw
output SHA-256
`9b07c67dc76e507034b3429e057d1f989ae0fcbbe36a8926f1b62091565008e9`.

Verified: composition of the liquidation grid, filtered ticker maximums
(AAA `55.47177006951307` / `13.008130081300814`; BBB `23.089355806972986` /
`6.557377049180328`), low-liquidity rows on 2021-01-05, and the four config
error messages.

## Boundary correction (Phase 59)

Oracle cross-checking exposed that the ticker-maximum date filter is
inclusive of the cutoff (`loc[cutoff:]`), while low-liquidity filtering is
strict. The Phase 59 kernel was corrected and its test strengthened with the
AAA maximum. The Phase 59 acceptance record carries an addendum; no history
was rewritten.

## Test coverage

Two tests: composition goldens and config validation.

## Gates (current tree)

- Full headless CTest: **598/598 passed**.
- LLVM 22 ASan+UBSan: **598/598 passed**, no reports.
- ImPlot compile/link unchanged.

## Limits

No invented capacity sweep or commission/slippage API. Report assembly
remains in Iteration 6.
