# Phase 26 native Monte Carlo simulation-composer design

## Scope decision

Python's `fincore.simulation.monte_carlo.MonteCarlo` combines historical-return
storage, GBM generation, antithetic handling, terminal-risk analysis, price
paths, stress reporting, and convenience `var`/`cvar` methods. The accepted
native leaves already provide the three stable parts required by
`MonteCarlo.simulate`: calibrated GBM paths, antithetic transformation, and a
value-owning terminal-path result.

Three approaches were considered:

1. **Selected — stateless simulation composer.** Add one public function that
   composes the three accepted leaves into a `simulation_result`. It directly
   covers the useful `MonteCarlo.simulate` workflow without adding mutable
   state or reimplementing numerical behavior.
2. **Full native `MonteCarlo` class now.** This would own historical returns
   and include `simulate`, `var`, `cvar`, `price_paths`, stress methods, and
   parameter constructors. It is too broad: it mixes accepted simulation and
   stress leaves with Python's unused seed/parameter branches.
3. **Another terminal-statistics overload.** This duplicates the accepted
   Phase 25 result factory and does not provide an end-to-end simulation entry
   point.

Phase 26 selects option 1. It creates an explicit native convenience API, not
a one-to-one class emulation.

## Public API and composition

Create `<fincore/simulation/monte_carlo.hpp>`:

```cpp
[[nodiscard]] simulation_result simulate_monte_carlo(
    const dataframe& returns,
    std::size_t path_count = 1000U,
    std::size_t horizon = 252U,
    bool antithetic = false,
    std::optional<std::uint64_t> seed = std::nullopt,
    std::string_view column = returns_column);
```

The function performs exactly this public-leaf composition:

```cpp
dataframe paths = gbm_from_returns(
    returns, horizon, path_count, 252U, seed, column);
if (antithetic) {
    paths = antithetic_variates(paths);
}
return analyze_simulation_paths(paths);
```

The fixed `252U` is intentional: Python `MonteCarlo.simulate` always invokes
`gbm_from_returns(..., frequency=252)`. A future lower-level native API can
continue using the already public `gbm_from_returns` period parameter. The
composer must not add ignored drift/volatility arguments: Python exposes them
on `simulate` but does not use them. Callers needing known parameters continue
to use the accepted `geometric_brownian_motion` and `analyze_simulation_paths`
leaves directly until a separately designed price-path slice exists.

The returned result retains native orientation: index rows are simulated time
steps and named genuine-`double` columns are paths. Without antithetic mode it
has exactly `path_count` `path_N` columns; antithetic mode appends matching
`antithetic_path_N` columns in the accepted transform's order. The result owns
all output paths/statistics, so source returns and intermediate frames have no
lifetime coupling.

## Semantics and boundaries

Python source oracle for constant returns `[.01, .01, .01, .01]`, two paths,
horizon four, seed 42 gives each base path:

```text
[0.00003968332704484823, 0.00007936822885623585,
 0.00011905470549655739, 0.0001587427570282074]
```

With `antithetic=True`, Python appends two paths mirrored about the first
value, ending at `-0.00007937610293851094`; the terminal mean is
`0.00003968332704484823` and sample standard deviation is
`0.00013747798790091163`. The native tests use these deterministic,
zero-volatility numbers for source parity. For nonconstant returns they assert
only direct public-leaf composition and native seed repeatability, never NumPy
random-stream equality.

All validation remains delegated. `gbm_from_returns` owns selector (including
embedded-NUL), returns-column, retained-NaN, finite, sample, period, path-count
and numerical validation; `antithetic_variates` owns transformation shape/name
checks; `analyze_simulation_paths` owns nonempty terminal-frame and
finite-terminal checks. The composer adds no parallel filtering, sorting,
statistics, random engine, or error translation. A zero horizon produces an
empty native path table and is rejected by the result invariant as
`input_error`; Python instead reaches an `IndexError` while deriving terminal
statistics. This deliberate native boundary is documented rather than hidden.

## Test and acceptance strategy

The leaf receives a genuine public-header RED test before its declaration.
Its GREEN tests cover exact signature/default expression; Python constant-path
oracle in native orientation; all terminal statistics/VaR/CVaR; antithetic
schema/mirror/path doubling; direct public-composition parity on nonconstant
data; same/different seed behavior; invalid input propagation; zero horizon;
and ownership after source mutation/destruction. The later façade-only
consumer includes only `<fincore/simulation.hpp>` and proves the full result
contract.

Acceptance retains current/fresh AppleClang headless, registration-audited
Homebrew LLVM ASan+UBSan, ImPlot/demo compile/link-only without launching GUI,
invalid-ImGui multiline diagnostic, and actual FetchContent GoogleTest
provenance. It explicitly excludes a class, cached results, price paths,
`from_parameters`, stress-table wrapping, standalone var/cvar resimulation,
custom periods, providers, reports, full Python/NumPy RNG parity, and GUI
runtime work.
