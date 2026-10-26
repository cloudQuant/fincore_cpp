# Iteration 5 slice 2 Acceptance — Factor Costs and Analysis Assembly

## Scope

Adds `factor/costs.hpp` + `src/factor/costs.cpp` (capacity and gross-to-net
ledger) and `factor/analysis.hpp` + `src/factor/analysis.cpp` (compute-once
model assembly with a native fingerprint), plus two test TUs. Four new paths
and CMake/test registrations; no dependency change.

## Oracle parity

Cost oracle `/tmp/fincore-phase77-python.NbR0og/costs_oracle.py` (SHA-256
`a7f4467…`, raw `2a7924b…`) at `4b8f31b`, exit `0`, empty stderr.

- Capacity: maximum `166666.6666666667`, by-date
  `[166666.6666666667, 1000000.0000000002, 200000.0]`, binding
  A/2022-01-03, trades `[[0.6,0.4],[0.1,0.1],[0.5,0.5]]`.
- Cost ledger: turnover `[0.5,0.1,0.5]`, spread
  `[0.0002,4e-5,0.0002]`, impact
  `[0.020353792706191448, 0.0017071067811865472, 0.019086034037919898]`,
  total `[0.020553792706191447, 0.001747106781186547, 0.019286034037919897]`,
  net `[-0.010553792706191447, -0.006747106781186547,
  -0.017286034037919895]`, participation `[[0.06,0.02],[0.01,0.005],
  [0.05,0.025]]`; `net = gross - total` exactly.
- Short borrow: `0.0005` per date, net `[-0.009786034037919897, -0.0055,
  0.0015]`.
- Invalid gross dates, over-capacity value, short without borrow panels,
  `max_participation=0` and non-gross-normalized weights fail with
  `factor_data_error`.

`analyze_factor` assembles the accepted kernels once and reproduces the
performance oracle values (IC first `0.9999999999999999`, mean IC
`-0.1783333`, alpha/beta `0.1626165`, quantile mean `0.002015254`, rank
autocorrelation `0.9999999999999998`), with a 64-hex native fingerprint that
is stable for identical inputs and changes with the config/data.

## Declared divergences and gaps

- Wide volume/borrow frames must be sorted; availability is a 0/1 double
  panel, not booleans.
- The native `factor_analysis_model` is reduced: cumulative/quantile
  returns, positions, spread tables, grouped/time-aggregated views,
  portfolio inputs and event analysis are not assembled yet.
- Overlapping-trade `portfolio.py` construction is not ported; costs consume
  caller-supplied gross-normalized weights.
- Research, tears and report rendering remain open Iteration 5 phases.

## Test coverage

`factor_costs_test.cpp` (4 tests): capacity, long ledger, short borrow
ledger, invalid ledgers. `factor_analysis_test.cpp` (2 tests): model
assembly/fingerprint and empty-input rejection.

## Gates (current tree)

- Full headless CTest: **687/687 passed**.
- Sanitizer: not re-run in this slice (no claim).
- ImPlot compile/link unchanged.

## Limits

The cost ledger is a reference accounting tool, not an execution model.
Reduced-model gaps and the unported portfolio constructor remain visible.
