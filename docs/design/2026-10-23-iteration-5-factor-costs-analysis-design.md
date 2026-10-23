# Iteration 5 slice 2 Design — Factor Costs and Analysis Assembly

## Source contract

`factor_analysis/costs.py`:

- `FactorCostModel(half_spread_bps, impact_coefficient, max_participation,
  impact_exponent=0.5)` validates finite/non-negative fields and
  `0 < max_participation <= 1`.
- `_normalise_weights` unsets a `(date, asset)` weight Series, fills absent
  pairs with zero, requires gross normalization `sum|w| = 1` (1e-12) on every
  date, and defines trades as `abs(diff(positions))` with the first row equal
  to the absolute initial position.
- `estimate_factor_capacity` requires a finite, strictly positive wide
  dollar-volume panel covering every weight date/asset; per date the bound is
  `min(max_participation * ADV / |trade|)` over traded assets (zero trades do
  not bound), and the result is the minimum across dates with the first
  binding date/asset.
- `apply_factor_costs` aligns finite gross returns to the weight dates,
  requires both borrow panels only when short exposure exists (and rejects
  them otherwise), rejects unavailable shorts and values above capacity, and
  computes `participation = |trade|*value/ADV`, one-way turnover
  `0.5*sum|trade|`, spread `sum|trade|*half_spread_bps*1e-4`, impact
  `sum(|trade|*coef*participation^exponent)`, borrow
  `sum(short*rate)`, total and `net = gross - total`. Every ledger value must
  remain finite.

`factor_analysis/analysis.py: analyze_factor` builds one immutable model from
the prepared table, with IC, weights, factor returns, alpha/beta, quantile
means/SE, turnover, rank autocorrelation and cumulative/grouped/time
aggregated views.

## Native model

- `factor_cost_model`, `factor_capacity_result`, `factor_cost_result` own
  dates/assets/ledger vectors; capacity keeps the full trade matrix and
  binding identity.
- `apply_factor_costs` takes a `std::span` of gross returns, a
  `factor_weight_row` list (date/asset/weight), a wide volume frame and
  optional wide borrow/availability frames (availability is 0/1).
- `analyze_factor(prepared, config)` owns the prepared copy, forward periods,
  IC/mean IC, factor returns/weights, quantile means, alpha/beta, per-quantile
  turnover, rank autocorrelation and a native SHA-256 fingerprint over the
  prepared rows + config flags. It computes each kernel once.

## Numerics and gaps

- Costs match the oracle exactly: capacity `166666.6666666667` (binding
  A/2022-01-03), trades `[0.6,0.4;0.1,0.1;0.5,0.5]`, turnover
  `[0.5,0.1,0.5]`, spread `[0.0002,4e-5,0.0002]`, impact
  `[0.020353792706191448, 0.0017071067811865472, 0.019086034037919898]`,
  net `[-0.010553792706191447, -0.006747106781186547, -0.017286034037919895]`.
  Short borrow `0.0005` per date and net `[-0.009786034037919897, -0.0055,
  0.0015]`.
- Native volume/borrow frames must be sorted; the source reindexes unsorted
  frames. Availability is a 0/1 double panel rather than booleans.
- `factor_analysis_model` is a reduced model: cumulative factor/quantile
  returns, positions, spreads, grouped/time-aggregated results, portfolio
  inputs and event analysis are not yet assembled.
- Overlapping-trade portfolio construction (`portfolio.py`) is not ported in
  this slice; costs consume caller-supplied gross-normalized weights.

## Oracle evidence

`/tmp/fincore-phase77-python.NbR0og/costs_oracle.py` (SHA-256
`a7f4467b1bacd353238931acd2ec48fae106554de2ad3e701ae07b5e3e80553a`) at
`4b8f31b`, exit `0`, empty stderr; raw SHA-256
`2a7924b09776473d98a72203a85a37c16def86c6522c63a01f5031fa5b59e115`.

## Tests

`factor_costs_test.cpp` (4 tests) and `factor_analysis_test.cpp` (2 tests):
capacity, long cost ledger, short borrow ledger, invalid ledgers; model
assembly/fingerprint and empty-input rejection.
