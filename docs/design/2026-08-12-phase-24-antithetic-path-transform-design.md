# Phase 24 Antithetic Path Transform Design

## Purpose and boundary

Phase 24 ports the small Python `antithetic_variates` primitive from
`fincore.simulation.paths` without introducing the larger `MonteCarlo` or
`SimResult` layers. Python represents paths as rows and time steps as columns,
then vertically appends a mirrored copy around `paths[0, 0]`. Native fincore
uses the already accepted inverse orientation: timestamp/index rows and named
path columns. The DataFrame-native counterpart therefore preserves the raw
index and appends a mirror column for every existing path column. This is the
same doubling of paths, not a new matrix wrapper or a transpose-oriented API.

## Public API and shape

`include/fincore/simulation/paths.hpp` will expose:

```cpp
inline constexpr std::string_view antithetic_path_column_prefix{
    "antithetic_"};

dataframe antithetic_variates(const dataframe& paths);
```

For source columns `path_0`, `path_1`, ..., the result has the original
columns first, followed by `antithetic_path_0`, `antithetic_path_1`, ... in
the same source-column order. It owns an exact copy of the input index,
including raw non-monotonic or duplicate timestamps. If `initial` is the first
value of the first source double column, every appended value is
`2.0 * initial - source_value`. The original columns are copied by value.

The input must have a nonempty index and at least one path column; every data
column must be a genuine `double` column of index length. Empty tables and
mixed/non-double path tables raise `fincore::input_error`. Derived names must
fit and must not collide with any input name. The native leaf deliberately
preserves ordinary IEEE arithmetic for finite overflow, NaN, and signed
infinity just as NumPy's direct expression does; it does not filter, clamp, or
invent a finite-only input policy.

## Implementation and validation

The paths leaf will validate the input's exact double-only schema and output
name/capacity/cell representability before constructing a result. It will load
the copied index, then materialize original and antithetic columns by value.
It will neither sort/deduplicate timestamps nor change existing GBM, LHS,
bootstrap, correlation, stress, or simulation-facade behavior.

Python source-oracle fixtures cover the canonical two-path mirror, singleton,
nonfinite propagation, and empty-array failures. C++ tests additionally cover
the native column mapping, collisions, wrong types, malformed/empty frames,
raw index preservation, and source-lifetime ownership. The existing
`<fincore/simulation.hpp>` façade already includes `paths.hpp`, so a
façade-only consumer proves automatic export without a redundant umbrella
change.

## Exclusions

This phase excludes normal/correlated transforms, LHS changes, external RNGs,
GBM/calibration changes, `MonteCarlo`, `SimResult`, VaR/CVaR, providers,
reports, packaging/release work, and GUI runtime behavior.
