# Quickstart

This walkthrough goes from a small in-memory frame to a rendered report. It uses
synthetic values and does not download market data; nothing here is an investment
result.

## Build a return series and read a few numbers

```cpp
#include <fincore/dataframe.hpp>
#include <fincore/metrics/drawdown.hpp>
#include <fincore/metrics/ratios.hpp>

#include <cstdio>
#include <vector>

int main() {
    const std::vector<fincore::timestamp> index{
        fincore::utc_timestamp(2025, 1, 2),
        fincore::utc_timestamp(2025, 1, 3),
        fincore::utc_timestamp(2025, 1, 6),
        fincore::utc_timestamp(2025, 1, 7)};

    fincore::dataframe returns;
    returns.load_index(std::vector<fincore::timestamp>(index));
    returns.load_column<double>(
        "returns", {0.012, -0.02, 0.005, 0.03},
        hmdf::nan_policy::dont_pad_with_nans);

    std::printf("sharpe  %.4f\n", fincore::sharpe_ratio(returns, "returns"));
    std::printf("max dd  %.4f\n", fincore::max_drawdown(returns, "returns"));
    return 0;
}
```

Three things are worth noticing:

- The index is explicit; `utc_timestamp` validates the calendar date and rejects
  an unrepresentable one.
- `dont_pad_with_nans` keeps the values exactly as given. A padding policy would
  insert `NaN` for absent positions instead.
- Metrics take the column name. Passing a column that does not exist, or one that
  is not a `double` column, raises `input_error` — a typo fails loudly.

## Prepare a factor panel

`analyze_factor` consumes `prepared_factor_data`: rows, forward labels and a
cleaning-loss report. Each row's `forward_returns` vector must be aligned by
position with `forward_labels`.

```cpp
#include <fincore/factor/analysis.hpp>

fincore::prepared_factor_data data;
data.forward_labels = {"1D", "5D"};
const auto start = fincore::utc_timestamp(2022, 1, 3);
const auto day = 86'400'000'000'000LL;
const char* assets[] = {"ALPHA", "BETA", "GAMMA", "DELTA", "EPSILON"};

for (int date = 0; date < 40; ++date) {
    for (int asset = 0; asset < 5; ++asset) {
        fincore::factor_row row;
        row.date = start + date * day;
        row.asset = assets[asset];
        row.factor = 0.01 * static_cast<double>(asset) -
                     0.02 * static_cast<double>(date % 7);
        row.quantile = asset + 1;                 // caller-supplied bucket
        row.forward_returns = {0.001 * (date % 5),
                               0.002 * (asset % 3)};  // aligned with 1D, 5D
        data.rows.push_back(row);
    }
}

fincore::factor_analysis_config config;
config.periods = {"1D", "5D"};
config.turnover_periods = {1, 5};
config.include_portfolio_inputs = false;

const auto model = fincore::analyze_factor(data, config);
```

`quantile` is supplied by the caller, not computed: the analysis studies the
buckets you declare. `periods` must name prepared labels, and `forward_returns`
is aligned with `forward_labels` by position, which is why the two lists here
describe the same columns in the same order.

## Render a report

```cpp
#include <fincore/factor/report.hpp>
#include <fincore/report/html.hpp>
#include <fincore/report/xlsx.hpp>

#include <fstream>
#include <string>

const auto document = fincore::build_factor_report(model, "Factor Report");

fincore::report_html_options options;
options.language = "en";                 // "zh" for the Chinese labels
const std::string html = fincore::render_html(document, options);
const std::string workbook = fincore::render_xlsx(document);

std::ofstream{"factor_report.html", std::ios::binary} << html;
std::ofstream{"factor_report.xlsx", std::ios::binary} << workbook;
```

The same `report_document` drives every backend. `render_html` produces one file
per language from the same model, and `render_xlsx`, `render_pdf_bytes`,
`render_plotly`, `render_bokeh` and `render_matplotlib` are further views of it.
Rendering twice produces byte-identical output for the deterministic backends,
which is what makes reports safe to hash and diff.

## Handle failures where they happen

```cpp
#include <fincore/error.hpp>

try {
    const double sharpe = fincore::sharpe_ratio(returns, "missing");
    (void)sharpe;
} catch (const fincore::input_error& error) {
    std::fprintf(stderr, "rejected: %s (%s)\n", error.what(),
                 error.code().c_str());
}
```

Structural problems throw; a well-formed series that simply cannot produce the
statistic returns `NaN`. Check `std::isnan` for the second case and catch
`fincore::fincore_error` for the first.

## Next steps

- [Data and numerical conventions](conventions.md) — timestamps, `NaN`, ownership,
  digests and the error model.
- [Metrics and rolling windows](guides/metrics.md) — which ratio to use and how
  each one treats missing data.
- [Portfolio, risk, optimization and simulation](guides/portfolio-risk.md) — the
  risk estimators and their statuses.
- [Factor research](guides/factor-research.md) — IC, quantiles, tears and exports.
- [API reference](api/index.md) — every function with its exact contract.
