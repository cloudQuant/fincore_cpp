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

### Baseline performance

The baseline measured on the same Windows 11 host (MSVC 19.42, 32 logical
CPUs, no parallelism in either runner):

| Scale | Python total | C++ total | C++ speedup | Wall speedup |
|---|---:|---:|---:|---:|
| 500 symbols, 3-repeat median | 7.083 s | 1.740 s | 4.1x | 4.3x |
| 5,449 assets, 1 run | 67.319 s | 24.531 s | 2.7x | 2.8x |

Stage detail shows the native advantage is concentrated in data loading
(4.7-5.5x), research evaluation (1.9-4.5x) and CSV export (3.3-3.7x). The
factor kernel itself is effectively tied (0.8-1.0x): pandas/NumPy vectorized
rank plus a sliding-window stable correlation is already efficient, while the
native runner uses scalar rank/correlation loops.

The next optimization pass targets the native load, research and export hot
paths with a goal of **at least 10x end-to-end speedup versus Python** on the
full A-share panel, without changing the factor/research semantics or the
1,044-test result. Current reports:

- [500-symbol report](alpha003_a_stocks/results/report.md) (generated, local)
- [full-universe report](alpha003_a_stocks/results-full/report.md) (generated, local)
- [benchmark README](alpha003_a_stocks/README.md) (method, tables, parity fixes)
- [comparison runner](alpha003_a_stocks/run_benchmark.py)

## Native-only harnesses

| Source | Purpose | Build / use |
|---|---|---|
| `factor_pipeline_benchmark.cpp` | Iteration 7/8 compute-once pipeline measurements; JSON per case | CMake target `fincore_factor_benchmark` (excluded from `all`) |
| `factor_alignment_benchmark.cpp` | Iteration 7 factor-alignment C4-vs-current harness | Manual build; POSIX `getrusage`; not a CMake target |
| `factor_layout_comparison.cpp` | Layout/memory experiment with `fork`/`waitpid` | Manual build; POSIX only; not a CMake target |

These are native-only experiments. The first target builds on the supported
platforms; the latter two retain POSIX process/memory APIs and are not part of
the Windows portability gate.
