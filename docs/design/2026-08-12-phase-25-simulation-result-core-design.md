# Phase 25 DataFrame-native simulation-result core design

## Scope decision

Python `fincore.simulation.base` combines a mutable `SimResult` container,
terminal-value statistics, VaR/CVaR helpers, and generic parameter utilities.
The next native slice is deliberately narrower: an immutable, value-owning
result object for an already-generated DataFrame of paths. It makes the
accepted GBM, GBM-from-returns, and antithetic generators consumable without
prematurely introducing the stateful Python `MonteCarlo` orchestrator.

Three approaches were considered:

1. **Selected — terminal result value.** A `simulation_result` owns an input
   DataFrame and exposes named terminal statistics plus terminal VaR/CVaR.
   It is composable with all existing native path generators and has clear
   ownership/lifetime semantics.
2. **Full `MonteCarlo` class.** This would also own historical returns,
   estimation defaults, generation, stress dispatch, and seeding. It is too
   broad: it duplicates accepted leaves and pulls Python's known
   orchestration/seed inconsistencies into a first result slice.
3. **Loose free functions only.** This avoids a class but forces every caller
   to revalidate paths and provides no stable, value-owning handoff to later
   Monte Carlo orchestration.

Phase 25 selects option 1. It is a native C++ API, not a one-for-one Python
object emulation.

## Public API and native orientation

Create `<fincore/simulation/result.hpp>`:

```cpp
struct simulation_terminal_percentiles {
    double percentile_1;
    double percentile_5;
    double percentile_25;
    double percentile_50;
    double percentile_75;
    double percentile_95;
    double percentile_99;
};

struct simulation_terminal_statistics {
    double mean;
    double sample_standard_deviation;
    double median;
    simulation_terminal_percentiles percentiles;
};

class simulation_result {
public:
    [[nodiscard]] const dataframe& paths() const noexcept;
    [[nodiscard]] const simulation_terminal_statistics& statistics() const noexcept;
    [[nodiscard]] std::size_t path_count() const noexcept;
    [[nodiscard]] std::size_t observation_count() const noexcept;
    [[nodiscard]] double terminal_value_at_risk(double alpha = 0.05) const;
    [[nodiscard]] double terminal_conditional_value_at_risk(
        double alpha = 0.05) const;
};

simulation_result analyze_simulation_paths(const dataframe& paths);
```

The native DataFrame orientation is already the inverse of Python's ndarray:
one named genuine-`double` column per path, with physical DataFrame index rows
as observations. Terminal values are therefore `values.back()` for each path
column in DataFrame column order. The input index is copied unchanged (including
duplicates and nonmonotonic order); it is never sorted or normalized. Path
count is the number of path columns, and observation count is index length.
Those names deliberately avoid Python's row/column `n_paths`/`horizon`
ambiguity.

`simulation_result` has no public mutable path member. The factory copies its
input DataFrame into the result so statistics and terminal-risk methods remain
consistent after the source mutates or is destroyed. Its private constructor
enforces the paired-path/statistics invariant.

## Statistical and error semantics

For finite terminal values, Phase 25 matches Python's
`compute_statistics`/`SimResult` arithmetic:

- mean; sample standard deviation (`ddof=1`); median; and linear NumPy-style
  percentiles at 1, 5, 25, 50, 75, 95, and 99 percent;
- terminal VaR is the same linear percentile at a strict `0 < alpha < 1`;
- terminal CVaR is the arithmetic mean of terminal values less than or equal
  to that VaR.

The existing private `detail::linear_percentile` is the shared finite
percentile primitive. No duplicate sort/interpolation formula belongs in the
tests. A single path is accepted: it retains Python's `NaN` sample standard
deviation while mean/median/percentiles/VaR/CVaR are that value. Invalid
alpha, including zero, one, NaN, and infinities, returns `NaN`, matching
Python's `SimResult.var` and `SimResult.cvar` rather than throwing.

Unlike NumPy's warning-and-NaN behavior for terminal NaN/infinity, native
analysis rejects a nonfinite terminal input with `fincore::input_error` before
sorting. This makes `std::sort` well-defined and prevents a silently
non-orderable terminal distribution. Nonterminal observations retain ordinary
DataFrame values because they do not participate in this terminal-only slice.
Empty index, no path columns, a non-double/mixed column, a malformed column,
and unrepresentable/overflowing materialization or finite-statistic
intermediates also raise `input_error`. No random generation, filtering of
finite terminal samples, clamping, or index rewrite is introduced.

## Test and acceptance strategy

The mandatory Anaconda oracle will use a four-path, three-observation
path-major ndarray with terminal values `[80, 110, 130, 190]`; its documented
statistics are mean `127.5`, sample standard deviation
`46.45786621588785`, median `120`, percentiles
`[80.9, 84.5, 102.5, 120, 145, 181, 188.2]`, and alpha-dependent VaR/CVaR.
Native tests transpose that fixture into timestamp rows/named path columns and
assert exact raw index/schema/path ownership plus public-statistical parity.
They also cover singleton, unordered/duplicate index, valid nonterminal
NaN/infinity retention, nonfinite terminal rejection, empty/mixed/malformed
input, invalid alpha NaN results, and source mutation/destruction.

The leaf header will receive a genuine public-header RED test before its
declaration exists. A later simulation façade consumer includes only
`<fincore/simulation.hpp>`. Acceptance retains configured/fresh Apple/LLVM
ASan+UBSan/ImPlot compile-link/no-GUI/invalid-ImGui/Fetched-GoogleTest evidence
and explicitly excludes the full `MonteCarlo` class, return calibration or
GBM changes, stress dispatch, random control, reports, providers, and GUI
runtime work.
