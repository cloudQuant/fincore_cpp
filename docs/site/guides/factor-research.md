# Factor research: from rows to tables

The factor area turns a prepared panel into IC studies, quantile buckets, tears,
artifacts and export tables. This guide follows that path and points out the
places where a configuration choice changes the result.

## The input panel

`prepared_factor_data` is the only accepted input:

```cpp
struct prepared_factor_data {
    std::vector<factor_row> rows;      // retained observations
    std::vector<std::string> forward_labels;  // labels aligned with row returns
    factor_loss_report loss_report;    // cleaning-loss accounting
};
```

Each `factor_row` carries a date, an asset, a finite factor value, a one-based
quantile label supplied by the caller, an optional group label, and a vector of
forward returns **aligned by position with `forward_labels`**. That alignment is
the contract: the label order and the per-row vector order must describe the same
columns.

## Analysis configuration

```cpp
fincore::factor_analysis_config config;
config.periods = {"1D", "5D"};        // must name prepared labels
config.turnover_periods = {1, 5};     // positive, unique lags
config.by_group = true;               // needs a group column
config.include_portfolio_inputs = false;
const auto model = fincore::analyze_factor(prepared, config);
```

| Field | Default | What it changes |
|---|---|---|
| `long_short` | `true` | Demeans weights and returns per date; `false` keeps raw values |
| `group_neutral` | `false` | Demeans within date and group; needs groups |
| `equal_weight` | `false` | Equal-weights the positive and negative sides |
| `by_group` | `false` | Adds per-group views; needs a group column |
| `periods` | empty (all) | Narrows the analysed forward-return labels |
| `turnover_periods` | `{1}` | Positive, unique lag values |
| `time_aggregation` | `{"M"}` | Monthly `"M"`/`"ME"` is the only native aggregation |
| `include_portfolio_inputs` | `true` | Builds portfolio inputs for the **first** selected period |
| `portfolio_benchmark_period` | `"1D"` | Non-empty label recorded in the fingerprint |

`analyze_factor` raises `factor_data_error` when the panel has no rows or labels,
a row's return count does not match the label count, `turnover_periods` or
`time_aggregation` is empty, duplicated or invalid, the benchmark period is
empty, a requested period is not a prepared whole-day label, or `by_group` is set
without a group column.

## What the model contains

- Aggregate views such as `mean_returns_by_quantile` and
  `mean_returns_by_date`, plus IC and turnover tables.
- Per-group views when `by_group` is set; those tables were computed from the
  full universe **before** group rows were selected, so demeaning can span
  groups.
- A fingerprint (`result_fingerprint`) that hashes the **inputs**, not the
  results, so two runs with the same inputs agree even before comparing numbers.

Two quantile tables exist and they are flattened differently. The aggregate one
(`quantile_return_table`) is **quantiles × labels**: `quantiles` holds the row
keys and `labels` the column keys. The per-date one
(`quantile_date_return_table`) adds a leading row axis, so it is
**date × quantiles × labels**, and one date's block is `quantiles × labels` wide.
Read the field comment before indexing by hand; both carry `groups` only when the
output is grouped.

## Tears and artifacts

`build_factor_tear_charts(model)` packages the model into chart-ready series:
information coefficient, mean return by quantile and period, top-minus-bottom
spread, turnover, rank autocorrelation, plus optional event and group snapshots.
`build_cumulative_returns_by_quantile`, `build_events_distribution`,
`build_ic_qq`, `build_quantile_returns_violin` and `build_monthly_ic_heatmap`
produce individual charts from the same tables.

`factor/artifacts.hpp` provides the export path. Its tables are an ordered
contract: column order is part of the digest, so a column inserted in the middle
is a contract change even when the values are identical.

## Point-in-time joins

Point-in-time helpers join a signal panel to a reference panel using three
timestamps per reference row: when the value became **effective**, when it became
**known**, and the **as-of** date being evaluated. A reference row is eligible
only when it was effective and known by the as-of date; ties resolve in the order
the header documents. Universe filters — for example dropping assets that are not
in the index on that date — are applied on top of that cut, which is why a case
can legitimately end up with fewer cross-sections than input dates.

## Exports and comparison

`factor/research.hpp` writes the research tables used by the Alpha003 benchmark.
The comparison is **streamed** and runs outside every timed interval; no join,
dropna, aggregation or tolerance expansion is applied to hide missing
observations. A mismatch is reported with the table, row and column that differ,
not as a single failing boolean.

Two practical rules follow:

1. Changing a header, a label or a column order changes the digest, so export
   changes are visible in the paired benchmark even when the numbers are equal.
2. Pinned reference files under `tests/fixtures` and `tests/data` are evidence.
   Treat their text as data, not as prose to reword.

## A minimal study

```cpp
const auto model = fincore::analyze_factor(prepared, config);
const auto tears = fincore::build_factor_tear_charts(model);
const auto& table = model.mean_returns_by_quantile;

// Row keys are quantiles, column keys are labels: stride the row-major buffer.
for (std::size_t q = 0; q < table.quantiles.size(); ++q) {
    for (std::size_t l = 0; l < table.labels.size(); ++l) {
        const double mean = table.means[q * table.labels.size() + l];
        std::printf("Q%d %s %.5f
", table.quantiles[q],
                    table.labels[l].c_str(), mean);
    }
}
```

The aggregate table's label order follows the request, so print
`mean_returns_by_quantile.labels` alongside any table you save; a table without
its label order is ambiguous once it leaves the process.
