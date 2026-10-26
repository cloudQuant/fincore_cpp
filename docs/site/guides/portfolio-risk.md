# Portfolio, risk, optimization and simulation

This guide covers the four areas that share one failure model: they own their
inputs, they report what happened instead of returning a plausible-looking
number, and they state their preconditions in the header. The API reference
carries the exact fields; this page explains how to use them together.

## Portfolio inputs own their data

`portfolio_inputs` copies the frames it is given at ingestion, so a later mutation
of the caller's frame never reaches the report. `materialize()` returns fresh
frames on every call: mutating the result does not affect the next call.

The four maps keys are fixed — `returns`, `positions`, `transactions` and
`benchmark_returns`. "Not supplied" and "supplied but empty" are different
states: the first materializes to no key at all, the second to one key holding an
empty frame.

Positions are row-oriented, not date-keyed: two rows may share a timestamp, and
the caller's row order is preserved. The cash column is named `cash` by default,
counts in the allocation denominator and is excluded from the per-asset output
columns and from the asset count.

## Positions, transactions and capacity

| Area | Behaviour worth knowing |
|---|---|
| Round trips | FIFO against a signed price; one block may close and reopen within the same direction |
| Turnover | Uses the per-date book; the first day of a book with no previous state is halved |
| Capacity | Days to liquidate combine position size, rolling mean volume and the tightest single-asset limit; ties resolve to the earliest date, then the first asset |
| Transaction frames | Daily notional adds absolute trade values, so a buy and a sell on the same date reinforce rather than cancel; trading days count distinct dates |

These are accounting rules, not preferences: reports built from the same
transactions will match only if the same convention is used on both sides.

## Risk estimates

`forecast_var` and `forecast_es` return a `risk_estimate` whose `estimate` is
**negative for losses** under the `losses_negative` sign convention. The label is
carried on the result, so a consumer never has to guess the sign.

```cpp
fincore::risk_forecast_config config;
config.method = fincore::risk_forecast_method::historical;
config.confidence_level = 0.99;

const auto var = fincore::forecast_var(returns, config);
if (var.status == fincore::risk_estimate_status::ok) {
    std::printf("1-day 99%% VaR: %.4f\n", var.estimate);
}
```

| Method | What it does | `horizon` | `kernel` |
|---|---|---|---|
| `historical` | Empirical quantile of the cleaned sample | Ignored | empty |
| `evt` | GPD peaks-over-threshold or GEV block maxima, `threshold`/`block_size` optional | Ignored | `evt_var`, `evt_cvar` |
| `garch` | Conditional variance forecast composed over the horizon | Used, at least 1 | `conditional_var`, `conditional_es` |

Preconditions and statuses:

- The index must be strictly ascending and duplicate-free; `NaN` observations are
  dropped first. An empty result set, an unsorted or duplicated index, or an
  invalid configuration raises `input_error`.
- `status` is `ok` when at least two non-`NaN` observations were usable and any
  GARCH fit converged, `insufficient_data` when fewer than two remained, and
  `failed` when a GARCH fit did not converge or failed its stationarity checks.
  A non-converged GARCH fit also fills `note`.
- `inputs_digest` is a lowercase SHA-256 over the cleaned timestamps and values,
  so two runs can be compared without comparing floats.
- Under the GARCH family, `garch_model::egarch` recurses on the **log** variance,
  so a negative `omega` is admissible there while the GARCH and GJR `omega`
  intercepts are positive because they enter a variance.

## Backtesting what you estimated

`backtest_var` pairs a forecast series with realised returns and reports Kupiec
unconditional coverage and Christoffersen independence. `expected_exceptions` is
`observations * (1 - confidence_level)`.

| Status | When |
|---|---|
| `pass` | Coverage tests do not reject the model and the sample is adequate |
| `fail` | A coverage test rejects the model and the sample is adequate |
| `inconclusive` | Fewer than three paired observations **or** fewer than five expected exceptions |
| `experimental` | The ES bootstrap path, whose z-score depends on the resample seed |

That is why zero exceptions at 99% on 250 days is `inconclusive` rather than a
pass: 2.5 expected exceptions is below the five-exception floor. `basel_traffic_light`
turns the exception count into `green`/`yellow`/`red` using binomial bands that
move with both the sample size and the confidence level.

## Walk-forward validation

`walk_forward_var` re-fits on a rolling window and emits one forecast per
observation after the first complete window. The result reports `index`,
`forecast`, `realized`, `refit_timestamps`, `fits`, an optional `backtest`, and a
status:

| Status | Meaning |
|---|---|
| `ok` | Forecasts were produced |
| `insufficient_data` | The series is not longer than the window |
| `unsupported` | The requested target, horizon or quantile is not implemented; `unsupported_reason` names it |

The point-in-time invariant is testable: perturbing returns from some index
onward must leave every earlier forecast bit-identical, move the first affected
fit's mean and forecast by exactly the perturbation, and leave its standard
deviation unchanged. A constant shift moves the level, not the dispersion.

## Optimization

| Entry point | Purpose |
|---|---|
| `optimize_portfolio` (`objectives.hpp`) | One of `max_sharpe`, `min_variance`, `target_return` or `target_risk`, with box, sector and turnover constraints |
| `efficient_frontier` (`frontier.hpp`) | Frontier points plus the minimum-variance and maximum-Sharpe portfolios |
| `risk_parity` (`risk_parity.hpp`) | Weights whose risk-contribution shares match an equal or user-supplied budget; fixed 252-period year, weights bounded to `[1e-6, 1]` while solving |

Constraints are applied by projection, and the result reports diagnostics that
mirror the solved constraint set rather than a convergence flag. One documented
limitation matters when reading results: **`min_variance` without sector
constraints is solved over the unit simplex**, so it returns a long-only book
regardless of `short_allowed` or a negative `min_weight`.

## Simulation

Simulation functions take an explicit seed wherever the result must be
reproducible. Two facts are worth keeping straight:

- A seed reproduces a stream inside this library; it does not promise the same
  numbers as a Python or NumPy call unless the function documents that mapping
  (the MT19937 stream does).
- Antithetic variates mirror the **return path** — `+10%` becomes `-10%` — rather
  than inverting the wealth level, so a mirrored path is `90`, not `100 / 1.1`.

## Failure model summary

| Situation | What you get |
|---|---|
| Missing or mis-typed column, embedded NUL, unsorted or duplicated index | `input_error` |
| Not enough observations for the statistic | `NaN` or `insufficient_data`, never a silent zero |
| A fit that cannot converge | `failed` status with a `note`, or `unsupported` with a reason |
| A result that must be compared or stored | `inputs_digest` (SHA-256) instead of raw floats |
