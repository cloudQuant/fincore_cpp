# Phase 31 Native Rolling Style Tilts Design

## Scope decision

Phase 30 accepted the static performance-by-style consumer. The next smallest
dependency-closed Python style API is
`fincore.attribution.style.calculate_style_tilts`: it uses only pandas/NumPy
ranking, compounding, and sample-standard-deviation behavior and requires no
factor provider, regression solver, SciPy, plot, or Python runtime.

This phase intentionally excludes `calculate_regression_attribution`. That
function calls correlation a beta and needs a separate semantic decision. It
also excludes `fetch_style_factors`, which still always raises until a provider
is configured. Python `StyleResult` mapping conveniences remain Python-only.

## Public API and deliberate native boundary

Extend `<fincore/attribution/style.hpp>` with:

```cpp
[[nodiscard]] dataframe calculate_style_tilts(
    const dataframe& asset_returns,
    std::size_t window = 252U);
```

Python accepts a `factor_returns` argument but never reads it. The native API
omits that dead and misleading parameter rather than silently accepting unused
factor data. A typed `std::size_t` expresses the native nonnegative window
domain and removes Python/NumPy floating-point slice semantics. Ordinary C++
integral conversions still apply, so callers that require stricter source-type
validation must perform it before the call.

`asset_returns` is time-by-asset. Its raw timestamp index is positional data,
and every stable named column is a genuine `double` asset column. The result
owns a copy of the input index suffix beginning at `effective_window` and owns
all names and values. It does not sort or deduplicate timestamps.

Python pre-creates that suffix and then writes with `.loc[label]`; duplicate
labels therefore broadcast and overwrite multiple rows. Native code preserves
duplicate and nonmonotonic raw labels but computes each output row by position.
This is an explicit correction of a pandas label-assignment side effect, not a
different rolling formula.

## Window and output mapping

For `T` observations:

```text
effective_window = min(window, T - 1)
```

The implementation first handles `T == 0` to avoid unsigned underflow. Any
empty-shaped input, or `effective_window < 2`, returns a canonical freshly
constructed `0x0` DataFrame. This preserves Python's `window == 0/1` and short
history behavior while normalizing Python's unusual indexed zero-column frame
to the project's safe empty surface.

Otherwise there are `T - effective_window` output rows. Output position for
input position `t` uses only `asset_returns[0:t]`, so the current row `t` is
excluded. The raw input index suffix `[effective_window,T)` is copied exactly.

The output contains genuine-double binary columns in Python's insertion order:

1. for every asset in stable input order: `{asset}_large`, `{asset}_small`;
2. for every asset: `{asset}_winner`, `{asset}_loser`;
3. for every asset: `{asset}_value`, `{asset}_growth`;
4. for every asset: `{asset}_high_vol`, `{asset}_low_vol`.

There are no hidden or identity columns.

## Exact formulas

All ranks are ascending, one-based, and use the average rank for exact ties.
Names retain the current Python source semantics even where economically
counterintuitive.

For each output position `t`:

- Size uses the last excluded-history row `t-1`. `large` is
  `rank <= N/3`; `small` is `rank > 2*N/3`. Thus low prior returns receive the
  `large` label and middle ranks receive neither label.
- Momentum compounds the last `min(window,t)` excluded-history returns as
  `product(1+r)-1`, with checked finite arithmetic. `winner` is strictly
  positive; zero and negative values are `loser`.
- Value/growth average-ranks the same cumulative returns. `value` is
  `rank <= N/2`; `growth` is `rank > N/2`. Low cumulative returns therefore
  receive the `value` label.
- Volatility uses the last `min(60,t)` excluded-history returns, sample
  standard deviation (`ddof=1`), and the linear median across assets. Values
  equal to the median are `high_vol`; values below it are `low_vol`.

All calculations follow stable asset and row order. Products, Welford sample
variance intermediates, medians, and final values must remain finite.

## Validation, capacity, and ownership

Nonempty computation requires only genuine-double, exact-index-length, finite
return columns with nonempty, embedded-NUL-free, distinct asset names. Derived
names must fit cloudQuant DataFrame's fixed column-name storage without
truncation: `asset.size() + suffix.size()` is strictly less than
`dataframe::ColNameType::capacity()`. All eight names per asset are validated
for uniqueness before materialization.

The implementation preflights `8*N`, output rows, output cells, index copies,
and vector capacities without constructing invalid spans or impossible test
objects. Malformed structure, nonfinite input or arithmetic, unrepresentable
sizes/names, `std::bad_alloc`, and `std::length_error` cross the public boundary
as `fincore::input_error`. Every output survives source mutation and
destruction.

## Source oracle and acceptance

The primary Python oracle has five rows, assets `A/B/C`, `window=3`, and raw
timestamps `{42,-7,100,3,55}`. Returns are:

```text
A: {0, .25, -.5, 0, .75}
B: {0, 0, -.5, -.5, .75}
C: {0, -.5, 1, .25, .75}
```

It emits raw suffix `{3,55}` and 24 columns in the four blocks above. The two
rows prove average-rank ties, current-row exclusion, trailing compounding,
zero-as-loser, volatility ordering, median equality, and exact output order.
With finite equal-length asset histories, changing `ddof=1` to `ddof=0`
rescales every cross-sectional volatility by the same positive factor, so the
binary high/low surface cannot distinguish that mutant. The exact `ddof=1`
formula is therefore closed by source-derived design, checked implementation
review, and finite-intermediate tests rather than an overstated public golden.

Tests additionally pin `window=0/1`, short and empty-shaped inputs, default
window, fixed-name and finite failures, raw nonmonotonic order, duplicate raw
labels with positional non-broadcast results, exact schema/types/count, and
value ownership. Leaf-only and attribution-facade-only consumers capture a
genuine missing-declaration compile RED before the declaration exists.

Acceptance repeats current focused/full CTest, fresh Apple headless,
registration-audited Homebrew LLVM ASan+UBSan, local ImPlot/demo
compile/link-only without launch, invalid-ImGui multiline validation, actual
FetchContent provenance, the mandatory Anaconda oracle, and independent
specification/task-quality reviews.
