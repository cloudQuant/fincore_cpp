# Iteration 5 — Factor Analysis: Design

**Status:** Revised plan. [Shared contract](../delivery-contract.md) applies.

## 1. Phase 77 panel spike

Evaluate owning wide tables versus canonical long/columnar storage with
compact date/asset IDs and indexed views. Benchmark dense and sparse,
unbalanced universes before choosing; a T×A dense allocation is not forced
when only a small fraction of pairs exists. Freeze complexity and peak-memory
budgets using Phase 39 workloads. `factor_panel` is the single semantic
boundary; internal iteration views may differ without changing its contract.

Owned/immutable backing storage holds timestamps, asset IDs, factor values,
forward-return columns, groups, weights and validity/membership masks.
Describe multiple factors/horizons without ambiguous "optional factor
columns" mixed into asset columns. Internal views cannot outlive backing
ownership. Returned prepared/analysis/research/report objects own or retain
immutable data, and tests destroy/mutate the caller's original inputs.

Freeze sort/duplicate policy, calendar/day-count and timezone normalization,
horizon meaning, label boundaries, group handling, ties/quantile errors,
missingness, asset entry/exit and PIT/as-of availability. Adapt Python
MultiIndex semantics explicitly; a scalar metric's preserve-input-order rule
is not a blanket ban on oracle-required panel sorting.

## 2. Kernel-first analysis and costs

78 builds prepared data from the adapter. 80/81/82 supply performance,
turnover, portfolio and gross-to-net cost/capacity calculations. Only then
79 assembles `FactorAnalysisModel`, including its portfolio inputs. Separate
small designs within 80/82 keep their broad public inventories implementable.

Shared weights, membership and forward-return results are computed once per
compatible analysis request. Cache identity includes input content/version,
asset/date ordering, horizon, groups/weights, calendar, config and oracle/
algorithm version as appropriate. Specify invalidation and deterministic
ordering. Do not hash raw object addresses or rely on mutable caller storage.

Independent invariants include weight budgets, group-neutral conditions when
selected, turnover bounds under declared conventions, gross/net/cost ledger
identities and future-perturbation invariance of earlier computations.
Regression/inference/special functions receive justified numerical budgets;
"no iterative solver" does not imply all floating results are bit-exact.

## 3. Models, research and rendering

84 projects immutable analysis results into the 88a shared document schema.
It does not create a competing factor-only document/asset type. 83 evaluates
plain transport inputs into tables/manifest with no file I/O. A separate
export function uses the 88a artifact contract, returns explicit paths and
statuses, creates a unique evaluation directory and refuses overwrite,
matching the pinned source. CSV serialization and optional chart export are
accepted independently; headless computation does not require matplotlib.

85 consumes neutral chart data with ImPlot. 86 embeds the pinned ECharts
asset with its version/hash and redistribution notices, uses a shared JSON/
HTML serializer, and renders offline. Encode user-supplied labels, notes and
asset names correctly inside HTML and script JSON, including `</script>`,
quotes, Unicode and nonfinite numeric representations. No CDN fallback is
needed for an offline artifact. Do not claim visual equivalence for
substituted violin/QQ features.

## 4. Measured compute-once and lifetime checks

Instrument expensive kernel invocation counts and allocation/copy counts;
rendering a second format must not rerun analysis. Snapshot reuse must also
preserve isolation when a consumer mutates a returned copy. Python snapshots
can themselves copy on access; mirror the observable isolation semantics,
not a presumed free view or every incidental internal allocation.

87 reports cold/warm computation, snapshot/projection, serialization and
file-output costs separately with equivalent Python/C++ work, several panel
sizes and peak RSS. Test cache-key changes and no stale output. A structural
argument alone is insufficient for a performance or compute-once claim.

## 5. Sequence

39 → 77 → 78 → 80/81; 82 also requires the performance kernels it calls
from 80. Complete required 80/81/82 before 79. Then 83 (plus artifact contract) and
84 (plus 88a). 85 requires 84 and needed chart primitives; 86 requires 84 and
88a. They can proceed separately. 87 closes measured behavior after the
functional slices; baseline collection occurs before them, not only at 87.
