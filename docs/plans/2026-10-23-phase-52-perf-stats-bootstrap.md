# Phase 52 Native Bootstrap Performance Stats Plan

**Goal:** Add the bootstrap surface explicitly mapped from Python:
`calc_distribution_stats` (exact), and a fixed-resample
`perf_stats_bootstrap` covering the thirteen unary perf metrics. The generic
`calc_bootstrap(func, ...)` maps to the accepted
`simulation::bootstrap_distribution` leaf (callable-statistic kernel) and is
not duplicated.

**Baseline:** `dev` after Phase 51 (oracle pin `4b8f31b`). Python source:
`fincore/metrics/perf_stats.py` lines 147–306.

**Files:** `include/fincore/metrics/perf_stats.hpp`,
`src/metrics/perf_stats.cpp`, `tests/unit/perf_stats_test.cpp`.

## Native API

```cpp
struct distribution_stats {
    double mean, median, std, p5, p25, p75, p95, iqr;
};
distribution_stats calc_distribution_stats(std::span<const double>);

struct bootstrap_metric_stats {
    std::string name;
    std::vector<double> samples;
    distribution_stats summary;
};
std::vector<bootstrap_metric_stats> perf_stats_bootstrap(
    const dataframe&, std::size_t n_samples = 1000, std::uint64_t seed = 0,
    column = returns_column);
```

## Contracts

- Distribution stats: mean, median (`np.percentile` 50), population standard
  deviation (`np.std` default ddof=0), linear percentiles 5/25/75/95, and
  `IQR = p75 - p25`. Empty input returns NaN fields.
- Bootstrap: resamples indices with replacement (`std::mt19937_64`), length
  preserved, metrics evaluated by the native leaves for the thirteen Python
  registry entries in order. `summary` is `calc_distribution_stats(samples)`.
- **RNG policy:** same-seed NumPy/C++ sample-stream equality is explicitly
  not claimed (delivery contract §). Determinism is native: identical
  `(seed, n_samples)` → identical samples; different seed → different
  stream. Fixed-resample kernel parity is tested by metric composition and
  summary recomputation.

## Tests

Distribution goldens (hex-repr fixtures), degenerate single/empty samples,
bootstrap determinism/order/seed sensitivity, single-sample and empty input,
selector/NUL validation.

## Gates

Clean detached oracle at `4b8f31b` (distribution goldens and metric names);
full CTest; LLVM 22 ASan+UBSan; acceptance record and README ledger.
