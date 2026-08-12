# fincore C++ Phase 27 Monte Carlo parameter-composer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> superpowers:subagent-driven-development to execute this plan task by task,
> with a fresh implementer and independent reviewer for each implementation or
> acceptance task.

**Goal:** Add a source-compatible, value-owning native counterpart to Python
`MonteCarlo.from_parameters` by composing accepted raw GBM paths and terminal
analysis.

**Architecture:** Extend the accepted `simulation/monte_carlo` leaf with one
stateless function. It applies Python's fixed-252 conversion to the documented
annual inputs, calls public `geometric_brownian_motion` exactly once, and
passes its complete price-path DataFrame exactly once to public
`analyze_simulation_paths`. The existing simulation facade already exports the
leaf, so the acceptance task extends its public-only consumer without a second
facade include.

**Tech Stack:** C++23, local cloudQuant/DataFrame, CMake, GoogleTest, local
Python oracle through `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base
python`, Homebrew LLVM 22 ASan+UBSan, and optional local Dear ImGui/ImPlot
compile/link verification.

**Baseline:** Accepted Phase 26 ends at
`1a4709eac3a8b71283f5cccef791f772cf40c88c` with 371 headless CTest cases.
Do not amend its leaf, acceptance, trace, or ignored review records.

## Global constraints

- Use only existing `fincore::dataframe`, `simulation_result`, public
  `geometric_brownian_motion`, public `analyze_simulation_paths`,
  `input_error`, and C++ standard-library facilities. Do not add Eigen,
  Python embedding, NumPy/pandas, a new RNG, providers, caches, GUI source, a
  class, options structure, callback, or overload.
- The public API must be exactly:

  ```cpp
  [[nodiscard]] simulation_result simulate_monte_carlo_from_parameters(
      double annualized_drift,
      double annualized_volatility,
      double initial_value = 1.0,
      std::size_t path_count = 1000U,
      std::size_t horizon = 252U,
      std::optional<std::uint64_t> seed = std::nullopt);
  ```

- Preserve the actual Python `MonteCarlo.from_parameters` transformation
  verbatim: `mu_daily=mu/252`, `sigma_daily=sigma/sqrt(252)`,
  `T=horizon/252`, and `dt=1/252`. This is source compatibility, not an
  attempt to reinterpret Python's annual-unit docstrings.
- The leaf must call `geometric_brownian_motion` exactly once then
  `analyze_simulation_paths` exactly once. It must retain the raw GBM complete
  price paths, including the index-zero initial row. Do not remove/subtract
  the initial row as `gbm_from_returns` does.
- A zero horizon remains valid: it produces a one-row result with every
  terminal equal to `initial_value`. Let raw GBM and terminal analysis own all
  error/finite/path-count/output-size rules; do not clean, sort, catch, or
  reword errors locally.
- An explicit seed is native mt19937_64 repeatability only; no Python/NumPy
  random-stream equality claim is permitted. No custom periods, antithetic
  option, return estimation, price-path method with optional parameters,
  `simulate` drift/volatility branch, stress wrapper, class lifecycle,
  providers, reports, packaging/release, or GUI runtime is in scope.
- All Python commands must use
  `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python`, never bare
  `python` or `python3`.

### Task 116: Record Phase 27 design and executable plan

**Files:**

- Create: `docs/design/2026-08-12-phase-27-monte-carlo-parameter-composer-design.md`
- Create: `docs/plans/2026-08-12-fincore-cpp-phase-27-monte-carlo-parameter-composer.md`

1. Record the selected stateless source-compatible design, rejected direct
   annual reinterpretation and full-class alternatives, exact API, raw-price
   output orientation, zero-horizon distinction from Phase 26, source oracle,
   validation ownership, and exclusions.
2. Record the leaf/facade/trace/final-review TDD and evidence requirements.
3. Run `git diff --check`; explicitly stage only these documents; run cached
   diff check; then commit:

   ```sh
   git commit -m "docs: plan phase 27 monte carlo parameter composer"
   ```

### Task 117: Add the explicit-parameter Monte Carlo composer leaf

**Files:**

- Modify: `include/fincore/simulation/monte_carlo.hpp`
- Modify: `src/simulation/monte_carlo.cpp`
- Modify: `tests/unit/monte_carlo_simulation_test.cpp`

1. Create the ignored Phase 27 SDD workspace, brief, ledger, and report. Run
   and retain this Anaconda source oracle from
   `/Users/yunjinqi/Documents/new_projects/fincore`:

   ```sh
   /Users/yunjinqi/opt/anaconda3/bin/conda run -n base python -c '
   from fincore.simulation.monte_carlo import MonteCarlo
   for name, kwargs in (
       ("zero_vol", dict(mu=.252, sigma=0.0, S0=2.0,
                         n_paths=2, horizon=4, seed=42)),
       ("zero_horizon", dict(mu=.252, sigma=0.0, S0=2.0,
                              n_paths=2, horizon=0, seed=42)),
   ):
       result = MonteCarlo.from_parameters(**kwargs)
       print(name, result.paths.tolist())
       print(name, result.statistics)
       print(name, result.var(.05), result.cvar(.05))
   '
   ```

2. Before adding the declaration, extend the existing leaf-only test (whose
   only fincore include remains `<fincore/simulation/monte_carlo.hpp>`) with a
   static assertion/call of the planned function. Run:

   ```sh
   cmake --build build --parallel 4
   ```

   Record the genuine missing-symbol RED; do not run CTest after the failed
   build.

3. Add the exact declaration and minimal implementation. It must materialize
   only the four source-compatible conversion arguments and compose:

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

   Do not replicate GBM propagation/statistics/risk arithmetic or add local
   validation/error translation.

4. Add public tests with no private production helper:

   - exact function-pointer type and all default-expression behavior;
   - the zero-volatility source oracle: index `{0,1,2,3,4}`, two genuine
     `double` `path_0`/`path_1` columns, all source cells, result counts,
     statistics, VaR, and CVaR;
   - zero horizon as two one-row initial-price paths with valid terminal
     statistics/risk; this specifically distinguishes Phase 26;
   - nonzero direct public parity against
     `analyze_simulation_paths(geometric_brownian_motion(...))` using the
     exact source conversion, plus same-seed equality/different-seed
     divergence without NumPy-stream claims;
   - inherited raw-GBM/result errors for NaN/infinite model values, zero path
     count, unrepresentable output/step count, and nonfinite terminal
     arithmetic. Retain finite zero/negative initial values and negative
     volatility as the accepted raw-leaf contract rather than imposing a new
     policy.

5. Run:

   ```sh
   cmake --build build --parallel 4
   ctest --test-dir build -R 'MonteCarloSimulation|GbmPaths|SimulationResult|Dataframe' --output-on-failure
   ctest --test-dir build --output-on-failure
   git diff --check
   ```

6. Dispatch a fresh independent read-only review. Correct every Critical,
   Important, or Minor finding in new scoped commits; never amend accepted
   history. Then stage exactly the three paths above and commit:

   ```sh
   git commit -m "feat: add parameter monte carlo composer"
   ```

### Task 118: Accept public simulation-facade access

**Files:**

- Modify: `README.md`
- Modify: `tests/unit/monte_carlo_simulation_umbrella_test.cpp`
- Create: `docs/acceptance/2026-08-12-phase-27-monte-carlo-parameter-composer-acceptance.md`

1. Extend the existing consumer that includes only
   `<fincore/simulation.hpp>` with the new exact function type/default
   expression and the zero-volatility price oracle. Assert raw index/schema,
   total-column/type evidence, price terminal statistics/risk, and zero
   horizon. Do not add a façade include or fabricate a missing-export RED:
   Phase 26 already exports the same leaf.
2. Document source mapping/fixed 252 transformation, complete-price-path and
   zero-horizon policies, seed/non-NumPy distinction, inherited errors,
   source oracle, genuine leaf RED/GREEN, Task 117 scope/review, public facade
   proof, and exclusions. Keep this acceptance document free of Task 118's
   SHA until Task 119.
3. Retain raw `tee` logs for current focused/full CTest, fresh Apple headless,
   clean registration-audited Homebrew LLVM ASan+UBSan, ImPlot/demo
   compile/link-only with build-root executable checked but never launched,
   invalid-ImGui multiline diagnostic, and actual GoogleTest FetchContent
   provenance. If an LLVM attempt is corrupt/time-sliced, retain it only as a
   non-acceptance diagnostic and use a unique clean retry.
4. Dispatch a fresh independent read-only document/gate review. Correct every
   Critical, Important, or Minor finding before staging exactly the three paths
   above and committing:

   ```sh
   git commit -m "docs: accept phase 27 monte carlo parameter composer"
   ```

### Task 119: Add non-self-referential traceability

**Files:**

- Modify: `docs/acceptance/2026-08-12-phase-27-monte-carlo-parameter-composer-acceptance.md`

1. Record the complete Task 117 implementation/fix chain, direct Task 118
   acceptance SHA, and exact scopes. State Task 119 is documentation-only and
   does not amend earlier commits; do not mention or predict its own SHA.
2. Run focused `MonteCarloSimulation|MonteCarloSimulationUmbrella` CTest and
   diff checks, then commit only this document:

   ```sh
   git commit -m "docs: trace phase 27 monte carlo parameter composer"
   ```

### Final Phase 27 review

Audit Task 117/118/119 scope and ancestry; API/defaults; source-compatible
fixed-252 raw-GBM-to-result composition; full price-path index/schema/order
and zero-horizon policy; source oracle; seed and inherited error boundaries;
absence of class/cache/RNG/antithetic/period/drift-volatility reinterpretation;
façade proof; raw gates, FetchContent provenance, no-GUI claims; and trace
wording. Fresh-configure Apple headless, rerun focused/full CTest and the
mandatory Anaconda oracle, then publish Spec/Task-Quality verdicts. Correct
every Critical, Important, or Minor finding in new scoped commits; do not amend
accepted history.
