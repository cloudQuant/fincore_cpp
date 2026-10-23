# Iteration 5 slice 4 Acceptance — PIT Ledger and Factor Portfolio Constructor

## Scope

Adds the causal point-in-time materialization route (`factor/pit.hpp`,
`src/factor/pit.cpp`) and the overlapping factor-portfolio constructor
(`factor/portfolio.hpp`, `src/factor/portfolio.cpp`) with two new test files
and CMake registrations. No dependency or public-signature change to the
accepted factor kernels.

## Phase 78 PIT preparation

- `pit_point` carries `as_of <= known_at <= effective_from` and a finite
  value; `pit_observation` adds the asset and the explicit
  `in_universe` flag. `validate_pit_alignment` rejects temporal inversions
  and non-finite values.
- `materialize_pit_factor(observations, evaluation_dates)` requires strictly
  increasing, duplicate-free evaluation dates and rejects duplicate
  `(asset, as_of, known_at, effective_from)` revisions. For each date it
  selects, per asset, the latest record with both `known_at` and
  `effective_from` at or before that date, ordered by
  `(effective_from, known_at, as_of)`. A selected `in_universe=false` record
  removes the asset; it never leaks the previous in-universe value forward.
- `prepare_pit_factor_data` materializes the ledger, rejects an empty
  materialization, and delegates to the accepted `prepare_factor_data`.
  Full-sample `filter_zscore`, which would make earlier eligibility depend on
  future returns, is rejected.

Oracle fixture (A revision, out-of-universe B, late C; dates 2022-01-04..06)
materializes exactly four rows:

| date | asset | value |
| --- | --- | --- |
| 2022-01-04 | A | 1.0 |
| 2022-01-05 | A | 2.0 |
| 2022-01-06 | A | 2.0 |
| 2022-01-06 | C | 4.0 |

Preparing the same fixture against prices yields two factor rows (single-asset
dates quantize unassigned), matching the Python route.

Declared gaps: timestamps carry the repository's UTC convention rather than a
per-observation timezone; an entirely empty observation ledger returns an
empty series from the materialization kernel (only preparation rejects it);
and no pandas Series wrapper or JSON export is added.

## Phase 82 factor portfolio constructor

- `factor_positions` builds disjoint trades at every factor date and overlaps
  holdings until their expiry, gross-normalizing each timestamp over the
  active trades. Periods are whole business-day labels (`"3D"`, `"1D2D"`);
  expiry follows the Monday-Friday calendar without exchange holidays, and an
  off-session start rolls forward first, matching pandas' default
  `BusinessDay`. The output spans every trade and expiry timestamp and ends
  with an explicit zero row after the last holding expires.
- `factor_cumulative_returns` compounds the selected period returns from one,
  treating a missing observation as zero, matching `cumulative_returns`.
- `build_factor_portfolio_inputs` resamples the compounded curve to every
  calendar day (`last().ffill()`), derives daily returns with a zero first
  observation, resamples and forward-fills the overlapping positions, adds a
  cash overlay (`cash = 1 - sum(assets)`), and emits the long-only
  equal-weight benchmark from `abs(factor)` when the requested label exists.

Oracle fixture (3 dates x 3 assets, factors 1/2/3, periods 1D/3D, pinned
Python `4b8f31b1`):

- `factor_positions(..., "3D")`: 18 rows on 2022-01-03, 04, 05, 06, 07 and
  10 (business-day expiries), weights `A=-0.5, B=0.0, C=0.5` through 01-07
  and zeros on 01-10.
- `factor_cumulative_returns(..., "1D")`:
  `1.01`, `1.0201`, `1.030301`.
- `build_factor_portfolio_inputs(..., "1D")`: returns
  `[0.0, 0.01, 0.01]`; positions 4 dates x (A, B, C, cash), starting
  `-0.5, 0.0, 0.5, 1.0` and ending `0.0, 0.0, 0.0, 1.0`; benchmark
  `[0.0, 0.02, 0.02]`.
- A Friday 2022-01-07 trade with a `1D` holding expires Monday 2022-01-10.

Declared gaps: intraday holding remainders are rejected (only whole-day
components are accepted); no custom trading calendar or holiday inference;
the daily grid uses UTC day boundaries and calendar days rather than a
session calendar; capital-scaled dollar positions and capital curves are not
exposed; explicit weights are accepted only by `factor_positions`, while
cumulative returns and the builder derive factor weights from the accepted
`factor_weights` kernel (the native `factor_returns` has no weights input).
Outputs are typed records rather than pandas frames.

## Oracle evidence

- Pinned Python source: `4b8f31b1b59707480d5db9603704d83bbf7e6946`
  (`/tmp/fincore-phase77-python.NbR0og/source`).
- Interpreter: base Anaconda `python3` 3.11.8.
- Script and raw output: `/tmp/fincore-phase82-python.8bDOgY/oracle.py` and
  `oracle.raw.txt` (outside the repository).
- Fixture JSON SHA-256:
  `11af4f42f7f7552afab3e1d94aaeea9897578eefe76388fd1ac87eeb40091b43`.
- The hash covers the PIT rows, 3D position ledger, cumulative series, daily
  inputs returns/positions and benchmark series used above.

## Tests and gates

`factor_pit_test.cpp` (2 tests: materialization/preparation and fail-closed
validation) and `factor_portfolio_test.cpp` (2 tests: overlapping/daily
constructor and weekend expiry). Full headless CTest: **709/709 passed**.
Sanitizer not re-run for this slice; no visualization is involved
(**NOT_RUN**).
