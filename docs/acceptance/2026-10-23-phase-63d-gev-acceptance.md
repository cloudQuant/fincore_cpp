# Phase 63 slice 4 GEV Acceptance

## Scope

Adds `gev_parameters` and `gev_fit`, the GEV branches of `evt_var`,
`evt_cvar` and `extreme_risk`, the `scipy.stats.genextreme.fit` start and
Nelder-Mead path, the regularized lower incomplete gamma / `E1` kernels and
the Gumbel small-tail ES series. `extreme_risk_result` gains optional
`location`, `scale` and `n_blocks`. Three paths changed plus tests and
records; no dependency change.

## Python parity

Source: `fincore/risk/evt.py` (`gev_fit`, `evt_var`, `evt_cvar`,
`extreme_risk`, `_gumbel_expected_shortfall`). Clean detached oracle at
`4b8f31b` (`/tmp/fincore-phase63d-python.Vo3KNQ`), Python 3.11.8 / NumPy
1.26.4 / pandas 3.0.3 / SciPy 1.17.1, exit `0`, empty stderr, clean
pre-status. Script SHA-256
`fa72acd7819421cd3443a3916580ff53027a9ad28595816ee582d7eb3dd1c732`; raw output
SHA-256 `a43031b589ee828b62e8d8eaead65d144c142b933f9b13d1c7bc960d1734e3f0`.

Heavy-tail 1200-day fixture:

- block 20 lower/upper xi `0.7422165388985796` / `0.7458259400937222`, mu
  and sigma matched to ≤2e-15; 60 blocks.
- block 40 lower/upper xi `0.739946573888107` / `0.7434406425221287`;
  30 blocks.
- automatic block (34) xi `0.7390702939614882`, 35 blocks.
- VaR/CVaR lower 0.05 `-0.0589887427734803`/`-0.1894758180300359`, 0.1
  `-0.040607877877553836`/`-0.11872905204186214`; upper 0.05
  `0.05999841670244999`/`0.19677910566036916`; block-40 and automatic
  variants matched at 1e-11.
- `extreme_risk` GEV reports tail index, location and scale; threshold is
  `NaN` and exceedances `0` by construction.

Bounded fixture: block 10/20/40 xi `-0.3296905627790604`,
`0.01106175003969376`, `0.6394385204703907`; VaR `-7.32650498499936e-06`,
`-1.4505465400897338e-05`, `-4.901264065514572e-05`.

The exact source fit path was reproduced after two diagnoses: the moment
start for `c = -0.5` is `(mean, 1)` because the standard variance is
infinite (`loc = mean - 0 * mean`), and SciPy's Nelder-Mead uses distinct
outside/inside contraction points plus a `200*N` evaluation cap. A
multi-start global search was rejected because GEV likelihoods are
multimodal and the source returns the `fmin` local optimum.

## Declared divergences and boundaries

- Bounded fits differ in the last bits of mu/sigma (≤7e-15 absolute,
  ~2e-9 relative) and VaR (≤1.4e-14 absolute), from `fmin` path rounding;
  heavy-tail fits match at 1e-12 (parameters) and 1e-11 (risk values).
- The `|xi| < 1e-10` Gumbel CVaR branch is implemented from the source
  formula but no fitted public fixture reaches it; the bounded block-20
  fit exercises the `xi < 1` incomplete-gamma branch. The Gumbel small-`t`
  series itself is mirrored exactly but not separately exposed.
- `gev_fit` drops NaN only (source behavior) and fails closed on infinite
  block extremes rather than fitting them.
- `threshold` is ignored for `evt_model::gev`, matching the source's GPD-only
  use. `n_blocks` is a `size_t`, not a float.
- No GEV confidence intervals, threshold selection, or rendering are added.

## Test coverage

`risk_evt_test.cpp` (11 tests total, 5 new/replaced): heavy-tail fit
goldens for both tails and block sizes plus the automatic block; VaR/CVaR
at two alphas; `extreme_risk` GEV fields; bounded xi negative/near-zero/
positive shapes; alpha, zero/oversized block size and flat-data validation.
The former "GEV unavailable" expectation is removed.

## Gates (current tree)

- Full headless CTest: **648/648 passed**.
- LLVM 22 ASan+UBSan: **648/648 passed**, no sanitizer reports.
- ImPlot compile/link unchanged.

## Limits

GEV block-maxima estimation is a reference implementation. It does not
certify a regulatory model, select thresholds, provide confidence intervals,
or claim global maximum-likelihood uniqueness for multimodal samples.
