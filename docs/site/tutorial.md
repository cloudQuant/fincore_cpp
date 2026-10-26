# Tutorial: a monthly performance report from prices

This tutorial builds a small but complete artefact: a price series becomes
returns, headline statistics, a rolling view, a monthly table and a rendered
report. It takes about fifteen minutes and needs nothing but a compiler and the
installed library.

Values are synthetic. Nothing here is an investment result.

## Set up

Build and install the library as described in [installation](installation.md), or
add `include/` and the built library to a scratch project. Every snippet below
continues from the previous one.

```cpp
#include <fincore/dataframe.hpp>
#include <fincore/metrics/drawdown.hpp>
#include <fincore/metrics/ratios.hpp>
#include <fincore/metrics/returns.hpp>
#include <fincore/report/html.hpp>
#include <fincore/report/models.hpp>
#include <fincore/report/xlsx.hpp>
#include <fincore/rolling_engine.hpp>

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
```

## Step 1 — Turn prices into returns

`simple_returns` divides each price by the previous one and drops the first
observation, keeping the timestamp of the later price of each pair.

```cpp
std::vector<fincore::timestamp> index;
std::vector<double> prices;
const auto day = 86'400'000'000'000LL;
for (int i = 0; i < 260; ++i) {
    index.push_back(fincore::utc_timestamp(2025, 1, 2) + i * day);
    prices.push_back(100.0 + 6.0 * std::sin(i * 0.11) +
                     3.0 * std::cos(i * 0.37) + 0.02 * i);
}
fincore::dataframe price_frame;
price_frame.load_index(std::vector<fincore::timestamp>(index));
price_frame.load_column<double>("close", prices,
                                hmdf::nan_policy::dont_pad_with_nans);

const auto returns = fincore::simple_returns(price_frame, "close");
std::printf("returns: %zu rows\n", returns.shape().first);
```

Check the row count: 260 prices produce 259 returns, and the first timestamp is
the second price's date. If your count differs, the input index probably repeats
a date or is unsorted.

## Step 2 — Headline statistics

```cpp
const double sharpe = fincore::sharpe_ratio(returns, "returns");
const double sortino = fincore::sortino_ratio(returns, "returns");
const double drawdown = fincore::max_drawdown(returns, "returns");
const double calmar = fincore::calmar_ratio(returns, "returns");

std::printf("sharpe %.3f  sortino %.3f  max dd %.3f  calmar %.3f\n",
            sharpe, sortino, drawdown, calmar);
```

All four annualize with 252 periods and drop `NaN`. A series that never declines
has no drawdown, so `calmar_ratio` returns `NaN` rather than a large number —
that is deliberate, and worth handling in your own output.

## Step 3 — A rolling view

`rolling_engine` evaluates metrics over every complete trailing window. A
20-observation window therefore starts producing output at row 20, not row 1.

```cpp
fincore::rolling_engine rolling{returns, std::nullopt, 20U, 252.0};
const auto series = rolling.compute(
    {fincore::rolling_metric::sharpe,
     fincore::rolling_metric::volatility});

for (const auto& [name, frame] : series) {
    const auto& values = frame.get_column<double>(fincore::returns_column);
    std::printf("%-12s %zu rows, last %.4f
", name.c_str(),
                frame.shape().first, values.back());
}
```

The map key names the metric; the output frame still uses the ordinary returns
column name, so one loop reads both series.

## Step 4 — A monthly table

`aggregate_returns` groups by calendar month and adds integer year and period
columns next to the aggregated value.

```cpp
const auto monthly = fincore::aggregate_returns(
    returns, fincore::return_aggregation_period::monthly, "returns");
std::printf("months: %zu\n", monthly.shape().first);
```

Monthly values are compounded within the month, not summed. That distinction is
the usual reason a hand-rolled table disagrees with this one.

## Step 5 — Compare with a benchmark

```cpp
const auto benchmark_prices = price_frame;   // stand-in for a real index
const auto benchmark = fincore::simple_returns(benchmark_prices, "close");

std::printf("beta %.3f  alpha %.4f\n",
            fincore::beta(returns, benchmark),
            fincore::alpha(returns, benchmark));
```

The two frames are paired on the timestamps they share, so a benchmark with a
different holiday calendar shortens the sample rather than shifting it.

## Step 6 — Build and render the report

```cpp
fincore::report_document document;
document.domain = "portfolio";
document.title = "Monthly performance review";
document.metadata = {{"source", "tutorial"}, {"frequency", "daily"}};

fincore::report_section section;
section.key = "performance";
section.title = "Performance";

auto add_metric = [&section](std::string key, std::string label, double value) {
    fincore::report_metric metric;
    metric.key = std::move(key);
    metric.label = std::move(label);
    metric.number = value;
    section.metrics.push_back(std::move(metric));
};
add_metric("sharpe", "Sharpe ratio", sharpe);
add_metric("sortino", "Sortino ratio", sortino);
add_metric("max_drawdown", "Maximum drawdown", drawdown);

fincore::report_table table;
table.key = "monthly_returns";
table.title = "Monthly returns";
table.columns = {"month", "return"};
for (std::size_t row = 0; row < monthly.shape().first; ++row) {
    table.rows.push_back(
        {fincore::report_cell::number_cell(
             static_cast<double>(row + 1U)),
         fincore::report_cell::number_cell(
             monthly.get_column<double>("returns")[row])});
}
section.tables.push_back(std::move(table));
document.sections.push_back(std::move(section));

const std::string html = fincore::render_html(document, {.language = "en"});
const std::string workbook = fincore::render_xlsx(document);
```

Write `html` and `workbook` to files and open them: the HTML is self-contained,
and the workbook has one sheet per section. Rendering the same document twice
produces identical bytes, so the files are safe to hash.

## What you built

- A returns frame derived from prices with the documented warm-up rule.
- Four headline statistics, each with its own degenerate behaviour.
- Two rolling series aligned to a complete-window rule.
- A monthly table that compounds inside each month.
- A benchmark comparison that pairs on common dates.
- One report document rendered to two formats.

## Where to go next

- [Metrics and rolling windows](guides/metrics.md) for the definition behind each
  number you just printed.
- [Portfolio, risk, optimization and simulation](guides/portfolio-risk.md) to add
  VaR, expected shortfall and a backtest verdict.
- [Factor research](guides/factor-research.md) to move from one series to a panel.
- [Reports, runtime and extensions](guides/reports-runtime.md) for the other
  backends and the artifact bundle contract.
