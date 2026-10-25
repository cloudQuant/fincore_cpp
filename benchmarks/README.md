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

### Performance (equal CPU, single-threaded)

The native parallel paths were removed so both implementations use the same
CPU model: one worker, no native thread pool. The C++ instrumented run reports
CPU time equal to its pipeline wall time (6.804 s vs 6.804 s in one full run),
and the Python process uses ~94% CPU time versus wall time (62.4 s vs 66.5 s).
The Python `fincore` package contains no computational multiprocessing or
thread-pool usage; only an `RLock` in the optional session API is present.

| Scale | Python total | C++ total | C++ speedup | Native improvement |
|---|---:|---:|---:|---:|
| 500 symbols, 3-repeat median | 7.782 s | 0.574 s | 13.6x | 3.0x |
| 5,449 assets, 3-repeat median | 66.485 s | 7.860 s | 8.5x | 3.1x |

Stage detail full panel: load 22.2x, factor 3.7x, research 6.3x and export
12.0x versus Python. The native code itself is about 3x faster than the first
accepted benchmark (24.531 s full -> 7.860 s). The earlier parallel numbers
(10.1x full / 13.5x at 500) are intentionally not used as the headline because
they did not hold CPU usage equal.

### 50x feasibility (equal CPU, complete output)

A 50x result would require 0.156 s at 500 symbols and 1.329 s full. Current
load+factor alone is 0.092 s and 0.961 s, and the mandatory result payload is
32.5 MB and 347 MB. Even with research and serialization at zero, the 500
budget is nearly consumed; full would need 347 MB of CSV generated inside
~0.37 s after load+factor. Single-thread complete-output 50x is therefore not
physically achievable. The realistic single-thread ceiling after further
rank-order reuse and compact row export is roughly 15-20x at 500 and 11-13x
full. A 50x headline would require a reduced-output summary/hash mode or
parallelism and would be a different benchmark contract.

Parity is unchanged in the equal-CPU run: every primary comparison stays at
1e-15 and the full 1,044-test suite passes. See the
[alpha003 README](alpha003_a_stocks/README.md) for the stage tables and the
50x analysis.

## Native-only harnesses

| Source | Purpose | Build / use |
|---|---|---|
| `factor_pipeline_benchmark.cpp` | Iteration 7/8 compute-once pipeline measurements; JSON per case | CMake target `fincore_factor_benchmark` (excluded from `all`) |
| `factor_alignment_benchmark.cpp` | Iteration 7 factor-alignment C4-vs-current harness | Manual build; POSIX `getrusage`; not a CMake target |
| `factor_layout_comparison.cpp` | Layout/memory experiment with `fork`/`waitpid` | Manual build; POSIX only; not a CMake target |

These are native-only experiments. The first target builds on the supported
platforms; the latter two retain POSIX process/memory APIs and are not part of
the Windows portability gate.
