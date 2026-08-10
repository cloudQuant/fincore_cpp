# fincore C++ Phase 2A Scalar Metrics Implementation Plan

> **For Codex:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Extend the accepted DataFrame-native core with the next reusable scalar risk, ratio, descriptive, paired-benchmark, and timing metrics while preserving Python-oracle semantics.

**Architecture:** Keep `dataframe` as every public series representation and add a private numeric helper layer for finite filtering, moments, pairwise alignment, and fixed-size least squares. New public metric headers compose the existing DataFrame boundary and accepted return/risk functions; an umbrella header exposes only implemented native APIs, not a pandas/Python compatibility shim.

**Tech Stack:** C++23, CMake 3.25+, local cloudQuant/DataFrame, GoogleTest/CTest, standard-library numerical algorithms; Python fincore only as the behavioral oracle.

---

### Task 8: Numeric primitives and basic metric boundary

**Files:**
- Create: `include/fincore/detail/numeric.hpp`
- Create: `include/fincore/metrics/basic.hpp`
- Create: `src/metrics/basic.cpp`
- Create: `tests/unit/basic_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write the failing test**

```cpp
TEST(BasicMetrics, UsesNativeFrequencyAnnualization) {
    EXPECT_EQ(fincore::annualization_factor(fincore::frequency::daily), 252.0);
    EXPECT_EQ(fincore::annualization_factor(fincore::frequency::monthly), 12.0);
    EXPECT_EQ(fincore::annualization_factor(fincore::frequency::daily, 365.0), 365.0);
}

TEST(BasicMetrics, AdjustReturnsPreservesIndexAndColumn) {
    const auto returns = fincore::make_series({10, 20}, "returns", {0.03, -0.01});
    const auto adjusted = fincore::adjust_returns(returns, 0.01);
    EXPECT_EQ(adjusted.get_index(), (std::vector<fincore::timestamp>{10, 20}));
    EXPECT_EQ(adjusted.get_column<double>("returns"), (std::vector<double>{0.02, -0.02}));
}
```

**Step 2: Run it to verify failure**

Run: `cmake --build build --target fincore_tests && ctest --test-dir build -R BasicMetrics --output-on-failure`

Expected: compilation fails because `fincore/metrics/basic.hpp` and its public functions do not exist.

**Step 3: Implement the minimum**

Add a `frequency` enum (`daily`, `weekly`, `monthly`, `quarterly`, `yearly`), `annualization_factor(frequency, std::optional<double>)`, and `adjust_returns(const dataframe&, double, std::string_view)`. The latter validates with `require_double_column`, copies the index, and calls `make_series`; zero adjustment still returns a value-owning frame. Add private header-only helpers for finite filtering, arithmetic mean, sample/population variance, pairwise filtering, and linear percentile. Do not expose vectors or numeric helpers publicly.

**Step 4: Verify**

Run: `ctest --test-dir build -R BasicMetrics --output-on-failure && ctest --test-dir build --output-on-failure`

Expected: basic tests and all existing tests pass.

**Step 5: Commit**

Run: `git add include/fincore/detail/numeric.hpp include/fincore/metrics/basic.hpp src/metrics/basic.cpp tests/unit/basic_test.cpp CMakeLists.txt tests/CMakeLists.txt && git commit -m "feat: add numeric primitives and basic metrics"`

### Task 9: Extended scalar risk metrics

**Files:**
- Modify: `include/fincore/metrics/risk.hpp`
- Modify: `src/metrics/risk.cpp`
- Create: `tests/unit/extended_risk_test.cpp`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write failing oracle tests**

```cpp
EXPECT_NEAR(fincore::downside_risk(returns), expected_downside, 1e-12);
EXPECT_NEAR(fincore::tail_ratio(returns), expected_tail, 1e-12);
EXPECT_NEAR(fincore::tracking_error(returns, benchmark), expected_te, 1e-12);
EXPECT_TRUE(std::isnan(fincore::residual_risk(flat, benchmark)));
```

Use deterministic fixtures calculated by the Python reference. Also cover pairwise timestamp alignment, `NaN` rules, invalid percentile or annualization arguments, and signed-infinity behavior where the reference preserves it.

**Step 2: Run it to verify failure**

Run: `ctest --test-dir build -R ExtendedRisk --output-on-failure`

Expected: compilation failure for absent declarations.

**Step 3: Implement by composing Task 8 helpers**

Add `downside_risk`, `tail_ratio`, `tracking_error`, `residual_risk`, and `var_cov_var_normal`. Every benchmark operation reuses explicit alignment. Do not rewrite accepted Sharpe/Sortino/Calmar formulas; share only semantics-preserving private primitives.

**Step 4: Verify**

Run: `ctest --test-dir build -R ExtendedRisk --output-on-failure && ctest --test-dir build --output-on-failure`

Expected: focused oracle fixtures and the complete suite pass.

**Step 5: Commit**

Run: `git add include/fincore/metrics/risk.hpp src/metrics/risk.cpp tests/unit/extended_risk_test.cpp tests/CMakeLists.txt && git commit -m "feat: add extended scalar risk metrics"`

### Task 10: Extended risk-adjusted ratios

**Files:**
- Modify: `include/fincore/metrics/ratios.hpp`
- Modify: `src/metrics/ratios.cpp`
- Create: `tests/unit/extended_ratios_test.cpp`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write failing oracle tests**

```cpp
EXPECT_NEAR(fincore::excess_sharpe(returns, benchmark), expected_excess_sharpe, 1e-12);
EXPECT_NEAR(fincore::information_ratio(returns, benchmark), expected_information, 1e-12);
EXPECT_NEAR(fincore::omega_ratio(returns, 0.0), expected_omega, 1e-12);
EXPECT_NEAR(fincore::stability_of_timeseries(returns), expected_stability, 1e-12);
EXPECT_TRUE(std::isnan(fincore::capture(returns, zero_benchmark)));
```

Use deterministic fixtures calculated by the Python reference. Cover pairwise timestamp alignment, `NaN` pairs, zero denominator, too-few samples, and the reference's different finite/infinite conventions for each metric.

**Step 2: Run it to verify failure**

Run: `ctest --test-dir build -R ExtendedRatios --output-on-failure`

Expected: compilation failure for absent declarations.

**Step 3: Implement by composing accepted metrics**

Add `excess_sharpe`, `information_ratio`, `omega_ratio`, `stability_of_timeseries`, `capture`, `up_capture`, `down_capture`, and `up_down_capture`. Reuse explicit alignment and accepted `annual_return` calculations; do not copy or change established Sharpe/Sortino/Calmar formulas.

**Step 4: Verify**

Run: `ctest --test-dir build -R ExtendedRatios --output-on-failure && ctest --test-dir build --output-on-failure`

Expected: focused oracle fixtures and the complete suite pass.

**Step 5: Commit**

Run: `git add include/fincore/metrics/ratios.hpp src/metrics/ratios.cpp tests/unit/extended_ratios_test.cpp tests/CMakeLists.txt && git commit -m "feat: add extended risk-adjusted ratios"`

### Task 11: Descriptive and benchmark statistics

**Files:**
- Create: `include/fincore/metrics/stats.hpp`
- Create: `src/metrics/stats.cpp`
- Create: `tests/unit/stats_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write failing tests**

Add fixture tests for `skewness`, excess `kurtosis`, `serial_correlation`, `win_rate`, `loss_rate`, `safe_correlation`, `relative_win_rate`, `r_squared`, and `tracking_difference`. Include fewer-than-required observations, constant series, unequal index sequences, and `NaN` pairs.

**Step 2: Run it to verify failure**

Run: `ctest --test-dir build -R Stats --output-on-failure`

Expected: missing `fincore/metrics/stats.hpp` compilation error.

**Step 3: Implement sample semantics**

Implement the Python reference's finite-observation/sample-moment formulas, not a dependency-specific alternative. A paired function must call `align_on_common_index`; `safe_correlation` and `r_squared` return `NaN` for constant or insufficient samples. Preserve the original input index only for future materialized results; these APIs are scalar.

**Step 4: Verify**

Run: `ctest --test-dir build -R Stats --output-on-failure && ctest --test-dir build --output-on-failure`

Expected: all tests pass.

**Step 5: Commit**

Run: `git add include/fincore/metrics/stats.hpp src/metrics/stats.cpp tests/unit/stats_test.cpp CMakeLists.txt tests/CMakeLists.txt && git commit -m "feat: add descriptive statistics metrics"`

### Task 12: Paired alpha/beta extensions and timing regressions

**Files:**
- Modify: `include/fincore/metrics/alpha_beta.hpp`
- Modify: `src/metrics/alpha_beta.cpp`
- Create: `include/fincore/metrics/timing.hpp`
- Create: `src/metrics/timing.cpp`
- Create: `tests/unit/extended_alpha_beta_test.cpp`
- Create: `tests/unit/timing_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write failing tests**

Test a `std::pair<double, double>` `alpha_beta` convenience operation, up/down pair filtering, annual beta, and Python golden values for Treynor–Mazuy, Henriksson–Merton, market-timing return, and Cornell timing. Add singular-design, fewer-than-ten-pairs, and mismatched-index cases returning `NaN`.

**Step 2: Run it to verify failure**

Run: `ctest --test-dir build -R "ExtendedAlphaBeta|Timing" --output-on-failure`

Expected: missing declarations/target sources fail compilation.

**Step 3: Implement fixed-size regression needs**

Extend the existing aligned-pair path rather than creating positional overloads. Use a pivoted dense solve for two/three independent variables; on singular or ill-conditioned input, return `NaN`. Preserve the Python minimum-observation contracts and make annualization explicit in public signatures.

**Step 4: Verify**

Run: `ctest --test-dir build -R "ExtendedAlphaBeta|Timing" --output-on-failure && ctest --test-dir build --output-on-failure`

Expected: new and existing paired metrics pass.

**Step 5: Commit**

Run: `git add include/fincore/metrics/alpha_beta.hpp include/fincore/metrics/timing.hpp src/metrics/alpha_beta.cpp src/metrics/timing.cpp tests/unit/extended_alpha_beta_test.cpp tests/unit/timing_test.cpp CMakeLists.txt tests/CMakeLists.txt && git commit -m "feat: add paired benchmark and timing metrics"`

### Task 13: Native metric umbrella and Phase 2A acceptance

**Files:**
- Create: `include/fincore/metrics.hpp`
- Create: `tests/unit/metrics_umbrella_test.cpp`
- Create: `docs/acceptance/2026-08-10-phase-2a-scalar-metrics-acceptance.md`
- Modify: `README.md`
- Modify: `tests/CMakeLists.txt`

**Step 1: Write a failing compile/use test and acceptance checklist**

```cpp
#include <fincore/metrics.hpp>

TEST(MetricsUmbrella, ExposesOnlyImplementedDataframeNativeMetrics) {
    const auto returns = fincore::make_series({1, 2}, "returns", {0.01, 0.02});
    EXPECT_TRUE(std::isfinite(fincore::sharpe_ratio(returns)));
}
```

Record unchecked acceptance items for API coverage, full core CTest, Homebrew-LLVM ASan+UBSan, and the still-independent optional ImPlot positive-build gate.

**Step 2: Run it to verify failure**

Run: `ctest --test-dir build -R MetricsUmbrella --output-on-failure`

Expected: missing umbrella header compilation failure.

**Step 3: Implement and record actual evidence**

The umbrella includes only accepted public headers; it must not invent a dynamic Python `Empyrical` object or include ImPlot. Update README with the new native surface and record actual test/sanitizer results. Leave calendar, positions, transactions, Bayesian, attribution, and full facade work marked as deferred. If no valid local Dear ImGui checkout has been supplied, retain the ImPlot positive compilation gate as open.

**Step 4: Run final verification**

Run: `cmake -S . -B build -G Ninja -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF && cmake --build build --parallel 4 && ctest --test-dir build --output-on-failure`

Run: `cmake -S . -B build-sanitize -G Ninja -DFINCORE_BUILD_TESTING=ON -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ && cmake --build build-sanitize --parallel 4 && ctest --test-dir build-sanitize --output-on-failure`

Expected: all registered tests pass in both builds with no sanitizer diagnostic. Record the exact count rather than assuming the Phase 1 total.

**Step 5: Commit**

Run: `git add include/fincore/metrics.hpp tests/unit/metrics_umbrella_test.cpp docs/acceptance/2026-08-10-phase-2a-scalar-metrics-acceptance.md README.md tests/CMakeLists.txt && git commit -m "docs: accept phase 2A scalar metrics"`

## Deferred after this plan

Calendar/yearly APIs, extended drawdown/consecutive dates, positions, transactions, round trips, Bayesian/bootstrap/performance-attribution modules, and the comprehensive native Empyrical facade remain separate, explicitly planned work. Do not represent this plan's completion as full Phase 2 or full project completion.
