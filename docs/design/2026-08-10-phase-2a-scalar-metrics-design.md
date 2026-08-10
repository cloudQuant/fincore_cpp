# fincore C++ Phase 2A Scalar Metrics Design

## Purpose and boundary

Phase 2A extends the accepted C++23/DataFrame foundation with the next group
of scalar, return-series analytics from the local Python implementation at
`/Users/yunjinqi/Documents/new_projects/fincore`. It keeps the established
public representation: a one-column `hmdf::StdDataFrame<std::int64_t>` is a
time series, every materialized series result is another DataFrame, and paired
metrics explicitly align timestamps before calculation.

The work is intentionally narrower than all of Phase 2. It provides the
numeric primitives needed by commonly used scalar risk, ratio, descriptive,
and timing metrics before adding calendar grouping, multi-asset holdings,
transactions, Bayesian models, performance attribution, and the broad
Empyrical facade. It therefore improves the native analytics path without
claiming full Python API parity.

## In-scope public surface

1. `metrics/basic.hpp`: a native `frequency` enum, annualization lookup and
   an index-preserving `adjust_returns` operation. There is no pandas-style
   dynamic `to_pandas` analogue because public inputs are already DataFrames.
2. `metrics/risk.hpp`: downside risk, tail ratio, tracking error, residual
   risk, and normal variance-covariance VaR in addition to the accepted
   historical VaR/CVaR APIs.
3. `metrics/ratios.hpp`: excess Sharpe, information, omega, stability, and
   capture ratios. Paired ratio and capture APIs use the existing common-index
   finite-pair boundary rather than pandas `Series` outer-index arithmetic.
   Existing Sharpe, Sortino and Calmar behavior remains unchanged.
4. `metrics/stats.hpp`: skewness/excess kurtosis, serial correlation, win/loss
   rates, safe Pearson correlation, relative win rate, CAPM R-squared, and
   tracking difference.
5. `metrics/alpha_beta.hpp`: a paired alpha/beta result and up/down
   alpha/beta. Calendar-year `annual_beta` remains deferred because the
   Phase 2A integer timestamp boundary deliberately has no calendar grouping
   semantics.
6. `metrics/timing.hpp`: scalar Treynor–Mazuy, Henriksson–Merton, market timing
   return, and Cornell timing regressions using a private fixed-size solver.
7. `fincore/metrics.hpp`: a small native umbrella header that exposes only
   implemented DataFrame-native APIs; it is not a compatibility wrapper for
   Python's dynamic `Empyrical` class.

## Semantic rules

- Structural violations (missing/wrongly typed columns, impossible frequency
  enum) throw `fincore::input_error`. Undefined numerical results follow the
  Python metric contract and return IEEE `NaN` unless the original formula
  intentionally yields signed infinity.
- Scalar calculations use finite or pairwise-finite observations exactly as
  their Python oracle does. Each public function documents whether `NaN` is
  ignored, preserved, or makes the result undefined; no global missing-data
  policy is inferred.
- A pair-dependent metric uses `align_on_common_index` first, then removes
  only pairs whose required observations are non-finite. It never pairs by
  position after different timestamp sequences are supplied. This is the
  explicit native contract for Phase 2A paired ratios as well: it avoids the
  pandas `Series` outer-index/implicit-`NaN` behavior while preserving the
  Python formulas on their valid aligned data domain.
- Internal moment, percentile, and small least-squares helpers are private to
  the library. They accept spans/vectors but never replace `dataframe` in the
  public API.
- Timing regressions use a pivoted small dense solve for the fixed two- and
  three-coefficient models. Singular designs and insufficient observations
  return `NaN`; a general matrix package is deliberately not introduced.

## Python oracle mapping

| Native group | Python reference |
| --- | --- |
| Basic frequency/adjustment | `fincore.metrics.basic` |
| Extended risk and ratios | `fincore.metrics.risk`, `fincore.metrics.ratios` |
| Descriptive/benchmark statistics | `fincore.metrics.stats` |
| Paired alpha/beta and timing | `fincore.metrics.alpha_beta`, `fincore.metrics.timing` |

Golden values are generated in focused tests from deterministic fixtures and
cross-checked with the Python reference only where a formula's edge behavior is
ambiguous. The test corpus must record the relevant Python fixture, rather
than treating a C++ reimplementation as its own oracle.

## Explicitly deferred work

The following remains for later Phase 2 slices: calendar-year aggregation and
date-range APIs (`yearly.py`), deep drawdown tables, consecutive-run dates,
multi-column position/transaction schemas, round trips, bootstrap/Bayesian
models, performance attribution, `perf_stats`, and the complete C++ native
facade/inventory for `empyrical.py`. Optimization, simulation, advanced risk
models, reports, data providers, tearsheets, packaging, and final parity
remain in their already documented later phases.
