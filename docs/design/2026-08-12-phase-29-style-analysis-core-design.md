# Phase 29 Native Style-Analysis Core Design

## Scope decision

Python's `fincore.attribution.style` contains one deterministic, tabular
analysis boundary (`style_analysis`), several derived analysis helpers, a
regression wrapper, and a remote factor-fetching entry point. The selected
Phase 29 slice is a complete native counterpart for the deterministic
`style_analysis` leaf: it classifies assets by size, momentum, value, and
volatility, derives the style-weighted return summary, and preserves the
equal-weighted time series in DataFrame form.

Three routes were considered:

1. **Selected — native DataFrame style-analysis leaf.** Implement the full
   deterministic `style_analysis` calculation and its optional size/value
   inputs with a value-owning result structure. It directly advances the
   requested pandas-to-DataFrame migration without adding a numerical solver.
2. **Mean-variance optimization first.** Python's optimization module is
   centered on SciPy SLSQP. A faithful C++ route requires a new solver policy
   or dependency, which is a separate architectural decision.
3. **EVT/GARCH risk models first.** Their source paths use SciPy fitting and
   optimizer-dependent estimates. Migrating them before fixing a native fit
   contract would make numeric parity and failure semantics ambiguous.

This phase selects route 1. It explicitly excludes `calculate_style_tilts`,
`calculate_regression_attribution`, `analyze_performance_by_style`,
`fetch_style_factors`, remote providers, SciPy, and plotting/report work.

## Public API and DataFrame mapping

Create `<fincore/attribution/style.hpp>` with these value types and exactly
one leaf:

```cpp
struct style_analysis_options {
    std::span<const double> market_caps{};
    std::span<const double> book_to_price{};
    std::span<const double> value_scores{};
    std::size_t momentum_window{252U};
    std::array<double, 2U> size_quantiles{.5, .5};
};

struct style_analysis_result {
    dataframe exposures;
    dataframe returns_by_style;
    dataframe overall_returns;
};

[[nodiscard]] style_analysis_result style_analysis(
    const dataframe& asset_returns,
    const style_analysis_options& options = {});
```

`asset_returns` is time-by-asset: its raw index is the input order and every
genuine `double` column is an asset, named by that DataFrame column. Python
uses labeled pandas Series for market caps, book-to-price, and value scores;
the native spans deliberately map positionally to this stable double-column
order because `StdDataFrame<timestamp>` does not support a string index. A
provided optional span must have one finite value per asset. `value_scores`
takes precedence over `book_to_price`, matching Python.

`exposures` uses an ordinal timestamp index (`0..asset_count-1`), an `asset`
string column containing the original names, then ordered double weight columns.
With market caps, those are `large`, `mid`, `small`; without caps the size
column is `equal_weight`. They are followed by `winner`, `loser`, `value`,
`growth`, `high`, and `low`. `returns_by_style` has ordinal index
`0..style_count-1`, a `style` string column in that same style order, and a
`return` double column. `overall_returns` retains the exact raw input index and
one `return` double column. All three frames are freshly materialized and
value-owning.

## Algorithm and native boundaries

The size masks use Python-compatible linear percentiles of supplied market
caps at the two supplied quantiles: `large >= upper`, `mid >= lower && < upper`,
and `small < lower`. Each nonnegative mask is normalized to sum to one when
its sum is positive. Without caps, `equal_weight` is normalized one for every
asset. The momentum path compounds the last
`min(momentum_window, observation_count)` returns per asset, classifies values
at or above the cross-sectional median as `winner`, and remaining assets as
`loser`. Value uses `value_scores` if present, otherwise book-to-price, with
the median split; no supplied score makes both `value` and `growth`
equal-weighted.

For volatility, replicate source rolling standard deviation semantics: for
each finite asset series, use trailing windows of width at most 60 with
`min_periods=20` and population (`ddof=0`) standard deviation. Compare the
last rolling standard deviation to that asset's median of defined rolling
values. `high` is `>=`, `low` is `<`; a style with zero total weight retains a
NaN style return, as pandas does. This preserves source behavior for short
histories without inventing a fallback classification.

Native input requires at least one raw row and at least one genuine double
asset column, exactly index-length columns, finite returns, finite supplied
characteristics, and quantiles in `[0, 1]`. Structural errors, embedded-NUL
asset names, nonfinite arithmetic, and allocation/length failures cross the
existing `input_error` boundary. Unlike pandas' pervasive NaN skipping, NaN or
infinite input returns/characteristics are rejected before output construction;
source-generated NaN only remains valid for an empty style's summary return.

## Oracle, tests, and acceptance

The source oracle uses a deterministic 60-by-3 return DataFrame with columns
`ALPHA`, `BETA`, and `GAMMA`, caps `{100, 40, 10}`, value scores
`{.2, .8, .5}`, `momentum_window=5`, and size quantiles `{.3, .7}`. Python
produces ordered styles `large, mid, small, winner, loser, value, growth, high,
low`; e.g. `winner` weights are `{0, .5, .5}`, `large` return is
`.0006666666666666668`, and `low` return is `.0007083333333333334`. Native
tests record every exposure and summary value, raw-index behavior, optional
precedence/default paths, short-history NaN summaries, validation, and result
ownership.

The leaf begins with a public-header RED. A follow-up attribution-façade-only
consumer includes only `<fincore/attribution.hpp>` and captures a genuine
missing-export RED before adding one leaf include. Acceptance repeats current
and fresh Apple headless, LLVM ASan+UBSan, ImPlot compile/link-only/no-GUI,
invalid-ImGui multiline diagnostic, and FetchContent provenance gates. No
existing metrics, simulation, or visualization behavior changes.
