# Phase 30 Native Performance-by-Style Design

## Scope decision

Phase 29 accepted the deterministic `style_analysis` leaf and left four
Python style APIs open. Three dependency-closed continuations were considered:

1. **Selected — `analyze_performance_by_style`.** It consumes static
   asset-by-style exposures, normalizes them in asset-name order, and emits a
   per-period style-return DataFrame. Its automatic path directly composes the
   already accepted `style_analysis` result, so it closes the next public
   source boundary without a solver, provider, or new statistical policy.
2. **Rolling `calculate_style_tilts`.** This is deterministic, but it adds a
   dead Python `factor_returns` parameter, pandas average-rank tie behavior,
   an exclusionary history window, and sample-standard-deviation semantics.
   It remains a separate later phase rather than being hidden inside this
   simpler consumer.
3. **Regression attribution.** The Python function calls a correlation
   “beta,” supports automatic and explicit input shapes, and uses exposure
   columns partly as a filter. That semantic mismatch with the existing native
   factor-regression surface requires an explicit design decision of its own.

`fetch_style_factors` is also excluded because Python always raises until a
provider is configured. This phase adds no provider, network access, SciPy,
plotting, report, package, or GUI-runtime work.

## Public API and DataFrame mapping

Extend `<fincore/attribution/style.hpp>` with two value-returning overloads:

```cpp
[[nodiscard]] dataframe analyze_performance_by_style(
    const dataframe& asset_returns);

[[nodiscard]] dataframe analyze_performance_by_style(
    const dataframe& asset_returns,
    const dataframe& style_exposures);
```

`asset_returns` is time-by-asset: raw rows are observations and every genuine
`double` column is a named asset. The explicit `style_exposures` input uses the
accepted Phase 29 representation: ordinal rows, one genuine string `asset`
identity column, and ordered genuine-double style columns. Exposure rows align
to return columns by exact asset name, independent of exposure row order.
Missing return assets receive zero weight; extra exposure assets are ignored,
matching pandas `reindex(returns.columns, fill_value=0.0)`.

The result uses ordinal timestamp index `0..observation_count-1`, intentionally
matching Python's `Period` index rather than retaining the raw return index.
For every style, it contains one genuine-double column named
`{style}_return`, in the stable input style-column order. It has no identity or
hidden extra column and owns its index, names, and values.

The one-argument overload returns an empty `0x0` frame for an empty-shaped
return input. Otherwise it calls the public `style_analysis(asset_returns)`
default path once and delegates the returned exposure frame to the explicit
overload. The explicit overload likewise returns an empty `0x0` frame when
returns are empty-shaped or exposures have no rows or no genuine-double style
columns. The native string `asset` column is representation metadata, so an
asset-only exposure frame corresponds to pandas' empty `N x 0` style table.

## Algorithm and validation

For each style, rebuild a finite weight vector in the exact return-asset
column order. Sum the aligned weights in that order. When the total is
positive, divide every weight by the total, preserving finite negative hedge
weights just as Python does. When the total is zero or negative, emit `0.0` at
every period. For a positive-total style, compute the ordered dot product of
each return row and normalized weights. This intentionally does not reuse the
Phase 29 nonnegative-mask normalization rule.

Nonempty computation requires:

- only genuine-double, exact-index-length, finite return columns with
  nonempty, NUL-free, distinct asset names;
- exactly one genuine string `asset` exposure column plus zero or more
  genuine-double style columns and no other typed columns;
- exact-index-length exposure columns; nonempty, NUL-free, distinct exposure
  asset identities; nonempty, NUL-free style names; and finite weights;
- finite weight sums, normalized weights, products, and row sums;
- representable ordinal indexes and output names/capacities.

Malformed structure, nonfinite input/arithmetic, and representability or
allocation failures raise `fincore::input_error`. Duplicate/nonmonotonic raw
return indexes remain valid because computation is positional, while the
output index is always ordinal. Every output is freshly materialized and
remains valid after both input frames are mutated or destroyed.

## Oracle and acceptance

The mandatory source oracle uses five rows, assets `ALPHA/BETA/GAMMA`, and raw
index `{42,-7,42,100,3}`. Explicit styles are `blend={2,1,0}`,
`hedged={1,-.5,.5}`, `zero={0,0,0}`, and
`negative_total={-1,0,0}`. Python yields ordinal index `0..4`, ordered columns
`blend_return, hedged_return, zero_return, negative_total_return`, and:

```text
{.013333333333333332, -.005, 0, 0}
{-.009999999999999998, -.010000000000000002, 0, 0}
{.016666666666666663, .045, 0, 0}
{.013333333333333332, -.03, 0, 0}
{-.006666666666666666, .015000000000000001, 0, 0}
```

The same oracle records the automatic default styles and proves reordered
exposure rows are equivalent. Native tests independently compute every dot
product, cover missing/extra assets, empty surfaces, validation, exact schema,
ordinal output, duplicate raw input indexes, and value ownership.

Both leaf-only and attribution-façade-only consumers are registered before the
new declaration to capture a genuine public-surface RED. Acceptance repeats
current focused/full CTest, fresh Apple headless, registration-audited
Homebrew LLVM ASan+UBSan, local ImPlot/demo compile/link-only without launch,
invalid-ImGui multiline validation, actual FetchContent provenance, and fresh
independent reviews.
