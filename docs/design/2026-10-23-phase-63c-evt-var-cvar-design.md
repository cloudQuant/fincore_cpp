# Phase 63 Slice 3 Design — EVT VaR/CVaR (GPD)

## Source contract

- `_default_pot_threshold`: required exceedances
  `max(10, floor(alpha*n)+1)`; retain the 90th percentile when it leaves that
  many strict exceedances; otherwise use the required order statistic and
  `nextafter(selected, -inf)`; too few tail observations raise.
- `evt_var` GPD: fit at the selected/explicit threshold; reject
  `alpha > n_exceed/n_total`; magnitude
  `u - beta*log(ratio)` for `|xi| < 1e-10`, else
  `u + (beta/xi)*(ratio^(-xi)-1)`; return-space sign by tail.
- `evt_cvar` GPD: `var + beta` (exponential) or
  `var + (beta + xi*(var-u))/(1-xi)` for `xi < 1`; otherwise raises
  `CVaR infinite for xi >= 1`.
- `extreme_risk` GPD: VaR, CVaR, tail index, threshold, exceedance count,
  row index alpha.

## Native decisions

- Enum model/tail; block_size is accepted but inert for GPD (parallel to the
  source signature), and GEV branches raise an explicit
  "GEV model is not available in this build" error for this slice.
- Automatic threshold arithmetic copies the source's percentile and strict
  exceedance conventions, including the `nextafter` step.
- The source's MLE sign defect (slice 2) is inherited: on strongly
  heavy-tailed fixtures `xi` reaches 1 and CVaR legitimately raises. The
  acceptance fixture is chosen with `xi ~ 0.72` so the CVaR path is
  exercised; the raising case remains tested on the slice-2 fixture through
  the shared MLE behaviour.

## Oracle evidence

Root `/tmp/fincore-phase63b-python.agf0sS`. Oracle 3 script SHA-256
`eb5ab7eb7c90f939ba3519fb68eba780501ef68c342019bd4bdee624715394ab`; output
values: VaR 0.05 `-0.020959731329854844`, CVaR 0.05
`-0.024413095253991088`, VaR 0.1 `-0.019012853078465397`, CVaR 0.1
`-0.021805352664204795`, extreme risk tail index `0.7170359451104547`,
threshold `0.020912878988368272`, 21 exceedances.

## Tests

VaR/CVaR goldens at two alphas with ordering, extreme-risk fields, and
alpha/GEV validation.
