# Phase 63 slice 4 Design — GEV Block Maxima

## Source contract

`fincore/risk/evt.py`:

- `gev_fit(data, block_size=None, tail="lower")` drops NaNs only, trims to
  `n_blocks = n // block_size` complete blocks, uses negated block minima for
  the lower tail and block maxima for the upper tail, and delegates to
  `scipy.stats.genextreme.fit` on the positive magnitudes.
- The default `block_size` is `int(sqrt(n))`.
- SciPy's fit start: adjusted skewness `g`; shape `c = 0.5` if `g < 0` else
  `c = -0.5`; moment-based loc/scale using the standard genextreme mean and
  variance. When the standard variance is infinite, `fit_loc_scale` sets
  `scale = 1` after `loc = mean - 0 * mean` (so `loc` is the sample mean).
  A support-compatibility check replaces the start with a data-derived loc
  and `scale = 1` when the moment start lies outside the data support.
- `genextreme.fit` runs `fmin` (Nelder-Mead) with `xtol = ftol = 1e-4` and
  `maxiter = maxfun = 200 * 3 = 600` on `_penalized_nnlf`: unsupported
  observations are dropped and charged `_LOGXMAX * 100` each, while the
  `n * log(scale)` term still counts every observation.
- `evt_var(..., model="gev")`: `t = -log1p(-alpha)`;
  `var_mag = mu - sigma*log(t)` if `|xi| < 1e-10`, otherwise
  `mu + (sigma/xi)*(t**(-xi) - 1)`; sign by tail.
- `evt_cvar(..., model="gev")`: Gumbel ES
  `mu + sigma*(gamma + exp(-t)*log(t) + E1(t))/alpha` (small-`t` series
  branch below `1e-5`), or
  `mu + (sigma/xi)*(gammainc(1-xi,t)*gamma(1-xi)/alpha - 1)` for `xi < 1`;
  `xi >= 1` raises.
- `extreme_risk(..., model="gev")` reports VaR, CVaR, tail index, location
  and scale (no GPD threshold/exceedance fields).

## Native decisions

- `gev_parameters {xi, mu, sigma, n_blocks}` is the public result;
  `extreme_risk_result` gains optional `location`, `scale`, `n_blocks`
  (GPD results leave them empty and keep `threshold`/`n_exceedances`).
- `gev_fit` filters NaN only (the source does not drop infinities; an
  infinite block extreme makes the fit fails closed).
- The private `scipy_style_nelder_mead` reproduces SciPy's simplex rules,
  including the distinct outside/inside contraction points and the
  `200*N` evaluation cap. The shared `minimize_nelder_mead` is not changed:
  it uses a single contraction point, and altering it would shift accepted
  GPD/GARCH fits.
- The objective reproduces `_penalized_nnlf`, including the per-bad-point
  `log(max double) * 100` charge and the full-length scale term. `xi = -c`
  is returned.
- Special functions: regularized lower incomplete gamma by series below
  `a+1` and continued fraction above; `E1` by series for `x < 1` and
  continued fraction otherwise; Euler–Mascheroni as a compile-time
  constant. The Gumbel small-`t` ES series is the source's sixteen-term
  form.
- `threshold` is ignored for `evt_model::gev` (the source forwards it only
  to the GPD branch).

## Oracle evidence

Root `/tmp/fincore-phase63d-python.Vo3KNQ`, detached at `4b8f31b`, module
resolved under the detached root, Python 3.11.8 / NumPy 1.26.4 / pandas
3.0.3 / SciPy 1.17.1, exit `0`, empty stderr, clean pre-status. Script
SHA-256 `fa72acd7819421cd3443a3916580ff53027a9ad28595816ee582d7eb3dd1c732`;
raw output SHA-256
`a43031b589ee828b62e8d8eaead65d144c142b933f9b13d1c7bc960d1734e3f0`.

Heavy-tail fixture (`sign * max(|2u-1|, 1e-3)^-0.25 * 0.01`,
`u = (i*7919 mod 1000)/1000`, 1200 observations):

- block 20 lower: xi `0.7422165388985796`, mu `0.019478399594106605`,
  sigma `0.003635672307968767`, 60 blocks; VaR/CVaR at 0.05
  `-0.0589887427734803`/`-0.1894758180300359`; at 0.1
  `-0.040607877877553836`/`-0.11872905204186214`.
- block 20 upper: xi `0.7458259400937222`, mu `0.01930859111583602`,
  sigma `0.003717379847844264`; VaR/CVaR at 0.05
  `0.05999841670244999`/`0.19677910566036916`.
- block 40 lower: xi `0.739946573888107`, mu `0.02324936792257617`,
  sigma `0.003302944806895195`, 30 blocks; VaR/CVaR at 0.05
  `-0.0589820102613349`/`-0.1756985937778102`.
- block 40 upper: xi `0.7434406425221287`, mu `0.023363420083466202`,
  sigma `0.0031492320730879706`; CVaR at 0.1 `0.10987980436883901`.
- Default block (34): xi `0.7390702939614882`, mu `0.022447653077878327`,
  sigma `0.0050653905775546165`, 35 blocks; VaR/CVaR at 0.05
  `-0.07715171961133763`/`-0.25508073875774867`.
- `extreme_risk` lower: location `0.019478399594106605`, scale
  `0.003635672307968767`, 60 blocks; upper location
  `0.01930859111583602`, scale `0.003717379847844264`.

Bounded fixture (`((-0.02 + 0.04*i/399)^3)`, 400 observations): block 10
xi `-0.3296905627790604`, sigma `3.324470463056271e-06`, VaR
`-7.32650498499936e-06`; block 20 xi `0.01106175003969376`, VaR
`-1.4505465400897338e-05`; block 40 xi `0.6394385204703907`, sigma
`5.382188123834289e-06`, VaR `-4.901264065514572e-05`.

## Numerics and divergences

- Native fits reproduce the oracle parameters to ~1e-12 for the heavy-tail
  fixture (max observed difference ~2e-15 on xi) and VaR/CVaR to ~1e-11.
- Bounded fits differ in the last bits of mu/sigma (≤7e-15 absolute,
  ~2e-9 relative) and VaR (≤1.4e-14 absolute); tests use 1e-10 on xi and
  1e-12 on the tiny outputs.
- The xi≈0 Gumbel CVaR limit is implemented from the source formula, but no
  public fitted fixture lands within `|xi| < 1e-10`; the bounded block-20
  fit exercises the `xi < 1` incomplete-gamma path instead.
- The optimizer relies on the moment start: GEV likelihoods are multimodal,
  and the source returns the local `fmin` optimum rather than a global
  maximum. The native path reproduces that local optimum; it does not
  promise global-MLE equality.
- `n_blocks` is a size_t; Python's fit dictionary carries the same integer
  count.

## Tests

Five added/replaced tests in `risk_evt_test.cpp`: heavy-tail fits, VaR/CVaR
goldens, `extreme_risk` location/scale/blocks, bounded shapes, and
alpha/block-size/flat-data validation (the previous "GEV unavailable"
expectation is removed).
