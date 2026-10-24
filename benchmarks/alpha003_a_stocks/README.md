# alpha003 A-share factor research benchmark

This benchmark runs the Alpha101 **alpha003** factor through two independent
research stacks on the same raw A-share panel:

- **Python**: pandas computes alpha003 and the public `fincore` package
  evaluates the factor research.
- **C++**: native `fincore_cpp` loads the panel, computes alpha003, evaluates
  the research and writes the same table set.

It then compares factor values and research tables with a strict tolerance and
reports stage-level timings. The comparison script exits non-zero when a
primary numeric check fails.

## Factor definition

Alpha003 is the negative ten-bar correlation of the cross-sectional open and
volume ranks:

```
alpha003 = -1 * correlation(rank(open), rank(volume), 10)
```

The implementation follows the Backtrader `alpha101/alpha003.py` reference:

- rank is the average tie rank divided by the number of eligible assets on
  that date;
- the rolling window needs 10 complete rank pairs;
- correlation uses the reference anchor-centered, two-stage-scaled Pearson
  kernel, which stays finite for near-constant windows;
- a constant window yields NaN; the result is negated.

The factor is computed independently by each runner from the raw CSVs; the
shared part is only the research contract.

## Data

`data/a_stocks/` is a copy of
`D:\source_code\backtrader\tests\datas\a_stocks`:

- 5,471 per-symbol CSVs, 243 observed sessions from 2025-01-02 to 2025-12-31;
- columns `trade_time,open,high,low,close,volume,amount`;
- `trade_time` is the last observed source minute on the Asia/Shanghai wall
  clock; prices are unadjusted;
- the benchmark uses the shared calendar in `_metadata/observed_calendar.csv`
  and converts local timestamps to UTC.

The 94 MB payload is gitignored to keep the repository small. See
[data/README.md](data/README.md) for provenance, integrity pins and the copy
command.

The default run uniformly samples **500** files, matching the source
Backtrader example. `--max-symbols 0` uses all files. The last 22 grid rows are
reserved (`--end-offset 22`) so the 20-day forward labels exist for every asset
in the study window.

## Build and run

Build the native runner once:

```powershell
cmake -S . -B build-win -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DFINCORE_BUILD_TESTING=ON `
  -DFINCORE_DATAFRAME_SOURCE_DIR=D:/source_code/fincore_cpp/third_party/DataFrame
cmake --build build-win --target fincore_alpha003_research --parallel
```

Run the comparison from the repository root:

```powershell
# 500-symbol benchmark, three repeats, medians in the report
python benchmarks/alpha003_a_stocks/run_benchmark.py --max-symbols 500 --repeats 3

# full available universe, one repeat
python benchmarks/alpha003_a_stocks/run_benchmark.py --max-symbols 0 --repeats 1 `
  --results-dir benchmarks/alpha003_a_stocks/results-full
```

The Python runner needs the local `fincore` checkout:

```powershell
$env:FINCORE_ROOT = "D:\source_code\fincore"
```

or pass `--fincore-root`. The default is that path. The C++ executable is found
automatically under `build-win/` (or `build/`); pass `--cpp-exe` if it lives
elsewhere. `--skip-run` reuses `results/<engine>/run1`.

Outputs:

```
benchmarks/alpha003_a_stocks/
  results/
    python/run1..N/   factor_values.csv + native fincore tables
    cpp/run1..N/      factor_values.csv + tables/*.csv
    logs/             per-process logs
    comparison.json   machine-readable checks and timings
    report.md         generated Markdown report
```

`results/` and `results-full/` are generated and gitignored.

## Numeric result — 500 symbols, 3 repeats

Data scale: 498 evaluated assets, 243 grid dates, 212 research dates after the
22-row tail reserve, 114,509 factor cells and 103,601 signal cells.

| Check | Rows (py/cpp/common) | Max abs diff | Status |
|---|---:|---:|---|
| factor panel | 114,509 / 114,509 / 114,509 | 2.22e-16 | PASS |
| signals, raw value | 103,601 / 103,601 / 103,601 | 0 | PASS |
| signals, processed value | 103,601 / 103,601 / 103,601 | 0 | PASS |
| labels, common cells | 310,803 / 310,234 / 310,234 | 0 | PASS |
| rank IC | 636 / 636 / 636 | 1.11e-16 | PASS |
| IC summary | 3 / 3 / 3 | 1.01e-16 | PASS |
| quantile mean | 15 / 15 / 15 | 8.67e-17 | PASS |
| quantile standard error | 15 / 15 / 15 | 9.39e-17 | PASS |
| spread | 3 / 3 / 3 | 8.89e-17 | PASS |
| turnover (1D) | 1,055 / 1,060 / 1,055 | 0 | PASS |
| stability, published summary | 3 / 3 / 3 | 1.82e-04 | PASS (secondary) |
| stability, per-date kernel | 610 common rows | 1.44e-15 | PASS |

## Numeric result — full universe, 1 repeat

Data scale: 5,449 evaluated assets (5,471 files minus late-history/no-window
assets), 1,255,737 factor cells and 1,136,571 signal cells.

| Check | Max abs diff | Status |
|---|---:|---|
| factor panel | 2.22e-16 | PASS |
| signals, raw + processed | 0 | PASS |
| labels, 3,404,416 common cells | 0 | PASS |
| rank IC / IC summary | 1.11e-16 / 0 | PASS |
| quantile mean / SE | 9.37e-17 / 9.15e-17 | PASS |
| spread | 8.17e-17 | PASS |
| turnover | 0 | PASS |
| stability, published summary | 1.35e-04 | PASS (secondary) |
| stability, per-date kernel | 5.44e-15 | PASS |

The full run has 5,297 Python-only label rows; every one is a NaN forward
return. C++ omits NaN labels from its export. All common label values match
exactly.

## Performance

Stage medians on the same Windows 11 host (MSVC 19.42, 32 logical CPUs). Python
runs pandas + fincore; C++ runs native fincore_cpp. Neither runner uses
parallelism.

### 500 symbols, 3 repeats

| Stage | Python | C++ | Python / C++ |
|---|---:|---:|---:|
| load | 1.061 s | 0.226 s | 4.7x |
| factor | 0.074 s | 0.092 s | 0.8x |
| research | 3.406 s | 0.761 s | 4.5x |
| export | 2.408 s | 0.657 s | 3.7x |
| **total stage** | **7.083 s** | **1.740 s** | **4.1x** |
| wall (process) | 7.709 s | 1.785 s | 4.3x |

### Full universe, 1 repeat

| Stage | Python | C++ | Python / C++ |
|---|---:|---:|---:|
| load | 12.906 s | 2.350 s | 5.5x |
| factor | 1.026 s | 1.072 s | 1.0x |
| research | 23.825 s | 12.231 s | 1.9x |
| export | 29.562 s | 8.832 s | 3.3x |
| **total stage** | **67.319 s** | **24.531 s** | **2.7x** |
| wall (process) | 68.202 s | 24.626 s | 2.8x |

### Interpretation

- **C++ is consistently faster overall**, but the end-to-end advantage is
  **4.1x at 500 symbols and 2.7x on the full universe**, not an order of
  magnitude. Load, research and CSV export dominate both stacks.
- **The factor kernel is essentially tied.** Pandas/NumPy rank plus a
  sliding-window stable correlation is slightly faster at 500 symbols
  (0.074 s vs 0.092 s) and tied at full scale (1.026 s vs 1.072 s). The native
  runner intentionally uses scalar per-date sorting and per-window
  correlation; it was not micro-optimized.
- **C++ research advantage shrinks with scale** (4.5x at 500, 1.9x full).
  Native research time grows from 0.76 s to 12.23 s (16x) while the asset count
  grows 10.9x; Python grows 7.0x (3.41 s to 23.83 s). Per-date maps, sorting and
  quantile construction in the native path do not scale as well as pandas'
  vectorized grouping on this workload.
- **Export is a large fraction of both totals** (34% of Python and 38% of C++
  at 500 symbols; 44% and 36% full). Both write the same multi-million-row CSV
  tables, so I/O limits the end-to-end speedup.
- These numbers are single-host observations, not a cross-platform performance
  claim. The 500-symbol rows are medians of three repeats; the full-universe
  row is one repeat.

## Parity bugs found and fixed

The comparison exposed three semantic divergences in the native research path.
They were fixed in `src/factor/research.cpp` / `src/factor/data.cpp`; the full
1,044-test C++ suite still passes.

1. **Cross-sectional rank scale.** The native `cross_section_rank` used
   `(rank - 1) / (n - 1)`; the Python contract uses `rank / n`
   (`rank(pct=True)`). This shifted signals' `processed_value` and some
   quantile boundaries.
2. **Quantile bucket cross-section.** Multi-horizon research assigned buckets
   once on the entire factor panel, then dropped rows without a forward return.
   Python assigns buckets per period on the cross-section that has a label.
   Missing labels therefore shifted bucket edges and aggregate quantile
   returns (~2e-4). Buckets are now assigned per horizon.
3. **Demeaned quantile returns.** The native `evaluate_factor_research` used
   the analysis default (`demeaned=true`) for quantile tables; Python research
   reports raw returns. The native research table now requests raw returns.

A benchmark-side numeric issue was also fixed: pandas `rolling.corr` returns
`±inf` for near-constant rank windows, while the reference kernel returns NaN
or a stable finite value. Both runners now use the same anchor-centered,
two-stage-scaled Pearson kernel, which made the factor cell sets and values
match exactly.

## Declared differences

- The C++ `ic.csv` table is a **rank IC (Spearman)**. It is compared with
  Python `rank_ic`. Python additionally reports Pearson `ic`; the native CSV
  export has no separate Pearson column.
- Python's `labels.csv` keeps NaN forward-return rows; C++ omits them. Only
  common finite values are compared.
- The published stability summary differs by 1.8e-04 (500 symbols) / 1.3e-04
  (full). Python's summary aggregates a slightly different prepared-cell set
  than the per-date reconstruction; the per-date rank-autocorrelation kernel
  itself matches to 1.4e-15 / 5.4e-15.
- Coverage tables have different shapes: Python reports factor-level
  input/finite/label/evaluable counts; C++ reports prepared-row counts per
  horizon. They are reported, not numerically gated.

## Limitations

- Unadjusted prices, no transaction costs, no capacity or investability
  filters. The result is a research-parity benchmark, not an alpha claim.
- The 22-row tail reserve changes the study window from 234 to 212 dates; this
  is deliberate and applied identically to both stacks.
- The 500-symbol sample is a deterministic uniform sample of sorted file names,
  not a market-cap or liquidity sample.
- Performance was measured on one Windows 11 host; the native and Python
  runners do not use threads. Treat stage ratios as indicative, not universal.
