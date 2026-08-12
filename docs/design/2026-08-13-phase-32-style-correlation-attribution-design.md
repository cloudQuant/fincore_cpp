# Phase 32 Native Style Correlation Attribution Design

## Scope decision

Phase 29 accepted native static style analysis, Phase 30 accepted
performance-by-style, and Phase 31 accepted rolling style tilts. The remaining
Python `fincore.attribution.style` calculation with a successful local path is
`calculate_regression_attribution`.

Three continuations were considered:

1. **Selected — native style correlation attribution.** Its automatic path is
   dependency-closed by the accepted style-analysis and performance-by-style
   APIs, while its explicit path requires only stable timestamp intersection,
   means, standard deviations, Pearson correlation, and a residual.
2. **Small scalar parity debt elsewhere.** Helpers such as
   `common_sense_ratio` may require fewer lines, but switching modules now
   would leave the public style-analysis route incomplete. The selected slice
   is the smallest useful continuation of the current attribution/style line,
   not the smallest function in the whole Python package.
3. **`fetch_style_factors`.** Python has no successful implementation: it
   logs and unconditionally raises until a provider is configured. Porting an
   always-failing stub would add provider/logging policy without analytical
   capability.

Python `StyleResult` dictionary conveniences remain Python-only because the
native `style_analysis_result` already owns and names its three tabular
results. This phase adds no provider, network call, SciPy/NumPy dependency,
regression solver, embedded Python, plot, report, package, or GUI runtime.

## Name versus formula

The source function calls its coefficient `style_beta`, but it does **not**
fit a regression and does not calculate an OLS slope. For each style it uses:

```text
coefficient  = PearsonCorr(portfolio_return, style_return)
contribution = coefficient * mean(style_return)
residual     = mean(full_portfolio_return) - sum(contributions)
```

The public name remains `calculate_regression_attribution` for source API
discoverability. Documentation calls the calculation **style correlation
attribution** and never describes it as QR, OLS, a factor-regression beta, or
a multivariate fit. The implementation must not delegate to
`fit_factor_regression` or replace correlation with covariance divided by
portfolio or style variance.

## Public API and DataFrame mapping

Extend `<fincore/attribution/style.hpp>` with:

```cpp
inline constexpr std::string_view style_attribution_contribution_column{
    "contribution"};
inline constexpr std::string_view style_attribution_residual_name{"residual"};

[[nodiscard]] dataframe calculate_regression_attribution(
    const dataframe& asset_returns);

[[nodiscard]] dataframe calculate_regression_attribution(
    const dataframe& portfolio_returns,
    const dataframe& style_returns,
    std::span<const std::string_view> selected_styles);
```

The one-argument overload is the automatic composer. `asset_returns` is a
time-by-asset frame and follows the accepted `style_analysis` input contract.

The explicit overload maps both Python portfolio forms without a variant
type. Every genuine-double column in `portfolio_returns` is one portfolio
component; a single column is the Series case, while multiple columns are
averaged across columns at every row before attribution. The frame may not
contain other typed columns.

`style_returns` is time-by-style. `selected_styles` replaces Python's
`style_exposures` argument because Python never reads exposure rows, values,
types, or index; it only uses exposure column membership as a whitelist. A
selector not present in `style_returns` is ignored, matching an extra exposure
column. Selector order is not output order: selected styles remain in the
stable `style_returns` double-column order, matching Python's iteration order.
An empty selector therefore produces a residual-only result.

The output owns an ordinal timestamp index, the existing genuine string
`style` column, and one genuine double `contribution` column. Selected style
rows come first in stable style-return column order; a `residual` row is
always last. There are no hidden columns and no input timestamp is reused as
an output identity.

## Explicit alignment and numerical semantics

The explicit overload requires unique portfolio and style-return timestamp
labels. Labels may be nonmonotonic. For every selected style, common labels
are collected in portfolio left-hand order. This deliberately rejects
pandas' duplicate-label `.intersection(...).loc[...]` expansion, which can
broadcast unequal duplicate counts or fail with unrelated array shapes. The
automatic overload remains positional and accepts duplicate or nonmonotonic
raw asset-return labels because all derived frames have the same row order.

Let `P` be the row-wise portfolio mean over every portfolio column and let
`S_j` be a selected style-return column. For each style:

```text
common_j = stable unique timestamp intersection(P, S_j)

if size(common_j) < 3:
    contribution_j = 0
else:
    p_std = population_std(P on common_j)
    s_std = population_std(S_j on common_j)
    if p_std < 1e-15 or s_std < 1e-15:
        contribution_j = 0
    else:
        corr_j = population_covariance(P, S_j) / (p_std * s_std)
        contribution_j = corr_j * mean(S_j on common_j)

portfolio_mean = mean(P over every portfolio row)
residual = portfolio_mean - sum(contribution_j)
```

The `< 1e-15` comparison is strict, as in Python. At least three common rows
are required even though two points could define a correlation. Each style's
mean uses only its own common rows, while the residual starts from the full
portfolio mean rather than any common subset. Centered checked accumulation is
used for mean, population variance, covariance, correlation, contribution,
attributed sum, and residual. Correlation is not clamped.

## Automatic composition

For a nonempty-shaped input, the one-argument overload:

1. calls public `style_analysis(asset_returns)` exactly once;
2. calls public
   `analyze_performance_by_style(asset_returns, analysis.exposures)` exactly
   once;
3. consumes `analysis.overall_returns` as the equal-weight portfolio series;
4. consumes each `{style}_return` performance column positionally against the
   portfolio series; and
5. emits unsuffixed style names in the accepted exposure-column order.

It does not call the one-argument performance overload, which would repeat
`style_analysis`, and it does not align the ordinal performance index against
the raw asset-return index. The shared numerical kernel therefore supports an
explicit timestamp-aligned mode and an automatic positional mode.

## Empty, validation, and ownership policy

An empty-shaped automatic input or explicit `portfolio_returns` input returns
a canonical freshly constructed `0x0` DataFrame before nonempty validation.
For a nonempty portfolio, zero selected/present style columns returns exactly
one residual row. A selected style with fewer than three common timestamps,
including a zero-row style column, receives a zero contribution.

Nonempty explicit computation requires:

- at least one genuine-double, exact-index-length portfolio column and no
  other typed column; finite values; nonempty, NUL-free, distinct names;
- unique portfolio and style-return timestamps, with nonmonotonic order valid;
- every selected style that is present to be a genuine-double,
  exact-index-length, finite column; unselected columns remain unread;
- selected names to be nonempty, NUL-free, distinct, representable by
  cloudQuant DataFrame's fixed column-name storage, and different from the
  reserved `residual` name;
- representable input/output counts, ordinal index, names, aligned pairs, and
  cells; and finite checked arithmetic throughout.

Python skips NaN pairs and allows signed infinity to leak into warnings, NaN,
or infinite residuals. The native boundary instead rejects nonfinite
portfolio values and selected style values before calculation and rejects
nonfinite intermediate or result arithmetic. Allocation, length, malformed
DataFrame-library, name, duplicate-index, and numerical failures cross the
public boundary as `fincore::input_error`.

Every selector is copied before DataFrame lookup. The result owns its index,
style strings, and contributions and remains valid after either input frame,
selector backing storage, or automatic intermediate is mutated or destroyed.

## Source oracle and mutation-resistant tests

The primary explicit Python oracle uses portfolio index
`{30,10,50,20,40}`, values `{.03,.01,.05,.02,.04}`, style index
`{40,20,50,10,30}`, and style columns in order `growth,value,unselected`.
Exposure columns are ordered `value,growth` and contain unused `NaN/+Inf`
values. Python returns:

```text
growth   0.06
value   -0.03
residual 0.0
```

This proves stable style-return order, reordered timestamp alignment, unused
exposure data, centered Pearson behavior, common style means, and residual
conservation. A two-column portfolio oracle yields
`growth=.018000000000000006`, `value=-.009000000000000003`, and
`residual=.061000000000000006`.

The automatic five-row, three-asset oracle uses raw timestamps
`{42,-7,42,100,3}` and produces, in order:

```text
equal_weight  .009333333333333332
winner        .002613098319673955
loser         .0007593263966019986
value         .009333333333333332
growth        .009333333333333332
high          0
low           0
residual     -.02203909138294262
```

Tests additionally distinguish Pearson correlation from uncentered dot
product and OLS slope; lock the strict variance threshold, common-only style
mean, full-portfolio residual mean, fewer-than-three and constant branches;
prove selector filtering versus style-return order; reject explicit duplicate
timestamps, reserved/duplicate/embedded-NUL selectors, wrong types, short
columns and nonfinite values; exercise empty/residual-only surfaces, capacity
and checked arithmetic; verify automatic public composition, duplicate raw
labels, exact schema/types/order/count, and value ownership. Leaf-only and
`<fincore/attribution.hpp>`-only consumers must capture a genuine missing-API
compile RED before the declaration exists.
