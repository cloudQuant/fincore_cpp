# Iteration 5 — Factor Analysis: Requirements

**Phases:** 77–87. **Status:** Revised plan; NOT_RUN.
**Oracle:** Python `4b8f31b1b59707480d5db9603704d83bbf7e6946`.
The [shared delivery contract](../delivery-contract.md) applies.

## 1. Scope and entry

Port factor preparation, performance/turnover/portfolio/costs, full analysis,
research and selected reports. Phase 39 freezes the ledger/oracle; upstream
changes are recorded without automatic re-pinning. Native primitives used
by each slice must be accepted; compute phases need no GUI. Rendering uses
72–75 as applicable. Shared report/artifact contracts from 88a precede 84/86.

## 2. Functional scope and owners

| Phase | Pinned source | Required delivery |
|---|---|---|
| 77 | `factor_analysis/data.py`, `calendar.py`, `pit.py`, consumers | Panel schema/layout spike: identities, date/asset ordering, missingness, membership, groups/weights, horizons, calendar/PIT, lifetime and memory. Quantization/forward-return primitives validated before consumers. |
| 78 | `factor_analysis/data.py` | `prepare_factor_data`, `prepare_factor_data_by_horizon`, `prepare_factor_data_from_forward_returns`, `prepare_pit_factor_data`, `quantize_factor`, `compute_forward_returns` and ledger-required models. |
| 80 | `factor_analysis/performance.py`, `inference.py` | Mean/dispersion by quantile, IC/group IC, factor returns/alpha-beta and event cumulative-return capabilities; all exported statistical inference paths mapped, including optional dependency/error behavior. Split into bounded design slices. |
| 81 | `factor_analysis/performance.py` turnover routes | Quantile turnover, long/unbalanced universes, rank/autocorrelation and lag semantics from the ledger. Preserve absent-versus-missing observations and denominator membership. |
| 82 | `factor_analysis/portfolio.py`, `costs.py` | After the required Phase 80 performance kernels: portfolio inputs/weights/returns and cost/capacity ledger: `FactorCostModel`, `FactorCapacityResult`, `FactorCostResult`, `estimate_factor_capacity`, `apply_factor_costs`; gross/net return and unit/sign identities. |
| 79 | `factor_analysis/analysis.py`, `models.py` | `analyze_factor` assembles the full immutable model after its required 80/81/82 kernels. No backward dependency or duplicate implementation in consumers. |
| 83 | `factor_analysis/research.py` | `ResearchEvaluationSpec`, `ResearchEvaluationResult`, `research_capabilities`, `evaluate_factor_research`; pure evaluation returns tables/manifest. Separate explicit export operation writes CSV/optional chart artifacts with no overwrite. |
| 84 | `factor_analysis/models.py`, tears and `report/factor/compute.py` | Renderer-ready tear models and projection to the shared `ReportDocument`, preserving sections, metrics/tables/series, units, legends/notes and metadata. |
| 85 | factor tears/rendering | ImPlot summary/returns/information/turnover/event panels from frozen data; supported chart mappings and fidelity differences explicit. |
| 86 | `report/factor/html.py`, `report/assets/echarts.min.js` | Self-contained zh/en offline HTML; shared 88a model/serialization, pinned asset/hash/notices, correct chart values and browser rendering. |
| 87 | Whole factor pipeline | Measure compute-once, weight/snapshot reuse, cost of copies, sparse/dense scaling, runtime/RSS and report output; preserve numerical and isolation contracts. |

Phase IDs intentionally appear in execution order above. No blanket
exclusion of `inference.py`, calendar or PIT is permitted just because the
old draft omitted them. Phase 39/77 explicitly assigns each public capability
and any unsupported scenario, with a narrowed coverage claim if deferred.
The pin contains no separate `fincore/alphalens` facade to recreate.

## 3. Data correctness

Use one panel contract, but choose layout from measured dense and sparse
workloads instead of mandating a wide non-owning view. Represent absent
asset/date pairs, present NaNs, tradability/universe membership, groups,
weights and multiple forward-return horizons unambiguously. Lock duplicate
keys, unsorted dates, ties/duplicate quantile edges, all-equal values,
zero-aware/group quantization and data-loss accounting to actual source
behavior; do not assume every qcut path drops duplicate edges.

PIT fixtures must include publication/as-of timing, membership changes and
calendar gaps. Future returns used as labels must not leak into eligibility,
weights or earlier feature/analysis windows beyond the oracle contract.
Returned models survive caller mutation/destruction; cache reuse cannot
expose mutable shared results.

## 4. Exit

All ledger-assigned factor functions/models/scenarios reconciled and accepted;
no unexplained gap or hidden calendar/inference loss; compute and rendering
statuses separate; measured factor performance and durable evidence retained.
No network provider or new execution/trading engine is introduced.
