# Glossary

Terms used across the guides and the reference. Identifiers stay literal; the
definitions describe what this library means by each term.

{glossary}
Value at risk (VaR)
  A loss threshold not expected to be exceeded at a given confidence level over a
  horizon. Under the `losses_negative` convention the estimate is negative.

Expected shortfall (ES)
  The average loss beyond the VaR threshold, also called conditional VaR. It is
  always at least as severe as the VaR for the same confidence level.

Maximum drawdown
  The largest peak-to-trough decline of a wealth path. `max_drawdown` returns it
  as a negative fraction; `gen_drawdown_table` reports a positive percentage of
  the prior peak.

Sharpe ratio
  Mean excess return divided by its sample standard deviation, scaled by
  `sqrt(annualization)`. Daily annualization is 252.

Sortino ratio
  Mean excess return divided by downside deviation, where only negative excess
  values contribute and squared deviations divide by the usable count.

Calmar ratio
  Annualized return over the absolute maximum drawdown. It is undefined — `NaN` —
  when the series never declines.

Information coefficient (IC)
  The cross-sectional rank correlation between a factor value and its forward
  return on the same date.

Quantile
  A bucket label supplied by the caller, not computed by the analysis. The study
  describes the buckets you declare.

Turnover
  How much of a book changes between observations, measured per date and optional
  lag. The first day of a book with no previous state is halved.

Capacity
  How many days it would take to liquidate a position given its size, rolling mean
  volume and the tightest single-asset limit.

Round trip
  A matched open-and-close of a position, matched FIFO against a signed price. One
  block may close and reopen within the same direction.

Point-in-time (PIT)
  A join that only uses data that was both effective and known on the evaluation
  date, so a revision cannot leak backwards into history.

Brinson attribution
  A decomposition of excess return into allocation, selection and interaction
  terms. The BHB and Fachler variants place the interaction differently.

Alpha and beta
  The intercept and slope of a regression of excess returns on benchmark excess
  returns, paired on shared timestamps.

GARCH family
  Conditional-variance models: GARCH(1,1), EGARCH(1,1) and GJR-GARCH(1,1) with an
  asymmetric shock term. EGARCH recurses on the log variance.

Extreme value theory (EVT)
  Tail modelling with a generalised Pareto distribution over threshold exceedances
  (GPD) or a generalised extreme-value distribution over block maxima (GEV).

Efficient frontier
  The set of portfolios with the lowest volatility for each level of expected
  return, plus the minimum-variance and maximum-Sharpe points.

Risk parity
  Weights whose risk-contribution shares match an equal or user-supplied budget,
  solved with weights bounded to `[1e-6, 1]` and a fixed 252-period year.

Digest and fingerprint
  A SHA-256 over a canonical serialization. A digest over inputs answers "same
  inputs?"; the factor fingerprint hashes inputs, a report digest covers the
  document.

Artifact bundle
  The named outputs a renderer returns: metadata with `backend` and
  `report_digest`, plus artifacts such as `figure`, `file` or
  `axis:<section>.<series>`.

Inconclusive
  A backtest verdict meaning the sample is too small to decide: fewer than three
  paired observations or fewer than five expected exceptions.
