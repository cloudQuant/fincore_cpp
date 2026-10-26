# Phase 19 GBM-from-Returns Design

**Date:** 2026-08-12

## Decision

Phase 19 adds the calibrated convenience counterpart to Python
fincore.simulation.paths.gbm_from_returns. It composes the accepted
DataFrame-native parameter estimator from Phase 17 with the accepted native
GBM propagator from Phase 18. The result remains a named DataFrame with time
as rows and path names as double columns; this phase does not introduce a
Monte Carlo object, a result wrapper, or another random-number implementation.

The public API belongs in the already-exported paths leaf:

~~~cpp
dataframe gbm_from_returns(
    const dataframe& returns,
    std::size_t horizon = 252U,
    std::size_t path_count = 1000U,
    std::size_t periods_per_year = 252U,
    std::optional<std::uint64_t> seed = std::nullopt,
    std::string_view column = returns_column);
~~~

It delegates once to estimate_simulation_parameters(returns,
periods_per_year, column), then delegates once to
geometric_brownian_motion. The public estimator already owns the native rules
for genuine-double column validation, bounded selector ownership, NaN-only
omission, infinity rejection, at-least-two retained returns, sample standard
deviation, and annualization. Phase 19 does not duplicate or alter those
rules.

## Source-compatible transformation

Python estimates annual values mu and sigma, then uses the following arguments
when it invokes its GBM leaf:

~~~text
dt          = 1.0 / frequency
mu_daily    = mu / frequency
sigma_daily = sigma / sqrt(frequency)
T           = horizon / frequency
~~~

The native wrapper preserves that source transformation verbatim, with
periods_per_year in place of frequency. It therefore calls the accepted native
GBM leaf as:

~~~cpp
geometric_brownian_motion(1.0,
                          parameters.drift / periods_per_year,
                          parameters.volatility / sqrt(periods_per_year),
                          static_cast<double>(horizon) / periods_per_year,
                          1.0 / periods_per_year,
                          path_count,
                          seed);
~~~

This is deliberately source-compatible even though the Python variable names
are potentially misleading. It does not reinterpret the already accepted
Phase 17 annualized parameter contract.

The source performs int(T / dt) inside its GBM leaf. Because T and dt are
separately rounded binary doubles, the source can produce fewer rows than the
requested horizon; for constant 0.01 returns, horizon=3, frequency=5, and
seed=42, Python returns two values 0.002002001334000303 and
0.004008010677341911. Native preserves this
double-transform-plus-truncation behavior through its public GBM delegation.
It must not silently force an exact-horizon output.

## Native DataFrame representation

Python first makes a path-by-time price array, then returns
price_paths[:, 1:] - 1.0. The native result remains transposed:

- one genuine-double column per path, named path_0, path_1, and so on in
  numeric order;
- the initial price row is removed;
- each remaining price value becomes the cumulative return price - 1.0;
- ordinal index values preserve the actual physical GBM steps
  {1, ..., n_steps}; and
- when source-compatible truncation yields zero simulated steps, index and
  every named double path column are empty.

Rows, labels, and values are copied into the returned DataFrame. The
historical-return frame and temporary price-path frame may be mutated or
destroyed after the call.

## Randomness, errors, and exclusions

An explicit seed is forwarded to the existing native GBM leaf. The same
native mt19937_64 plus Box-Muller stream gives repeatable native output; no
test or documentation claim equates it to NumPy default_rng samples. Omitted
seed retains the existing local random_device behavior.

path_count == 0 is rejected by the existing GBM leaf and periods_per_year == 0
by the existing parameter estimator. All additional division, square-root,
total-duration, and price - 1.0 operations must be finite; allocation/index/
name failures remain input_error. Since inputs are historical returns, no new
zero/negative-initial-price policy is needed: the wrapper always starts the
delegated price path at 1.0.

This phase excludes changes to parameter estimation, raw GBM propagation,
antithetic variates, Latin-hypercube sampling, bootstrap, MonteCarlo,
SimResult, simulation VaR/CVaR, correlation/stress functionality,
optimization, providers, reports, full NumPy RNG parity, GUI runtime, and
packaging/release work.

## Source oracle and verification

The required local Anaconda source oracle is:

~~~sh
cd /Users/yunjinqi/Documents/new_projects/fincore
/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python -c '
import numpy as np
from fincore.simulation.paths import gbm_from_returns
for values, horizon, paths, frequency in (
    (np.array([0.01, 0.01, 0.01, 0.01]), 4, 2, 4),
    (np.array([0.01, 0.01, 0.01, 0.01]), 3, 1, 5),
    (np.array([0.01, 0.01, 0.01, 0.01]), 0, 2, 4),
):
    result = gbm_from_returns(values, horizon=horizon, n_paths=paths,
                              frequency=frequency, seed=42)
    print((horizon, paths, frequency), result.shape, result.tolist())
'
~~~

It yields:

~~~text
(4, 2, 4) (2, 4)
[[0.0025031276057951857, 0.005012520859401182,
  0.0075281954445340915, 0.010050167084168393],
 [0.0025031276057951857, 0.005012520859401182,
  0.0075281954445340915, 0.010050167084168393]]
(3, 1, 5) (1, 2)
[[0.002002001334000303, 0.004008010677341911]]
(0, 2, 4) (2, 0) [[], []]
~~~

Tests use the deterministic zero-volatility fixture for cross-language
numbers, direct public-leaf parity for nonzero seeded behavior, source order
and ownership, the two-truncation boundary, structural/embedded-NUL errors,
and finite/intermediate failure paths. A later public-only simulation-umbrella
consumer proves the existing paths inclusion exposes this new member without
adding another umbrella include.
