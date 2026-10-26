# Metrics and rolling windows

This guide explains how the metric families fit together, which conventions the
numbers follow, and what each family returns when the input is degenerate. It is
a map, not a substitute for the API reference: every function carries the exact
formula and failure rule in its own documentation.

## Inputs and units

Metric functions take a `fincore::dataframe`, a column name (defaulting to
`"returns"` for most of them) and, where relevant, a risk-free rate. Values are
decimal returns (`0.01` is one percent). Timestamps are UTC nanosecond instants
and are used for ordering, window boundaries and date-based grouping, not for
arithmetic on the values.

`NaN` means "missing" everywhere in this library. It is not silently replaced by
zero, and it is not dropped from a frame. Each function states whether it drops
missing observations pairwise, per column or not at all; the default for the
scalar ratios is to drop `NaN` and work with the remaining values.

An empty or mis-typed column is a structural error and raises `input_error`.
A column that exists but is too short usually yields `NaN` rather than an
exception, because "not enough data" is a value, not a programming mistake.

## Annualization

`annualization_factor(frequency)` returns the number of periods per year:

| Frequency | Factor |
|---|---|
| `daily` | 252 |
| `weekly` | 52 |
| `monthly` | 12 |
| `quarterly` | 4 |
| `yearly` | 1 |

Passing an explicit value as the second argument returns it unchanged, including
zero or a negative number — validation belongs to the caller. Ratios that scale
by `sqrt(annualization)` use this factor directly, so switching frequency changes
a Sharpe ratio but never the sign of the mean.

## Return and ratio family

| Group | Functions | Notes |
|---|---|---|
| Level and excess | `mean_return`, `annual_return`, `adjust_returns` | `adjust_returns` subtracts a constant from every value and keeps `NaN` as `NaN` |
| Risk-adjusted | `sharpe_ratio`, `adjusted_sharpe_ratio`, `sortino_ratio`, `calmar_ratio`, `mar_ratio`, `conditional_sharpe_ratio`, `deflated_sharpe_ratio`, `omega_ratio` | Each has its own deviation divisor and its own degenerate rule |
| Tail | `conditional_sharpe_ratio`, `tail_ratio`, `common_sense_ratio`, `value_at_risk` family in `metrics/risk.hpp` | The tail is defined by a cutoff quantile, not by a fixed count |
| Benchmark-relative | `alpha`, `beta`, `alpha_beta`, `up_alpha_beta`, `down_alpha_beta`, `tracking_error`, `information_ratio`, `capm_r_squared` | Pairwise alignment against a benchmark column |

Two rules explain most surprises:

- **Sample versus population.** `sharpe_ratio` divides by the sample standard
  deviation (one degree of freedom), while the downside deviation inside
  `sortino_ratio` divides squared deviations by the number of usable
  observations. The two ratios are therefore not interchangeable denominators.
- **Zero is not special-cased.** A zero deviation is left to IEEE arithmetic
  rather than replaced by `NaN` or infinity, except where a function says
  otherwise. `calmar_ratio` and `mar_ratio` return `NaN` when the maximum
  drawdown is zero or positive, because the quotient would be meaningless.

`adjusted_sharpe_ratio` clamps its skew/kurtosis adjustment to `[0.9, 1.1]` for
fewer than 20 raw observations and to `[0.8, 1.3]` otherwise, and returns `NaN`
for fewer than four raw values. `deflated_sharpe_ratio` treats a trial count
below one as one and returns exactly 0 or 1 when its estimated Sharpe variance is
non-positive.

## Drawdown family

| Function | Returns |
|---|---|
| `drawdown_series` | A frame with the running drawdown for each observation |
| `max_drawdown`, `second_max_drawdown`, `third_max_drawdown` | A single depth as a negative or zero fraction |
| `max_drawdown_window`, `max_drawdown_underwater` | The peak, valley and optional recovery timestamps of the deepest episode |
| `get_all_drawdowns`, `get_all_drawdowns_detailed`, `get_top_drawdowns` | Depth lists, or rows with peak/valley/recovery and duration |
| `max_drawdown_days` / `_weeks` / `_months` | Depth of the worst drawdown measured over that calendar bucket |
| `max_drawdown_recovery_days` / `_weeks` / `_months` | Time to recover the worst drawdown |

When several highs are equal, the window starts at the **latest** one, so it
covers the episode that produced the trough rather than everything since the
first equal high. A series that never declines has no drawdown window at all,
which is why the window functions return `std::optional`.

## Distribution and stability statistics

`skewness`, `kurtosis`, `serial_correlation`, `hurst_exponent`, `stutzer_index`,
`r_cubed`, `r_cubed_turtle`, `win_rate`, `loss_rate`, `relative_win_rate`,
`safe_correlation` and the performance-statistics family in `metrics/perf_stats.hpp`
answer "what shape is this series?" rather than "what did it earn?".

They share two conventions: non-finite observations are excluded from the
statistic and reported through the usable count, and the divisor is the one
stated in each function's documentation. Some use the biased (population) form
to match a reference implementation; do not assume consistency across functions.

## Rolling windows and tables

`rolling_engine` computes a requested set of metrics over a fixed window and
returns one frame per metric. Windows are complete-window only: the first
`window - 1` observations produce no row, so the output index is shorter than the
input index. `metrics/rolling.hpp` exposes the individual rolling primitives for
callers that need a single series.

Table builders (`metrics/yearly.hpp`, `metrics/frequencies.hpp` and the quantile
tables in `factor/performance.hpp`) group by date or period. They preserve the
ordering rule stated in their own documentation — some order by calendar year and
ISO week label rather than by bucket start, which can place a January observation
after a December row of the same year.

## Worked example

```cpp
#include <fincore/dataframe.hpp>
#include <fincore/metrics/drawdown.hpp>
#include <fincore/metrics/ratios.hpp>

#include <cstdio>

int main() {
    const auto returns = fincore::make_series(
        {fincore::utc_timestamp(2025, 1, 2),
         fincore::utc_timestamp(2025, 1, 3),
         fincore::utc_timestamp(2025, 1, 6),
         fincore::utc_timestamp(2025, 1, 7)},
        "returns", {0.012, -0.02, 0.005, 0.03});

    std::printf("sharpe   %.4f\n", fincore::sharpe_ratio(returns, "returns"));
    std::printf("sortino  %.4f\n", fincore::sortino_ratio(returns, "returns"));
    std::printf("max dd   %.4f\n", fincore::max_drawdown(returns, "returns"));

    if (const auto window = fincore::max_drawdown_underwater(returns, "returns")) {
        std::printf("peak %lld valley %lld\n",
                    static_cast<long long>(window->peak),
                    static_cast<long long>(window->valley));
    }
    return 0;
}
```

Four observations are enough for a Sharpe ratio but not enough to be
representative; the example shows the calling shape, not a research result.

## Choosing a metric

- Reporting a headline risk-adjusted number? Use `sharpe_ratio` and say which
  annualization you used.
- Penalising downside only? `sortino_ratio` with an explicit `required_return`.
- Judging a short track record? `adjusted_sharpe_ratio` or
  `deflated_sharpe_ratio`; both try to discount luck, and both say so in their
  clamped ranges.
- Comparing against a benchmark? `alpha`/`beta` and `information_ratio`, and
  remember the alignment rule: the two frames are paired on the timestamps they
  share.
- Summarising a path? Drawdown windows and recovery lengths are more informative
  than a single maximum, because they say how long a reader would have suffered.

The API reference lists every function with its exact formula and failure rule;
when a number surprises you, the function's own documentation is the authority.
