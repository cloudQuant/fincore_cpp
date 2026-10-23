# Phase 92 Acceptance — Runtime Catalog Selected Native Equivalents

## Scope

Expands `builtin_catalog()` from two operations to **44** selected native
equivalents of the pinned source catalog, using the source operation IDs and
recording a native kernel symbol in each spec's provenance. No new dependency
and no change to request/plan/batch/session/snapshot semantics.

## Source catalog classification

Pinned Python `4b8f31b1` builds **273** operations from ten non-empty provider
modules (catalog digest
`52c860002b00a6ca4f25a242f86a9297900d217490684808e88551d23e905d98`):

| domain | source ops | registered | classification of the remainder |
| --- | ---: | ---: | --- |
| metrics | 131 | 17 | Native kernels exist for most; registration is limited to scalar DataFrame→number paths and two value-owning frame paths. Aligned/pair/typed-kwarg variants and multi-output kernels remain unregistered. |
| risk | 20 | 3 | Backtesting/calibration scalars selected. Typed config objects (EVT/GARCH/walk-forward/report) need runtime input contracts not yet defined. |
| factor_analysis | 41 | 0 | Inputs are a prepared factor ledger, not expressible in the current `runtime_value` variant. |
| portfolio | 30 | 0 | Kernels exist; most return typed tables/windows and optional cash-column inputs, pending input contracts. |
| performance | 9 | 0 | Cashflow/inference kernels partially exist; runtime contracts pending. |
| simulation | 15 | 0 | Composer/parameter objects are not serializable as current runtime values. |
| attribution | 17 | 0 | Typed performance-attribution spans/columns pending. |
| data | 4 | 0 | Provider/data utilities; explicitly outside the native runtime scope. |
| report | 3 | 0 | Report documents are supported as a runtime value but their builders take domain models. |
| extensions | 0 | 0 | Provider is empty at the pin. |

No source ID is invented: all 44 registered IDs were checked for exact
membership in the pinned catalog (44/44 present, 0 missing). The previous
non-source ID `metrics.sharpe_ratio` is replaced by the source ID
`metrics.ratios.sharpe_ratio`.

## Registered operations

Metrics (default-parameter forms; `returns` and `benchmark` are DataFrame
inputs named after the Python callable parameters):

| operation ID | inputs | native kernel |
| --- | --- | --- |
| `metrics.alpha_beta.alpha` | returns, benchmark | `alpha` |
| `metrics.alpha_beta.beta` | returns, benchmark | `beta` |
| `metrics.drawdown.max_drawdown` | returns | `max_drawdown` |
| `metrics.ratios.calmar_ratio` | returns | `calmar_ratio` |
| `metrics.ratios.information_ratio` | returns, benchmark | `information_ratio` |
| `metrics.ratios.mar_ratio` | returns | `mar_ratio` |
| `metrics.ratios.omega_ratio` | returns | `omega_ratio` |
| `metrics.ratios.sharpe_ratio` | returns | `sharpe_ratio` |
| `metrics.ratios.sortino_ratio` | returns | `sortino_ratio` |
| `metrics.ratios.up_down_capture` | returns, benchmark | `up_down_capture` |
| `metrics.returns.cum_returns` | returns | `cumulative_returns` (one-column frame) |
| `metrics.returns.cum_returns_final` | returns | `cumulative_return` |
| `metrics.risk.annual_volatility` | returns | `annual_volatility` |
| `metrics.risk.conditional_value_at_risk` | returns | `conditional_value_at_risk` |
| `metrics.risk.tail_ratio` | returns | `tail_ratio` |
| `metrics.risk.tracking_error` | returns, benchmark | `tracking_error` |
| `metrics.yearly.annual_return` | returns | `annual_return` |

Risk:

| operation ID | inputs | native kernel |
| --- | --- | --- |
| `risk.backtesting.kupiec_lr` | observations, exceptions, confidence_level | `kupiec_lr` |
| `risk.calibration.basel_traffic_light` | exceptions, observations, confidence_level | `basel_traffic_light` |
| `risk.calibration.expected_exception_count` | observations, confidence_level | `expected_exception_count` |

Declared limits: optional Python keywords (`risk_free`, `annualization`,
`cutoff`, `column`, ...) are not exposed as runtime inputs in this slice, so
only the native defaults are reachable; `metrics.returns.cum_returns` returns
the native one-column DataFrame rather than a pandas Series; and the native
catalog digest intentionally differs from the Python digest because it
describes a selected subset with native provenance.

## Tests and gates

`Runtime.BuiltinCatalogRunsAcceptedOperations` asserts the 44-operation
catalog, every scalar result against its direct kernel call, the cumulative
frame digest against the direct kernel, Kupiec `5.025167926750753`, the
expected-exception count, the green Basel zone, rejection of the removed ID,
and a missing-benchmark `input_error`. Rolling operations expose explicit `window`/`rolling_window` inputs with the
native defaults (252 for roll kernels, 126 for `rolling_beta`) and return the
native value-owned frames. Full Release and ASan+UBSan CTest:
**726/726 passed**.
