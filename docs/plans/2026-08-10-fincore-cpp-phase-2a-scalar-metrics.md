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
- Modify: `include/fincore/detail/numeric.hpp`
- Create: `tests/unit/extended_risk_test.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces and Python-oracle fixture:**

```cpp
double downside_risk(const dataframe& returns, double required_return = 0.0,
                     double annualization = daily_annualization,
                     std::string_view column = returns_column);
double tail_ratio(const dataframe& returns,
                  std::string_view column = returns_column);
double tracking_error(const dataframe& returns, const dataframe& benchmark,
                      double annualization = daily_annualization,
                      std::string_view returns_column_name = returns_column,
                      std::string_view benchmark_column_name = benchmark_column);
double residual_risk(const dataframe& returns, const dataframe& benchmark,
                     double risk_free = 0.0,
                     double annualization = daily_annualization,
                     std::string_view returns_column_name = returns_column,
                     std::string_view benchmark_column_name = benchmark_column);
double var_cov_var_normal(double portfolio_value, double confidence,
                          double mean = 0.0, double standard_deviation = 1.0);
```

For returns `{.01, -.02, .03, -.01}` and benchmark
`{.005, -.01, .02, -.005}` at index `{1,2,3,4}`, Python yields: downside
`0.1774823934929885`, tail ratio `1.4594594594594592`, tracking error
`0.14491376746189438`, and residual risk `0.03741657386773943` at daily
annualization. For the same return values at `{1,2,3,4}` and benchmark values
at `{2,3,4,5}`, tracking error is `0.6199193495931548` and residual risk is
`0.2749545416973504`, proving timestamp alignment rather than positional
pairing. `downside_risk({.01, NaN, -.02})` is `0.2244994432064365` and
`tail_ratio({.01, NaN, -.02, .03})` is `1.6470588235294115`; `tail_ratio` is
`NaN` when the fifth-percentile magnitude is zero. `residual_risk` with a
flat benchmark is `NaN`. `var_cov_var_normal(100000, .99, .001, .02)` is
`4552.695748081678` (within 1e-9). Implement the normal inverse CDF as a
private, deterministic rational approximation in `detail/numeric.hpp`; do not
add a statistics dependency. Its reference domain behavior is: confidence
`0` returns negative infinity, confidence `1` returns positive infinity,
confidence outside `[0, 1]` or non-finite returns `NaN`, and a non-positive or
non-finite standard deviation returns `NaN`.

**Step 1: Write failing oracle tests**

```cpp
EXPECT_NEAR(fincore::downside_risk(returns), expected_downside, 1e-12);
EXPECT_NEAR(fincore::tail_ratio(returns), expected_tail, 1e-12);
EXPECT_NEAR(fincore::tracking_error(returns, benchmark), expected_te, 1e-12);
EXPECT_TRUE(std::isnan(fincore::residual_risk(flat, benchmark)));
```

Use the exact fixture above. Also cover pairwise timestamp alignment, `NaN`
rules, empty/short inputs, invalid selected columns, and the normal-VaR
confidence/standard-deviation domain behavior observed in the Python oracle.

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

**Public interfaces and native boundary:**

```cpp
double excess_sharpe(const dataframe& returns, const dataframe& benchmark,
                     std::string_view returns_column_name = returns_column,
                     std::string_view benchmark_column_name = benchmark_column);
double information_ratio(const dataframe& returns, const dataframe& benchmark,
                         double annualization = daily_annualization,
                         std::string_view returns_column_name = returns_column,
                         std::string_view benchmark_column_name = benchmark_column);
double omega_ratio(const dataframe& returns, double risk_free = 0.0,
                   double required_return = 0.0,
                   double annualization = daily_annualization,
                   std::string_view column = returns_column);
double stability_of_timeseries(const dataframe& returns,
                               std::string_view column = returns_column);
double capture(const dataframe& returns, const dataframe& benchmark,
               double annualization = daily_annualization,
               std::string_view returns_column_name = returns_column,
               std::string_view benchmark_column_name = benchmark_column);
double up_capture(const dataframe& returns, const dataframe& benchmark,
                  double annualization = daily_annualization,
                  std::string_view returns_column_name = returns_column,
                  std::string_view benchmark_column_name = benchmark_column);
double down_capture(const dataframe& returns, const dataframe& benchmark,
                    double annualization = daily_annualization,
                    std::string_view returns_column_name = returns_column,
                    std::string_view benchmark_column_name = benchmark_column);
double up_down_capture(const dataframe& returns, const dataframe& benchmark,
                       double annualization = daily_annualization,
                       std::string_view returns_column_name = returns_column,
                       std::string_view benchmark_column_name = benchmark_column);
```

Every paired operation calls `align_on_common_index` and retains only pairs
whose return and benchmark are both finite; it neither pairs positions nor
models pandas `Series` outer-index arithmetic. `excess_sharpe` needs two
finite active samples and returns `NaN` when its sample tracking error is
smaller than `1e-15` or its quotient is non-finite. `information_ratio` also
needs two finite active samples, a finite positive annualization, and returns
`NaN` for zero/non-finite tracking error. `omega_ratio` retains finite return
samples, needs at least two, requires finite positive annualization and finite
threshold inputs, deannualizes the required return as the Python reference
does, and returns `NaN` for an invalid `required_return <= -1` (unless the
annualization is one) or a zero loss denominator. `stability_of_timeseries`
uses at least two finite observations and the squared correlation of a
least-squares line through cumulative `log1p` returns; any non-finite log
input or zero fitted-response variance returns `NaN`. Capture functions use
the finite aligned pairs to materialize inputs for accepted `annual_return`;
they return `NaN` for no applicable pairs, zero/non-finite benchmark annual
return, or an undefined quotient. Up/down variants select strictly positive/
negative benchmark returns after this filtering, and `up_down_capture` also
rejects a zero down-capture result.

For the finite aligned daily fixture returns `{.01, -.02, .03, -.01}` and
benchmark `{.005, -.01, .02, -.005}` at index `{1,2,3,4}`, the Python oracle
gives excess Sharpe `-4.750735894131342e-17`, information ratio
`-7.541559432251765e-16`, omega `1.3333333333333335`, stability
`0.08154934149362057`, capture `0.9361616468408038`, up capture
`6.637108874074204`, down capture `1.150300078888413`, and up/down capture
`5.769893435535484`. With the same values but benchmark index `{2,3,4,5}`,
the native common-index pairs produce excess Sharpe `-0.12803687993289598`,
information ratio `-2.0325224576824743`, capture `-0.023129239097150745`, up
capture `-0.045007856456753544`, down capture `-1864.997304577222`, and
up/down capture `2.4132933782956014e-05`. A two-finite-pair fixture
`returns={.01, NaN, -.02, .03}`, `benchmark={.005, .01, NaN, -.01}` validates
the paired missing-data rule with excess Sharpe `0.9091372900969896` and
information ratio `14.432107063270918` after common-index finite filtering.

**Step 1: Write failing oracle tests**

```cpp
EXPECT_NEAR(fincore::excess_sharpe(returns, benchmark), expected_excess_sharpe, 1e-12);
EXPECT_NEAR(fincore::information_ratio(returns, benchmark), expected_information, 1e-12);
EXPECT_NEAR(fincore::omega_ratio(returns, 0.0), expected_omega, 1e-12);
EXPECT_NEAR(fincore::stability_of_timeseries(returns), expected_stability, 1e-12);
EXPECT_TRUE(std::isnan(fincore::capture(returns, zero_benchmark)));
```

Use the exact fixtures above. Cover structural selected-column validation,
common-timestamp rather than positional pairing, finite-pair `NaN`/infinity
filtering, zero denominators, no up/down periods, fewer-than-required samples,
omega's required-return/annualization domain, and `stability` zero/log-domain
behavior.

**Step 2: Run it to verify failure**

Run: `ctest --test-dir build -R ExtendedRatios --output-on-failure`

Expected: compilation failure for absent declarations.

**Step 3: Implement by composing accepted metrics**

Add `excess_sharpe`, `information_ratio`, `omega_ratio`,
`stability_of_timeseries`, `capture`, `up_capture`, `down_capture`, and
`up_down_capture`. Reuse explicit alignment, private finite-pair helpers, and
accepted `annual_return` calculations; do not copy or change established
Sharpe/Sortino/Calmar formulas.

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

**Public interfaces and numerical contract:**

```cpp
double skewness(const dataframe& returns,
                std::string_view column = returns_column);
double kurtosis(const dataframe& returns,
                std::string_view column = returns_column);
double serial_correlation(const dataframe& returns, std::ptrdiff_t lag = 1,
                          std::string_view column = returns_column);
double win_rate(const dataframe& returns,
                std::string_view column = returns_column);
double loss_rate(const dataframe& returns,
                 std::string_view column = returns_column);
double safe_correlation(const dataframe& returns, const dataframe& benchmark,
                        std::string_view returns_column_name = returns_column,
                        std::string_view benchmark_column_name = benchmark_column);
double relative_win_rate(const dataframe& returns, const dataframe& benchmark,
                         std::string_view returns_column_name = returns_column,
                         std::string_view benchmark_column_name = benchmark_column);
double capm_r_squared(const dataframe& returns, const dataframe& benchmark,
                      std::string_view returns_column_name = returns_column,
                      std::string_view benchmark_column_name = benchmark_column);
double tracking_difference(const dataframe& returns, const dataframe& benchmark,
                           std::string_view returns_column_name = returns_column,
                           std::string_view benchmark_column_name = benchmark_column);
```

The public name is `capm_r_squared`, matching the actual Python public API;
do not invent a generic `r_squared` alias. All functions validate their named
double columns through the DataFrame boundary. Scalar one-series metrics retain
only finite values. Thus `skewness` requires three finite values and
`kurtosis` four; each uses the SciPy-default biased standardized central
moment (with Fisher/excess kurtosis), and returns `NaN` for zero/non-finite
variance. `serial_correlation` accepts only `lag >= 1`, removes non-finite
observations before forming the lagged sequences, needs at least two resulting
pairs, and delegates its Pearson calculation to the same private primitive.
`win_rate`/`loss_rate` are the fractions of finite observations strictly above/
below zero and return `NaN` for no finite observations.

Each paired operation must use `align_on_common_index`, retain left timestamp
order, and drop a pair if either component is non-finite. `safe_correlation`
requires at least two pairs and rejects either population standard deviation
below `1e-15`. `relative_win_rate` requires at least one pair and counts only
strict `returns > benchmark` outcomes. `capm_r_squared` is the squared Pearson
correlation (equivalent to the Python beta/sigma CAPM formula), clamps a finite
result to `[0, 1]`, and returns `NaN` for constant/insufficient pairs.
`tracking_difference` materializes finite aligned pairs and composes accepted
`cumulative_return`; it returns the strategy cumulative return minus the
benchmark cumulative return, or `NaN` for no usable pairs/non-finite result.

For daily fixture returns `{.01, -.02, .03, -.01, .02}` and benchmark
`{.005, -.01, .02, -.005, .015}` at index `{1,2,3,4,5}`, Python gives
skewness `-0.15798755143759596`, excess kurtosis `-1.4908058409951324`, serial
correlation (lag one) `-0.8525371202334078`, win/loss rates `.6`/`.4`, safe
correlation `.9930437840630139`, relative win rate `.6`, CAPM R-squared
`.9861359570661897`, and tracking difference `.004563664875000262`. With the
same value vectors but benchmark index `{2,3,4,5,6}`, native common-index
pairing yields safe correlation `-.7938841860374447`, relative win rate `.5`,
CAPM R-squared `.6302521008403359`, and tracking difference
`.009517365000000222`. A finite-pair fixture returns `{.01, NaN, -.02, .03}`
and benchmark `{.005, .01, NaN, -.01}` yields correlation `-1`, relative win
rate `1`, CAPM R-squared `1`, and tracking difference `.04535000000000011`.

**Step 1: Write failing tests**

Add exact fixture tests for `skewness`, excess `kurtosis`,
`serial_correlation`, `win_rate`, `loss_rate`, `safe_correlation`,
`relative_win_rate`, `capm_r_squared`, and `tracking_difference`. Include
fewer-than-required observations, constant series, invalid lag, zero-only
rates, unequal index sequences, `NaN`/infinity filtering, no common pairs, and
named-column structural errors.

**Step 2: Run it to verify failure**

Run: `ctest --test-dir build -R Stats --output-on-failure`

Expected: missing `fincore/metrics/stats.hpp` compilation error.

**Step 3: Implement sample semantics**

Implement the Python reference's finite-observation formulas, not a
dependency-specific alternative. Reproduce SciPy's default biased skew and
Fisher/excess kurtosis directly from central moments. A paired function must
call `align_on_common_index`; `safe_correlation` and `capm_r_squared` return
`NaN` for constant or insufficient samples. Preserve the original input index
only for temporary composition; these APIs are scalar.

**Step 4: Verify**

Run: `ctest --test-dir build -R Stats --output-on-failure && ctest --test-dir build --output-on-failure`

Expected: all tests pass.

**Step 5: Commit**

Run: `git add include/fincore/metrics/stats.hpp src/metrics/stats.cpp tests/unit/stats_test.cpp CMakeLists.txt tests/CMakeLists.txt && git commit -m "feat: add descriptive statistics metrics"`

### Task 12: Paired alpha/beta extensions

**Files:**
- Modify: `include/fincore/metrics/alpha_beta.hpp`
- Modify: `src/metrics/alpha_beta.cpp`
- Create: `tests/unit/extended_alpha_beta_test.cpp`
- Modify: `tests/CMakeLists.txt`

**Public interfaces and boundary:**

```cpp
std::pair<double, double> alpha_beta(
    const dataframe& returns, const dataframe& benchmark,
    double risk_free = 0.0, double annualization = daily_annualization,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);
std::pair<double, double> up_alpha_beta(
    const dataframe& returns, const dataframe& benchmark,
    double risk_free = 0.0, double annualization = daily_annualization,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);
std::pair<double, double> down_alpha_beta(
    const dataframe& returns, const dataframe& benchmark,
    double risk_free = 0.0, double annualization = daily_annualization,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);
```

All three return `{alpha, beta}`. They validate named double columns, align on
common timestamps in left order, discard any non-finite pair, and return
`{NaN, NaN}` if fewer than two selected pairs remain or risk-free/annualization
is non-finite or non-positive. The convenience operation materializes the
finite aligned data and delegates to the accepted public `alpha`/`beta`
operations, so it cannot create a positional alternative. Up selection uses
raw benchmark `> 0`; down selection uses raw benchmark `<= 0`, matching the
Python conditional functions before risk-free adjustment.

For factor values `[-.04, -.03, -.02, -.01, -.005, .005, .01, .02, .03,
.04, .05, .06]` and strategy values `0.0007 + 1.1 * factor + .35 * factor^2`
at index `{1..12}`, with annualization one and zero risk-free, Python gives
overall `{.00098825261780111084, 1.1072133507853401}`, up-market
`{.00047044263456097468, 1.1223286118980171}`, and down-market
`{.00058368902439021042, 1.0843780487804882}`. With strategy index `{1..12}`
and the same factor vector at `{2..13}`, common-index pairing gives overall
`{.01094515223921344, 1.1199165073730202}`, up-market
`{.0086816438356165371, 1.2019041095890413}`, and down-market
`{.0089735137195121606, 1.0177149390243903}`. The zero-risk-free fixture with
`risk_free=.001` gives overall `{.001095465968586451,
1.1072133507853401}` at annualization one.

`annual_beta` is not included: the Python function groups a `DatetimeIndex` by
calendar year, while this Phase 2A DataFrame boundary intentionally provides
only opaque integer timestamps. It remains explicitly deferred with the
calendar/yearly APIs rather than being reinterpreted as an annualized beta.

**Step 1: Write failing tests**

Test each `std::pair<double, double>` operation against the exact fixtures
above. Cover common-index rather than positional pairing, NaN/infinity
filtering, insufficient selected up/down samples, constant benchmark,
non-finite/invalid annualization, and named-column structural errors.

**Step 2: Run it to verify failure**

Run: `ctest --test-dir build -R ExtendedAlphaBeta --output-on-failure`

Expected: missing declarations/target sources fail compilation.

**Step 3: Implement by composition**

Extend the existing aligned-pair path rather than creating positional
overloads. Materialize the finite selection before invoking accepted
`alpha`/`beta`; do not change the behavior of existing standalone functions.

**Step 4: Verify**

Run: `ctest --test-dir build -R ExtendedAlphaBeta --output-on-failure && ctest --test-dir build --output-on-failure`

Expected: new and existing paired metrics pass.

**Step 5: Commit**

Run: `git add include/fincore/metrics/alpha_beta.hpp src/metrics/alpha_beta.cpp tests/unit/extended_alpha_beta_test.cpp tests/CMakeLists.txt && git commit -m "feat: add paired alpha beta metrics"`

### Task 13: Timing regression metrics

**Files:**
- Create: `include/fincore/metrics/timing.hpp`
- Create: `src/metrics/timing.cpp`
- Create: `tests/unit/timing_test.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Public interfaces and regression contract:**

```cpp
double treynor_mazuy_timing(const dataframe& returns, const dataframe& benchmark,
                            double risk_free = 0.0,
                            std::string_view returns_column_name = returns_column,
                            std::string_view benchmark_column_name = benchmark_column);
double henriksson_merton_timing(const dataframe& returns, const dataframe& benchmark,
                                double risk_free = 0.0,
                                std::string_view returns_column_name = returns_column,
                                std::string_view benchmark_column_name = benchmark_column);
double market_timing_return(const dataframe& returns, const dataframe& benchmark,
                            double risk_free = 0.0,
                            std::string_view returns_column_name = returns_column,
                            std::string_view benchmark_column_name = benchmark_column);
double cornell_timing(const dataframe& returns, const dataframe& benchmark,
                      double risk_free = 0.0,
                      std::string_view returns_column_name = returns_column,
                      std::string_view benchmark_column_name = benchmark_column);
```

Each operation aligns common timestamps, drops non-finite pairs, requires a
finite risk-free value and at least ten pairs, and returns `NaN` when the
design is singular or ill-conditioned. Implement a private partial-pivoted
fixed 3×3 least-squares solve; no numerical dependency is added. Treynor–Mazuy
regresses excess strategy return on `{1, excess market, excess market^2}` and
returns the quadratic coefficient. Henriksson–Merton uses `{1, excess market,
indicator(excess market < 0)}` and returns the indicator coefficient. Cornell
uses `{1, max(0, excess market), min(0, excess market)}` and returns beta-up
minus beta-down. `market_timing_return` reuses the same finite sample and
Treynor–Mazuy gamma multiplied by the mean squared excess market return.

For the 12-point factor/strategy fixture in Task 12, Python yields Treynor–
Mazuy `.34999999999999637`, Henriksson–Merton `.0004285816588285324`, market
timing return `.0003543749999999964`, and Cornell `.039162486406123742` at
zero risk-free. If the common-index pair with factor `.005` is non-finite and
removed, the finite 11-pair Python goldens are `.35000000000000175`,
`.00033602950713341572`, `.00038579545454545648`, and
`.040857423359513145`, respectively. A mismatched input leaving fewer than
ten common pairs and a constant/rank-deficient factor design must return
`NaN` for all four metrics.

**Step 1: Write failing tests**

Add exact main and finite-filter oracle fixtures plus insufficient/singular,
non-finite risk-free, common-index mismatch, and named-column structural
error tests.

**Step 2: Run it to verify failure**

Run: `ctest --test-dir build -R Timing --output-on-failure`

Expected: missing timing header/API/target sources fail compilation.

**Step 3: Implement the fixed-size regression path**

Keep the solver and aligned finite-pair representation private to the timing
module. Validate numerical rank before exposing any coefficient; do not use
the existing beta path as a positional substitute.

**Step 4: Verify**

Run: `ctest --test-dir build -R Timing --output-on-failure && ctest --test-dir build --output-on-failure`

Expected: timing golden fixtures and all existing tests pass.

**Step 5: Commit**

Run: `git add include/fincore/metrics/timing.hpp src/metrics/timing.cpp tests/unit/timing_test.cpp CMakeLists.txt tests/CMakeLists.txt && git commit -m "feat: add timing regression metrics"`

### Task 14: Native metric umbrella and Phase 2A acceptance

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

Calendar/yearly APIs (including `annual_alpha` and `annual_beta`), extended
drawdown/consecutive dates, positions, transactions, round trips,
Bayesian/bootstrap/performance-attribution modules, and the comprehensive
native Empyrical facade remain separate, explicitly planned work. Do not
represent this plan's completion as full Phase 2 or full project completion.
