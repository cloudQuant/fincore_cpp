# Iteration 1 — Metrics Completion: Requirements Specification

**Iteration ID:** iteration-1-metrics-completion
**Phase range:** Phase 38 acceptance (carry-over) + Phase 39 (oracle re-pin) + Phases 40–52
**Source of truth:** fincore Python `dev@4b8f31b` (v0.5.2.dev0), primarily `fincore/metrics/`
**Status:** Draft

## 1. Purpose and Scope

Bring the `fincore.metrics` domain to full public-API parity with the Python
oracle, completing the metric kernel that every later iteration (performance,
risk, factor analysis, reports) builds on. This iteration also carries two
mandatory preparatory items: closing out Phase 38 and re-pinning the parity
oracle from `2354bb5` to `4b8f31b`.

## 2. Entry Conditions

- Phase 38 (`adjusted_sharpe_ratio`) working-tree implementation is complete
  and committed through the established exact-index gate.
- The 206 unpushed implementation commits have been pushed to origin as
  backup (user-authorized).
- Doc checkout vs impl worktree topology decision recorded (README).

## 3. Functional Requirements

### 3.1 Preparatory phases

| Phase | Requirement |
|---|---|
| 38 (close-out) | Accept `adjusted_sharpe_ratio` against a clean detached Python oracle run: raw-count gates, dampening, 19/20/49/50 clamp boundaries, zero-std NaN/±inf classifications, order-sensitivity fixtures per the committed design doc. |
| 39 (oracle re-pin) | Rebuild parity goldens against fincore `dev@4b8f31b` in a fresh detached checkout; re-run the full C++ suite; classify every golden drift. Expected: none for already-ported metrics (fincore changes since `2354bb5` touched factor_analysis/report), but this must be **measured, not assumed**. Design doc records old/new pin SHAs and any drift items. |

### 3.2 Ratio and statistic phases (one narrow public API per phase)

| Phase | Public API | Python source | Requirement summary |
|---|---|---|---|
| 40 | `mar_ratio` | `fincore/metrics/ratios.py` | Calmar-style annual-return / max-drawdown ratio; reuse native `max_drawdown`; define negative/zero-drawdown result classes from oracle. |
| 41 | `conditional_sharpe` | `fincore/metrics/ratios.py` | Sharpe over returns above VaR; reuse native `value_at_risk`; lock the VaR-method default parity. |
| 42 | `deflated_sharpe_ratio` | `fincore/metrics/ratios.py` | Requires normal CDF/inverse — private stats helper only, no public dependency. Lock trial-count and variance-input semantics from oracle. |
| 43 | `burke_ratio` | `fincore/metrics/ratios.py` | Drawdown-root denominator; empty/degenerate-drawdown classes from oracle. |
| 44 | `treynor_ratio` | `fincore/metrics/ratios.py` | Reuse native beta; benchmark-column validation parity. |
| 45 | `modigliani_ratio` (M²) | `fincore/metrics/ratios.py` | Leverage by benchmark vol ratio; reuse annualization helpers. |
| 46 | `sterling_ratio` | `fincore/metrics/ratios.py` | Average-drawdown denominator; window semantics from oracle. |
| 47 | capture-return slices | `fincore/metrics/ratios.py` | Up/down/up-down capture **return** (distinct from existing capture ratios); up/down market masks locked to oracle behavior. |
| 48 | `hurst_exponent` | `fincore/metrics/stats.py` | Rescaled-range analysis; window and aggregation policy must be transcribed from oracle, not invented. |
| 49 | `stutzer_index` | `fincore/metrics/stats.py` | Assumes LPM1-based index; sign and aggregation conventions from oracle. |
| 50 | drawdown table machinery | `fincore/metrics/drawdown.py` | Top-N drawdown table, underwater series, recovery days/weeks/months, 2nd/3rd max drawdown enrichment. One phase for the table assembly (not per column). Returns a value-type table struct; no rendering. |
| 51 | `extract_interesting_date_ranges` | `fincore/metrics/timing.py` | Crisis-period definitions (`_historical_periods.py`) ported as static data; name and range parity; UTC-ns boundary semantics. |
| 52 | `perf_stats` bootstrap variants | `fincore/metrics/perf_stats.py` | Bootstrap-augmented aggregate stats; deterministic seeding contract; reuse existing simulation/bootstrap primitives (Phases 20–22). |

### 3.3 Per-phase standard functional requirements (apply to every phase)

- Exact-signature declaration only: `double f(const dataframe&, <scalars>, std::string_view)` pattern; no aliases, no overloads beyond the established default-argument form, no facade includes, no automatic-input conveniences.
- Embedded-NUL selector rejection before any DataFrame access with the established exact message; `require_double_column` structural validation precedence preserved.
- NaN/±inf result classes, raw-vs-clean sample counts, zero-variance and degenerate-input classes all locked to oracle hex-float or exact-class goldens.
- Input order, duplicate/nonmonotonic index labels, and scalar ownership after input destruction preserved. No sort, align, deduplicate, mutate, or retain of input storage.

## 4. Non-Functional Requirements

- **Parity policy:** hex-float exact parity (all items in this iteration are closed-form statistics; the tolerance policy of the roadmap §2 does not apply here).
- **Gates per phase:** clean-tree AppleClang build, full CTest, Homebrew LLVM 22 ASan+UBSan run; Python oracle invocations via the Anaconda base interpreter against a clean detached checkout only.
- **Evidence:** outside the repo under `/tmp/fincore-phase<N>-*` roots.
- **Oracle drift guard:** before Phase 40 begins, re-check fincore `dev` head; if past `4b8f31b`, re-run Phase 39's procedure first.

## 5. Exit Criteria (Definition of Done for the Iteration)

- All phases 38–52 have plan → design (clean-oracle run) → implementation → test hardening → acceptance record → README ledger entry, in commit order.
- `fincore.metrics` public API surface matches the Python oracle surface, module by module, except the explicit non-port (`bayesian.py` — PyMC).
- Full CTest green and clean-tree build at the iteration head; one ASan+UBSan run recorded for the iteration head.
- Golden-drift log from Phase 39 shows zero unresolved items.

## 6. Out of Scope

- Year×month heatmap pivot (lands with Iteration 4's heatmap phase).
- Any factor_analysis, performance, risk-model, optimization, viz, report, or runtime API.
- Rolling variants of the new ratios (Python rolling surface stays as-is; extend only if the oracle already exposes them in `metrics/rolling.py`).
