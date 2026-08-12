# Phase 27 native Monte Carlo parameter-composer design

## Scope decision

Phase 26 deliberately implemented only the historical-return workflow of
Python's mutable `MonteCarlo` class. Its design left Python's static
`MonteCarlo.from_parameters` path constructor for a separate slice because it
has a different output unit: it preserves complete price paths, including the
initial observation, rather than emitting cumulative return paths with their
initial row removed.

Three approaches were considered:

1. **Selected — a source-compatible stateless parameter composer.** Add one
   native function to the existing Monte Carlo leaf. It converts the source's
   fixed-252 parameter inputs, delegates to the accepted raw GBM leaf once,
   and wraps the resulting price table with the accepted terminal-result
   factory once.
2. **Call raw GBM with the advertised annual inputs directly.** This would be
   a cleaner new interpretation, but differs from the actual Python
   `from_parameters` transformation and would make the native mapping
   surprising beside accepted `gbm_from_returns` semantics.
3. **Add the full mutable `MonteCarlo` class.** That would combine historical
   storage, `price_paths`, `simulate`, resimulation VaR/CVaR, stress helpers,
   and several Python-only parameter branches. It is too broad and would
   duplicate established leaves.

The selected API is:

```cpp
[[nodiscard]] simulation_result simulate_monte_carlo_from_parameters(
    double annualized_drift,
    double annualized_volatility,
    double initial_value = 1.0,
    std::size_t path_count = 1000U,
    std::size_t horizon = 252U,
    std::optional<std::uint64_t> seed = std::nullopt);
```

The parameter order maps Python's
`MonteCarlo.from_parameters(mu, sigma, S0, n_paths, horizon, seed)` while
using native names that state the public units. This remains a stateless
function, not a class or options object.

## Source-compatible composition

Python's `from_parameters` has the following actual transformation before it
calls `geometric_brownian_motion`:

```text
dt          = 1.0 / 252
mu_daily    = mu / 252
sigma_daily = sigma / sqrt(252)
T           = horizon / 252
```

Although the source docstrings call `mu` and `sigma` annual values, this is
the already accepted Phase 19 source-compatible conversion. Phase 27 preserves
it rather than reinterpret the inputs. The native implementation performs the
single public composition:

```cpp
return analyze_simulation_paths(geometric_brownian_motion(
    initial_value,
    annualized_drift / 252.0,
    annualized_volatility / std::sqrt(252.0),
    static_cast<double>(horizon) / 252.0,
    1.0 / 252.0,
    path_count,
    seed));
```

It does not implement a separate random engine, path loop, statistics, VaR,
CVaR, input cleaner, or error translation. `geometric_brownian_motion` owns
finite/model/shape/path-count/seed behavior; `analyze_simulation_paths` owns
the value-owning result and terminal analysis.

## Output and boundary contract

The result owns a native path table whose raw ordinal index represents GBM
observations, including the initial row `0`. It has named genuine-`double`
`path_N` columns in order, one per input path. The values are prices, not the
cumulative returns returned by Phase 26's historical-return composer. A zero
horizon delegates to raw GBM's one-row initial-price table and is therefore a
valid result; this intentionally differs from Phase 26, where removal of the
initial price row leaves an empty return-path frame that terminal analysis
rejects.

For the local source oracle:

```text
MonteCarlo.from_parameters(mu=.252, sigma=0.0, S0=2.0,
                           n_paths=2, horizon=4, seed=42)
```

both Python paths are:

```text
[2.0, 2.0000079365236836, 2.0000158730788615,
 2.000023809665534, 2.0000317462837005]
```

and the terminal mean, median, VaR, and CVaR are
`2.0000317462837005`, with zero sample standard deviation. A zero horizon
returns two one-element paths `[[2.0], [2.0]]`. Nonzero-volatility tests must
compare to the explicit public native composition under the same native seed,
not to NumPy's random stream.

Python's behavior for zero/negative path counts, nonfinite inputs, and output
shape is not adopted where it conflicts with the existing native leaves:
native `input_error` propagation is intentional. Finite zero and negative
initial values and finite negative volatility retain the raw GBM leaf's
established behavior. No caller period, antithetic option, return-frame
parameter estimation, `price_paths` convenience branch, `simulate` drift or
volatility keyword branch, stress-table wrapper, provider, report, package,
or GUI-runtime work is included.

## Verification and acceptance

The leaf receives a genuine public-header RED before its new declaration. Its
tests must assert exact signature/default expressions, the deterministic
zero-volatility source oracle, full price-path index/schema/statistics/risk,
zero-horizon policy, direct public-leaf parity for a nonzero seeded fixture,
same/different native seed behavior, and inherited error boundaries. A later
facade-only consumer continues to include only `<fincore/simulation.hpp>`.
Because Phase 26 already exports the Monte Carlo leaf, that consumer proves
the additional public function without fabricating another missing-export RED
or changing the façade again.

Acceptance records current/fresh AppleClang headless, registration-audited
Homebrew LLVM ASan+UBSan, ImPlot/demo compile/link-only without launching the
demo, invalid-ImGui multiline diagnostic, and actual GoogleTest FetchContent
provenance. It also states the source-compatible unit conversion and excludes
full Python-class and NumPy-stream claims.
