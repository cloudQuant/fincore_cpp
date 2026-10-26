# Phase 11 Native Stress Scenarios Design

**Date:** 2026-08-12

## Decision

Phase 11 adds the deterministic return-stress subset of Python
`fincore.simulation.scenarios` as a DataFrame-native leaf module.  The C++
surface takes one explicitly named, genuine-`double` return column; produces
one value-owning result per requested scenario; and uses a typed summary
`dataframe` in place of Python's presentation-oriented pandas table.  It does
not introduce a random-number engine, a matrix dependency, providers, or a
simulation model.

The source is
`/Users/yunjinqi/Documents/new_projects/fincore/fincore/simulation/scenarios.py`.
For finite inputs, the BHB-independent arithmetic is directly equivalent to
the Python functions: crash replaces the midpoint return with `-0.20`, spike
replaces it with `0.10`, volatility crush multiplies every return by `0.5`,
volatility spike multiplies every return by `2.0`, and a custom scenario first
adds its shift then multiplies.  A direct Anaconda oracle on
`[.01, -.02, .03, -.01]` establishes the Phase 11 finite goldens.

## Public shape

The leaf public header will be
`include/fincore/simulation/scenarios.hpp`; a later
`include/fincore/simulation.hpp` umbrella exports only this leaf.

```cpp
inline constexpr std::string_view stress_scenario_name_column{"scenario"};
inline constexpr std::string_view stress_cumulative_return_column{
    "cumulative_return"};
inline constexpr std::string_view stress_max_drawdown_column{"max_drawdown"};
inline constexpr std::string_view stress_volatility_column{"volatility"};
inline constexpr std::string_view stress_final_value_column{"final_value"};

enum class predefined_stress_scenario {
    crash,
    spike,
    volatility_crush,
    volatility_spike,
};

struct custom_stress_scenario {
    std::string name;
    double return_shift{0.0};
    double return_multiplier{1.0};
};

struct stress_scenario_result {
    std::string name;
    dataframe stressed_returns;
    double cumulative_return;
    double max_drawdown;
    double volatility;
    double final_value;
};

std::vector<stress_scenario_result> stress_test(
    const dataframe& returns,
    std::string_view column = returns_column);

std::vector<stress_scenario_result> stress_test(
    const dataframe& returns,
    std::span<const predefined_stress_scenario> scenarios,
    std::span<const custom_stress_scenario> custom_scenarios = {},
    std::string_view column = returns_column);

dataframe scenario_table(
    std::span<const stress_scenario_result> results);
```

`stress_test(returns)` emits the four predefined scenarios in Python's source
order: `crash`, `spike`, `vol_crush`, `vol_spike`.  The explicit overload emits
predefined scenarios in supplied order followed by custom scenarios in supplied
order.  The returned vector and `scenario_table` deliberately preserve those
orders.  A `stress_scenario_result` owns its scenario name and its transformed
DataFrame, so it is independent of the input and of other result rows.

`scenario_table` uses synthetic ordinal `timestamp` values `0..N-1` because a
scenario is not a market timestamp.  Its exact schema is one genuine
`std::string` column, `scenario`, followed by the four genuine-`double`
columns `cumulative_return`, `max_drawdown`, `volatility`, and `final_value`.
It is numeric rather than Python's percent-formatted presentation table;
formatting remains a reporting concern.  Empty result input yields this typed
empty schema.

## Input contract and arithmetic

The selected return column must be a nonempty name, exist, be genuine
`double`, and exactly match the raw timestamp-index length.  The input index
is intentionally neither sorted nor deduplicated: scenario transformations
operate on supplied row order and copy the original index verbatim.  Empty
valid frames, missing/wrong/short selected columns, and a selected NaN throw
`fincore::input_error` before output construction.  Rejecting NaN is an
intentional native table-boundary difference from Python's `np.isnan` row
filter: silently deleting a timestamp would violate DataFrame identity.  A
signed infinity is not silently dropped or clamped; it is transformed and
summarized by ordinary IEEE arithmetic and the already accepted public metric
semantics.

Scenario configuration is structural: an unknown enum value, duplicate
predefined selection, empty custom name, custom name equal to a reserved
predefined name, or duplicate custom name throws `input_error`.  A finite
custom shift and multiplier apply as `(return + return_shift) *
return_multiplier`; no implicit `vol_mult` option is added because Python's
current implementation documents it but does not execute it.  Explicitly
empty predefined/custom spans are valid after return-column validation and
produce no results.  Arbitrary finite negative/non-unit weights are irrelevant
here; this module manipulates return observations directly rather than
portfolio allocations.

Every stressed output loads a new index and new selected-column vector with
the caller's column name.  It derives its summary exactly once via existing
public scalar operations on that new frame: `cumulative_return`,
`max_drawdown`, and `annual_volatility` with `daily_annualization`;
`final_value == 1.0 + cumulative_return`.  Thus the finite Python oracle matches while the
native implementation does not duplicate accepted compounding, drawdown, or
volatility formulas.  One finite observation is valid and inherits the public
annual-volatility NaN boundary.

## Verification and exclusions

The implementation must first compile a public-header-only test while the
leaf header is absent, record that RED, then use the mandatory Anaconda Python
oracle before GREEN.  Tests must cover every finite golden, source-order
preservation, custom shift-then-multiplier ordering, selected-only schema
validation, typed empty summary, NaN rejection, signed-infinity propagation,
all configuration errors, and a standalone umbrella-only C++23 consumer.
Phase acceptance follows the established AppleClang headless, Homebrew LLVM
ASan+UBSan headless, ImPlot/demo compile-link-only, and invalid-ImGui gate
matrix with retained terminal logs.

This phase intentionally excludes Python's `generate_correlation_breakdown`,
formatted `scenario_table` strings, bootstrap confidence intervals, Monte
Carlo/GBM/LHS/antithetic path generation, VaR/CVaR simulation, GARCH/EVT,
Fama-French/style attribution, providers, reports/tearsheets, package/release
work, full Python simulation parity, and GUI runtime validation.
