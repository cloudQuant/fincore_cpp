# Iteration 2 Close-Out — Performance and Portfolio

**Iteration:** iteration-2-performance-portfolio (Phases 53–59, 57b)
**Oracle pin:** `4b8f31b`
**Head evidence:** 606/606 headless CTest and 606/606 LLVM 22 ASan+UBSan.

## 1. Phase ledger

| Phase | Deliverable | Acceptance record |
|---|---|---|
| 53 | `twr` (new performance leaf) | 2026-10-23-phase-53-twr-acceptance.md |
| 55 | `xirr` + shared IRR solver | 2026-10-23-phase-55-xirr-acceptance.md |
| 54 | `mwr` on the shared solver | 2026-10-23-phase-54-mwr-acceptance.md |
| 56 | `cashflow_adjusted_returns/twr` | 2026-10-23-phase-56-cashflow-adjusted-acceptance.md |
| 57 | `standard_error_of_mean`, `sharpe_standard_error`, `sharpe_confidence_interval` | 2026-10-23-phase-57-inference-acceptance.md |
| 57b | `disclosure_context`, `render_disclosure` | 2026-10-23-phase-57b-disclosures-acceptance.md |
| 59 | liquidity kernels + `trade_record` | 2026-10-23-phase-59-transaction-liquidity-acceptance.md |
| 59b | top positions, notional, stack | 2026-10-23-phase-59b-position-residuals-acceptance.md |
| 59c | `gen_round_trip_stats` | 2026-10-23-phase-59c-round-trip-stats-acceptance.md |
| 59d | exposures + daily transaction bars | 2026-10-23-phase-59d-exposures-acceptance.md |
| 58 | `capacity_config/assessment`, `assess_liquidity` | 2026-10-23-phase-58-capacity-assessment-acceptance.md |

## 2. Public-API audit

`fincore/performance/*.py` and `fincore/portfolio/*.py` contain 39 public
definitions/classes. Thirty-two have same-named native leaves; the remaining
seven are pipeline/adapter helpers with proven native equivalents:

| Python | Native mapping |
|---|---|
| `agg_all_long_short` | the aggregation engine inside `gen_round_trip_stats` (Phase 59c) |
| `groupby_consecutive` | `groupby_consecutive_transactions` (foundation) |
| `add_closing_transactions` | closing is performed inside `extract_round_trips` (foundation) |
| `apply_sector_mappings_to_round_trips` | sector assignment is input to `compute_sector_exposures` / `get_sector_exposures` (Phase 59d) |
| `extract_pos` | the native positions frame with a `cash` column is the boundary contract |
| `make_transaction_frame`, `map_transaction` | ffn-style frame builders map to `trade_record` + the `amount`/`price`/`symbol` frame columns |

## 3. Declared scope limits (recorded, not silent)

- Cashflow FX/currency branch (Phase 56): the native boundary is
  single-currency; currency arguments are not exposed.
- IRR family: non-conventional streams return NaN; deterministic bisection with
  a 1e-15 relative log-rate interval replaces `brentq`.
- Wide portfolio panels must share one timestamp index (stricter than pandas
  inner alignment); unmapped sector symbols are excluded without a warning
  channel.
- The long transaction ledger is `trade_record` values, not a pandas long
  frame.
- No GIPS compliance is claimed; the disclosure template is text support.

## 4. Exit check

- [x] 53, 55, 54, 56, 57, 57b, 58, 59 accepted; residual portfolio capabilities
      landed as 59b–59d.
- [x] Full performance/portfolio ledger reconciled; seven helper names mapped
      with reasons.
- [x] Cross-language numerical and failure goldens recorded per phase.
- [x] Full-suite and sanitizer evidence at the iteration head.
- [x] Public consumer checks: each phase compiles through its public header and
      the master suite.
- [ ] Not claimed: report assembly/runtime registration (Iteration 6), FX
      cashflows, pandas-DataFrame long ledgers, or GIPS compliance.

## 5. Defects found and corrected during the iteration

- Phase 59: ticker-maximum `last_n_days` filtering is inclusive of the cutoff
  date, unlike the strict low-liquidity filter; corrected in Phase 58 with a
  strengthened test and a Phase 59 addendum.
- Phase 59c: the returns table initially consumed PnL values; caught by the
  oracle test and fixed by carrying both series in the group collector.
- Phase 59d: instrument errors in the oracle (percentile passed as 50 instead
  of 0.5; wrong bar iteration) were corrected before the goldens were locked.
