# Phase 23 Latin-Hypercube Sampling Design

**Date:** 2026-08-12

## Decision

Phase 23 ports Python `fincore.simulation.paths.latin_hypercube_sampling` as a
small DataFrame-native random-design leaf. It stops before the
orientation-sensitive antithetic-path transformation and the larger
`MonteCarlo`/`SimResult` layer. A Latin-hypercube design has a stable,
independent two-dimensional shape and is useful for later simulation work.

Python returns an `(n_samples, n_dimensions)` ndarray. The native DataFrame
uses samples as rows and named genuine-double dimension columns, preserving
that orientation without an untyped matrix wrapper.

## Public API and result shape

`include/fincore/simulation/paths.hpp` gains:

```cpp
inline constexpr std::string_view latin_hypercube_dimension_column_prefix{
    "lhs_dimension_"};

dataframe latin_hypercube_sampling(
    std::size_t sample_count,
    std::size_t dimension_count,
    std::optional<std::uint64_t> seed = std::nullopt);
```

The result owns ordinal `timestamp` index `{0, ..., sample_count - 1}` and
exactly `dimension_count` columns named `lhs_dimension_0`,
`lhs_dimension_1`, and so on. Its DataFrame shape is
`(sample_count, dimension_count)`. It preserves Python's three zero-shape
cases: `(0,d)` has typed empty dimension columns; `(n,0)` retains its ordinal
index with no columns; `(0,0)` is empty both ways.

`<fincore/simulation.hpp>` already includes `paths.hpp`, so this extension is
automatically available through the simulation facade. A leaf-only and a
facade-only public consumer will capture the missing-symbol RED before the
declaration; no redundant facade include is needed.

## Sampling and RNG semantics

For every dimension, the operation creates a permutation of all
`sample_count` strata and produces one point per row:

```text
(permutation[row] + open_unit_uniform) / sample_count
```

For every nonempty dimension this yields exactly one value in each source
half-open stratum `[k/n, (k+1)/n)` and all values are in `[0,1)`.

Python uses `numpy.default_rng(seed).permutation` and `uniform`. Native reuses
the accepted local `std::mt19937_64` engine and open-unit uniform primitive
from the GBM leaf, adding only a private rejection-mapped bounded index for a
local Fisher-Yates permutation. An explicit seed repeats within the supported
C++ build, but no NumPy stream equality is claimed. An absent seed uses a
local `std::random_device` seed. No global engine, external RNG parameter,
selector, input DataFrame, or NumPy interoperability is introduced.

## Safety boundary

Counts are `std::size_t`; Python negative-size errors are outside this stronger
C++ type boundary. Before allocation or random work, native validates ordinal
index, column-name construction, vector capacity, and
`sample_count * dimension_count` representability. Impossible shape/name/cell
counts and allocation/length failures become `fincore::input_error`. Positive
generation validates a finite denominator and every output in `[0,1)`. It
does not clip, sort, deduplicate, correlate, normal-transform, or simulate
paths after generation.

## Oracle, verification, and exclusions

The Anaconda source oracle records the seed-42 `4x3` and `1x3` arrays, their
per-dimension stratum coverage, zero shapes `(4,0)`, `(0,3)`, `(0,0)`, and
negative-size errors. Native tests use it for shape/range/stratum semantics,
not a nonconstant random-stream parity claim. They prove exact schema/index,
all zero shapes, range/coverage, fixed-seed repeatability/difference, finite
unseeded output, safe count/name/cell guards, output ownership, and automatic
facade export.

Acceptance retains current, fresh AppleClang, LLVM ASan+UBSan, ImPlot/demo
compile-link-only/no-GUI, invalid-ImGui, and FetchContent provenance evidence.
Antithetic variates, MonteCarlo, SimResult, terminal simulation statistics,
VaR/CVaR, calibration, bootstrap changes, full NumPy RNG parity, bindings,
providers, reports, packaging, and GUI runtime remain excluded.
