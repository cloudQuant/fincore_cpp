# Benchmarks

This directory contains native performance work and one end-to-end,
cross-language parity benchmark. Large datasets, generated CSVs and timing
artifacts stay local and are gitignored.

## alpha003 A-share parity and performance benchmark

[`alpha003_a_stocks/`](alpha003_a_stocks/README.md) compares the Python
`fincore` package with native `fincore_cpp` on the same 5,471-symbol A-share
daily panel for Alpha101 alpha003:

```
alpha003 = -1 * correlation(rank(open), rank(volume), 10)
```

Both runners load the raw per-symbol CSVs independently. Python computes the
factor with pandas and evaluates it through `fincore.factor_analysis.research`;
C++ computes it in the benchmark and evaluates it through native
`evaluate_factor_research`. The comparison covers factor panels, raw and
processed signals, labels, rank IC, IC summary, quantile returns and standard
errors, spreads, turnover and stability.

### Current status

- **Numeric parity:** all primary checks pass. The 500-symbol run (498
  evaluated assets, 114,509 factor cells) has a maximum primary absolute
  difference of **1.44e-15**; the full run (5,449 assets, 1,255,737 cells) has
  **5.44e-15**. The tolerance gate is `1e-6`.
- **Declared differences:** C++ exports rank IC (compared with Python
  `rank_ic`; Python's Pearson `ic` has no native column), Python keeps NaN
  label rows that C++ omits, and the published stability summary differs at
  ~1.8e-04 (500) / 1.3e-04 (full) while the per-date stability kernel matches
  at ~1e-15. Coverage tables have different shapes and are reported, not
  gated.
- **Data:** copied from `D:\source_code\backtrader\tests\datas\a_stocks`; the
  94 MB payload and generated `results*/` directories are gitignored. See
  [data provenance](alpha003_a_stocks/data/README.md).
- **Reproduce:**
  `python benchmarks/alpha003_a_stocks/run_benchmark.py --max-symbols 500 --repeats 3`
  or `--max-symbols 0 --results-dir benchmarks/alpha003_a_stocks/results-full`
  for the full universe.

### Historical performance (single-worker intent; CPU budget needs revalidation)

The native parallel paths were removed. However, the C++ runner records
`std::clock()` as CPU time; MSVC returns wall time from this function, so the
reported 6.804 s / 6.804 s is not CPU-utilization evidence. The recorded
Python CPU measurement uses `time.process_time()`. Runtime affinity, library
thread limits, alternating run order and matching timer boundaries must be
established before claiming an equal-CPU acceptance result. See
[Iteration 9](../docs/iterations/iteration-9-performance-30x-100x/README.md).

| Scale | Python total | C++ total | C++ speedup | Native improvement |
|---|---:|---:|---:|---:|
| 500 symbols, 3-repeat median | 7.782 s | 0.574 s | 13.6x | 3.0x |
| 5,449 assets, 3-repeat median | 66.485 s | 7.860 s | 8.5x | 3.1x |

Stage detail full panel: load 22.2x, factor 3.7x, research 6.3x and export
12.0x versus Python. The native code itself is about 3x faster than the first
accepted benchmark (24.531 s full -> 7.860 s). The earlier parallel numbers
(10.1x full / 13.5x at 500) are intentionally not used as the headline because
they did not hold CPU usage equal.

### Next target: 30x minimum, aiming for 100x

The earlier 11-13x full-panel ceiling and claim that 50x is physically
impossible were not established by the measurements. Current stage times and
output sizes show substantial remaining work, not an immutable lower bound.
[Iteration 9](../docs/iterations/iteration-9-performance-30x-100x/README.md)
sets a complete-output, single-core 30x minimum and 100x stretch target for
both 500-file and full-panel scenarios, with workload/timer repairs first.
Reduced-output and multicore experiments are separate scores, not substitutes.

The historical primary comparisons were within about 1e-15 and the recorded
native suite passed 1,044 tests; these are not fresh runs or Iteration 9
acceptance. See the [alpha003 README](alpha003_a_stocks/README.md) for the
historical stage tables and the measurement limitations.

## Native-only harnesses

| Source | Purpose | Build / use |
|---|---|---|
| `factor_pipeline_benchmark.cpp` | Iteration 7/8 compute-once pipeline measurements; JSON per case | CMake target `fincore_factor_benchmark` (excluded from `all`) |
| `factor_alignment_benchmark.cpp` | Iteration 7 factor-alignment C4-vs-current harness | Manual build; POSIX `getrusage`; not a CMake target |
| `factor_layout_comparison.cpp` | Layout/memory experiment with `fork`/`waitpid` | Manual build; POSIX only; not a CMake target |

These are native-only experiments. The first target builds on the supported
platforms; the latter two retain POSIX process/memory APIs and are not part of
the Windows portability gate.
