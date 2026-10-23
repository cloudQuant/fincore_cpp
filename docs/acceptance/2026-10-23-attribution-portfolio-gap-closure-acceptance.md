# Attribution and Portfolio Gap Closure Acceptance

## Scope

Closes the final three unresolved function gaps in the program ledger:
`calculate_idiosyncratic_risk`, `agg_all_long_short` and `align_and_warn`.
After this slice the ledger has **zero unexplained function gaps** (243
exact-name, 36 renamed, 51 deferred, 21 non-port out of 351).

## Idiosyncratic risk

`calculate_idiosyncratic_risk(returns, factor_data, market_beta=nullopt,
risk_free=0)` reproduces the source's first-asset market-beta behaviour: it
fits OLS of the first asset's excess returns on MKT/SMB/HML/RMW/CMA, takes the
MKT coefficient, and applies that single beta to every asset when computing
`var(asset) - beta^2 * var(market)` with population variances; negative
idiosyncratic variance clamps to zero. Supplying `market_beta` skips the fit,
in which case only MKT is required.

Oracle (SHA-256 `cd80801dbff581c854f996d21bb8e35f4b25f4d5dade390f644f0c9860a5e091`):
beta `0.30357142857141906`; specific volatilities `0.008701654177846832`,
`0.0041562412088803`, `0.005319973567995668`. Missing FF5 columns without an
explicit beta fail with `input_error`.

## Long/short round-trip aggregation

`agg_all_long_short(round_trips, column, statistics)` applies caller-supplied
`std::function` statistics to the full, long (`long=true`) and short sample
sets and returns parallel vectors per statistic. An absent side yields NaN.
This is the typed analogue of the source's arbitrary `stats_dict`.

Oracle: for pnl `[0.10,-0.04,-0.08,0.03,0.05,-0.01]` with three long and three
short trades, count/mean/std/min/max are
`6 / 0.008333333333333333 / 0.0649358657959272 / -0.08 / 0.1` (all),
`3 / -0.02 / 0.05567764362830022 / -0.08 / 0.03` (short) and
`3 / 0.036666666666666674 / 0.07094598884597587 / -0.04 / 0.1` (long).

## Attribution alignment

`align_and_warn` accepts a typed `factor_loading_table` (dates x assets x
factors) plus returns/positions/factor-return frames. It drops position
assets without loadings (except `cash`), truncates dates missing loadings from
all three frames, returns warnings instead of emitting them, and fails when no
position has loadings. Transaction turnover checks and stacked-Series
positions are not represented (declared).

Tests cover the 1/3 missing-asset ratio and one missing date, preserved factor
values, the no-coverage failure and malformed loading dimensions.

## Gates

New `AttributionFamaFrench.*`, `AttributionPerformance.*` and
`RoundTrips.AggregatesLongShortStatistics` tests. Full headless CTest:
**721/721 passed**. Sanitizer not re-run; visual NOT_RUN.
