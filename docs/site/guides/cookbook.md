# Cookbook

Short, working answers to tasks that come up again and again. Each recipe is
self-contained; the guides explain the reasoning behind the rules.

## Rolling metrics for one series

```cpp
#include <fincore/rolling_engine.hpp>

fincore::rolling_engine engine{returns, std::nullopt, 60U, 252.0};
const auto series = engine.compute({fincore::rolling_metric::sharpe,
                                    fincore::rolling_metric::volatility,
                                    fincore::rolling_metric::max_drawdown});
for (const auto& [name, frame] : series) {
    const auto& values = frame.get_column<double>(fincore::returns_column);
    // values.size() == returns.shape().first - 60 + 1
}
```

The output starts at the first **complete** window, so it is shorter than the
input by `window - 1` rows. Pass a benchmark frame as the second argument only if
you ask for `rolling_metric::beta`.

## The deepest drawdowns as a table

```cpp
#include <fincore/metrics/drawdown.hpp>

for (const auto& row : fincore::gen_drawdown_table(returns, 5U, "returns")) {
    if (std::isnan(row.net_drawdown_percent)) {
        continue;  // unused row: fewer than five episodes in the series
    }
    std::printf("%.2f%%  peak %lld  valley %lld\n",
                row.net_drawdown_percent,
                static_cast<long long>(*row.peak),
                static_cast<long long>(*row.valley));
}
```

Depth is a **positive** percentage of the prior peak, unlike `max_drawdown`,
which returns a negative fraction. When highs tie, the window starts at the
latest one.

## VaR plus a coverage verdict

```cpp
#include <fincore/risk/backtesting.hpp>
#include <fincore/risk/models.hpp>

fincore::risk_forecast_config config;
config.confidence_level = 0.99;

const auto estimate = fincore::forecast_var(returns, config);
const auto verdict = fincore::backtest_var(forecast, realized, 0.99, 0.05);

if (estimate.status != fincore::risk_estimate_status::ok) {
    // insufficient_data or failed: do not report the number as a result
} else if (verdict.status == fincore::risk_backtest_status::inconclusive) {
    // fewer than three pairs or fewer than five expected exceptions
}
```

`estimate` is negative for losses under the `losses_negative` convention. An
`inconclusive` verdict is not a pass: 250 days at 99% expects 2.5 exceptions,
below the five needed to test.

## GARCH VaR over a multi-day horizon

```cpp
config.method = fincore::risk_forecast_method::garch;
config.garch_kernel = fincore::garch_model::gjr_garch;
config.horizon = 10U;
const auto ten_day = fincore::forecast_var(returns, config);
```

Only the GARCH method composes over `horizon`; historical and EVT estimates
ignore it. A fit that does not converge returns `failed` with a `note` instead of
a number.

## Quantile means from a factor panel

```cpp
const auto model = fincore::analyze_factor(prepared, factor_config);
const auto& table = model.mean_returns_by_quantile;

for (std::size_t q = 0; q < table.quantiles.size(); ++q) {
    for (std::size_t l = 0; l < table.labels.size(); ++l) {
        std::printf("Q%d %s %.5f\n", table.quantiles[q],
                    table.labels[l].c_str(),
                    table.means[q * table.labels.size() + l]);
    }
}
```

Row keys are quantiles, column keys are labels, and the buffer is row-major. Read
`labels` from the same table when you save it; a table without its label order is
ambiguous.

## A point-in-time panel from revisions

```cpp
#include <fincore/factor/pit.hpp>

std::vector<fincore::pit_observation> observations{
    {"AAA", {as_of, known_at, effective_from, 1.25}, true},
    {"BBB", {as_of, known_at, effective_from, -0.40}, false},  // out of universe
};

const auto prepared = fincore::prepare_pit_factor_data(
    observations, prices, evaluation_dates);
const auto model = fincore::analyze_factor(prepared, factor_config);
```

Ordering is checked, not repaired: `known_at` must not precede `as_of`, and
`effective_from` must not precede `known_at`. On each evaluation date an asset
contributes the revision with the largest `(effective_from, known_at, as_of)`
among those already effective and known; assets outside the universe are skipped
silently, which is why a date can have fewer cross-sections than assets.

## A reproducible simulation

```cpp
#include <fincore/simulation/paths.hpp>

const auto paths = fincore::geometric_brownian_motion(
    100.0,    // initial value
    0.08,     // annualized drift
    0.20,     // annualized volatility
    1.0,      // total years
    1.0 / 252.0,  // time step
    1'000U,   // path count
    42U);     // seed
```

The same seed reproduces the same frame inside this library. That is a
reproducibility guarantee, not a promise of NumPy identity — only functions whose
documentation names a reference stream carry that promise.

## Add a table to a report and export it

```cpp
fincore::report_section section;
section.key = "risk";
section.title = "Risk";

fincore::report_table table;
table.key = "var";
table.title = "VaR by confidence level";
table.columns = {"level", "var", "verdict"};
for (const auto& row : rows) {
    table.rows.push_back({fincore::report_cell::text_cell(row.level),
                          fincore::report_cell::number_cell(row.value),
                          fincore::report_cell::text_cell(row.verdict)});
}
section.tables.push_back(std::move(table));
document.sections.push_back(std::move(section));

const std::string html = fincore::render_html(document, {.language = "zh"});
```

`language = "zh"` renders the Chinese labels from the same document; the cells you
supply are printed as given, so translate your own labels at the call site.
