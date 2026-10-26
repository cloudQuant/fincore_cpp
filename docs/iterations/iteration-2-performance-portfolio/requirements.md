# Iteration 2 — Performance and Portfolio: Requirements

**Phases:** 53–59, including 57b. **Status:** Revised plan; NOT_RUN.
**Oracle:** Python `4b8f31b1b59707480d5db9603704d83bbf7e6946`.
The [shared delivery contract](../delivery-contract.md) applies.

## 1. Scope and entry

Port performance measurement and the actual liquidity/capacity surface.
Phase 39 and the accepted native primitives used by each slice are required;
unrelated Iteration 1 residual work need not block this iteration. The title
and formulas do not constitute GIPS compliance or certification.

## 2. Functional requirements

| Phase | Exact Python source/routes | Contract to deliver |
|---|---|---|
| 53 | `performance/returns.py:twr` | Drop NaNs and geometrically chain supplied period returns, `prod(1+r)-1`; no cashflow-event subdivision in this function. |
| 54 | `performance/returns.py:mwr` | IRR for equally spaced cashflows or a positive scalar period interval. Use the shared root contract delivered in 55; aggregate equal periods, require one post-aggregation sign transition, preserve NaN/error outcomes. |
| 55 | `performance/returns.py:xirr` | Irregular dates, aligned unique Series labels versus positional inputs, same-timezone/local-day normalization, same-day aggregation, day-count /365 and log-gross-rate root finding. Freeze the native time representation; no silent UTC-day substitution. |
| 56 | `performance/cashflows.py` | `cashflow_adjusted_returns`, `cashflow_adjusted_twr` and exported timing/model helpers from the ledger. Event timing/sign/valuation rules live here, separate from 53. |
| 57 | `performance/inference.py` | `standard_error_of_mean`, `sharpe_standard_error`, `sharpe_confidence_interval`; sample/ddof, normal-quantile and interval semantics with declared numerical budgets. |
| 57b | `performance/disclosures.py` | `DisclosureContext` and `render_disclosure`; context and deterministic disclosure text are both in scope. This is text output, not an HTML/XLSX renderer. |
| 58 | `portfolio/capacity.py` | `CapacityConfig`, `CapacityAssessment`, `assess_liquidity`; compose transaction liquidation/liquidity helpers and preserve output shape/labels. |
| 59 | `portfolio/transactions.py` liquidity helpers | Map/port `days_to_liquidate_positions`, `get_max_days_to_liquidate_by_ticker`, `get_low_liquidity_transactions` and ledger-required dependencies. Reuse accepted transaction/slippage capabilities where compatible. |

There is no `portfolio/liquidity.py` at this pin. Do not create an invented
max-commission/max-slippage API or capacity sweep and call it Python parity.
The ledger must cover the rest of `portfolio` through historical accepted
mappings or new bounded slices; exact-name absence alone is not a missing
capability when a proven native equivalent exists.

## 3. Numerical and data boundaries

The IRR family is numerical root finding, not tolerance-free arithmetic.
Freeze residual and rate budgets, search domain, stopping criteria and
nonconvergence classifications. The oracle uses a scaled NPV in log-gross
space, not an unconstrained guess-based Newton method. Nonconventional
cashflow streams are rejected with the oracle result class rather than
arbitrarily choosing among roots.

Cashflow inputs are DataFrame-native with explicit date, amount and optional
period representations. Native adaptation of timezone-aware inputs must be
written before implementation; unsupported zones/forms are rejected explicitly
and reflected in coverage. Own all returned models and preserve units
(currency, shares, volume, days, annualization) and labeled outputs.

## 4. Exit

53–59 and 57b accepted, with complete performance/portfolio ledger
reconciliation, mapped historical capabilities, cross-language numerical
and failure cases, candidate full-suite/sanitizer evidence and public
consumer checks. Report assembly and runtime registration remain in
Iteration 6. Do not infer a GIPS compliance claim from formula parity.
