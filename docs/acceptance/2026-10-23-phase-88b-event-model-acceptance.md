# Phase 88b Acceptance — Factor Event Analysis Model

## Scope

Adds the optional event-analysis payload to the compute-once factor model:
`analyze_factor(factor_data, config, event_returns, event_before,
event_after)` builds a `factor_event_model` plus an owning event-input
snapshot. This closes the event half of the 88b integration gap; group-level
analysis remains outside the model.

## Model

- `offsets` and `event_dates` from the event-window kernel.
- `mean_returns`: per-offset mean across event dates.
- `return_distribution`: all finite window cells, ordered by offset then event
  date.
- `quantile_average_returns`: per-quantile event means and sample standard
  deviations across dates.
- `event_input_snapshot`: an owning copy of the supplied returns frame.
- The model fingerprint now binds the event frame, columns and values plus
  `event_before`/`event_after`, so changing event data changes the digest.

`event_before`/`event_after` must be non-negative; event analysis with
`group_neutral=true` fails closed because the native quantile event kernel has
no group adjustment. The source factor report has no event section, so the
report projection is unchanged.

## Oracle

Pinned Python `4b8f31b1`, 5 dates x 3 assets, event window (-1..2), long-short
demeaning; script
`/tmp/fincore-eventmodel-python.*/oracle.py`, JSON SHA-256
`e8ada969888e8c215ec37458fdd59daa33710415f8810865690767408f1dc7b1`.

| quantity | oracle |
| --- | --- |
| offsets | -1, 0, 1, 2 |
| event dates / distribution cells | 5 / 16 |
| mean-return path | ~0 at every offset (demeaned across all event assets) |
| Q1 event means | -0.003333333333333334, -6.94e-19, 0.0025, 0.004444444444444444 |
| Q1 sample std at offset -1 | 0.009428090415820633 |
| Q2 event means | -0.0008333333333333339, -0.004, -0.005, -0.002222222222222224 |
| Q3 event means | 0.004166666666666666, 0.004, 0.0025, -0.002222222222222223 |

The test asserts these values, the snapshot, fingerprint changes for added
and mutated event data, and fail-closed behaviour for group-neutral and
negative windows.

## Declared gaps

Group-aware event analysis (`by_group`, `group_adjust`), group-level factor
analysis and event tear-sheet rendering are not modelled. Event windows use
the source's inverted `cumulative` flag semantics through the accepted kernel.

## Gates

Full Release CTest: **723/723 passed**. Full ASan+UBSan: **723/723 passed**.
Visual NOT_RUN.
