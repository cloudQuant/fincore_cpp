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

### Performance (optimized)

The optimization pass targeting the native load, factor, research and export
hot paths is complete. Measured on the same Windows 11 host; Python is
single-threaded, native uses 8 file-loading threads and 3 horizon workers for
large inputs:

| Scale | Python total | C++ total | C++ speedup | Native improvement |
|---|---:|---:|---:|---:|
| 500 symbols, 3-repeat median | 7.477 s | 0.555 s | 13.5x | 3.1x |
| 5,449 assets, 3-repeat median | 60.585 s | 5.975 s | 10.1x | 4.1x |

The target of at least 10x end-to-end versus Python is met for the default
500-symbol run and the full A-share panel. The native code itself is 3-4x
faster than the first accepted benchmark (24.531 s full -> 5.975 s). Stage
detail: load 59x, factor 2.6x, research 6.8x and export 11.9x versus Python on
the full panel. The remaining full-run cost is research (3.0 s) and the
mandatory ~300 MB CSV export (2.4 s); export is I/O/formatting bound, not a
regression.

Parity is unchanged: every primary comparison stays at 1e-15, and the full
1,044-test suite passes. See the
[alpha003 README](alpha003_a_stocks/README.md) for the before/after tables and
the optimization notes.

## Native-only harnesses

| Source | Purpose | Build / use |
|---|---|---|
| `factor_pipeline_benchmark.cpp` | Iteration 7/8 compute-once pipeline measurements; JSON per case | CMake target `fincore_factor_benchmark` (excluded from `all`) |
| `factor_alignment_benchmark.cpp` | Iteration 7 factor-alignment C4-vs-current harness | Manual build; POSIX `getrusage`; not a CMake target |
| `factor_layout_comparison.cpp` | Layout/memory experiment with `fork`/`waitpid` | Manual build; POSIX only; not a CMake target |

These are native-only experiments. The first target builds on the supported
platforms; the latter two retain POSIX process/memory APIs and are not part of
the Windows portability gate.
