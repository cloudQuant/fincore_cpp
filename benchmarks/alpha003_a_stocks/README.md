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

## Numeric result — full universe, 3 repeats

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

Historical medians on the same Windows 11 host (MSVC 19.42 Release). The native
parallel loader and horizon workers were removed, but equal CPU budget was
not established by the recorded CPU field: the C++ runner uses `std::clock()`,
which measures wall time in MSVC. Its 6.804 s / 6.804 s cannot prove CPU usage.
Python uses `time.process_time()` (62.4 s versus 66.5 s in one full run).
The runner also fixes Python before C++ and lacks enforced affinity/library
thread limits. Python total is a stage sum; C++ total includes unitemized work.
Use the external process-wall rows for historical end-to-end comparisons.
These records are not the corrected Iteration 9 performance baseline.

### 500 symbols, 3 repeats, historical single-worker configuration

| Stage | Python | C++ | Python / C++ |
|---|---:|---:|---:|
| load | 1.283 s | 0.069 s | 18.6x |
| factor | 0.081 s | 0.023 s | 3.6x |
| research | 3.681 s | 0.263 s | 14.0x |
| export | 2.723 s | 0.211 s | 12.9x |
| **total stage** | **7.782 s** | **0.574 s** | **13.6x** |
| wall (process) | 8.445 s | 0.608 s | 13.9x |

### Full universe, 3 repeats, historical single-worker configuration

5,449 evaluated assets, 1,255,737 factor cells, 1,136,571 signal cells and
3,404,416 exported label rows; the CSV output is roughly 32.5 MB at 500
symbols and 347 MB full.

| Stage | Python | C++ | Python / C++ |
|---|---:|---:|---:|
| load | 14.785 s | 0.666 s | 22.2x |
| factor | 1.095 s | 0.296 s | 3.7x |
| research | 23.086 s | 3.665 s | 6.3x |
| export | 28.157 s | 2.350 s | 12.0x |
| **total stage** | **66.485 s** | **7.860 s** | **8.5x** |
| wall (process) | 67.398 s | 7.973 s | 8.5x |

### Before vs after (historical measurements)

Before this optimization program the native benchmark took 1.740 s at 500
symbols and 24.531 s full. The later runner-total medians are 0.574 s and
7.860 s: the native implementation is about **3.0-3.1x** faster, and the
end-to-end speedup versus Python moved from **4.1x to 13.6x** at 500 and from
**2.7x to 8.5x** full. The parallel run that reached 10.1x/13.5x is recorded
in history and is not interchangeable with the single-worker measurements.

### Data-structure and algorithm opportunities

The remaining single-thread costs are research (3.665 s full) and export
(2.350 s). Feasible improvements, without parallelism or output changes:

1. **Reuse the preprocessed rank order.** `cross_section_rank` already sorts
   every cross-section; IC, quantile, stability and turnover then sort/rank
   factor in later stages. Carrying compatible order/workspace through the
   prepared rows may remove redundant sorting; each horizon must still rank
   its own valid sample set. The earlier 20-30% estimate is unverified.
2. **Compact label/signal rows.** The native tables currently materialize
   millions of per-row `std::string` asset/period fields and one-element
   `forward_returns` vectors. Interning asset IDs and using a fixed
   single-horizon return field would cut allocation and memory traffic,
   especially in label assembly and export.
3. **Direct row-buffer serialization.** Table writers still build a
   `std::vector<std::string>` per row and a temporary string per number.
   The implementation already has a reusable output buffer, date cache and
   `to_chars`; direct typed appends can remove remaining row containers and
   temporary strings. The earlier 30-40% headroom estimate is unverified.
4. **Loader/format micro-work.** Whole-file parsing is already used;
   memory-mapped or larger batched reads and a decimal-format fast path can
   shave the remaining load/format overhead.

These are optimization candidates, not a proven 15-20x/11-13x ceiling.

### Iteration 9: 30x minimum and 100x stretch

[The new plan](../../docs/iterations/iteration-9-performance-30x-100x/README.md)
requires at least 30x and targets 100x for the complete 500-file and full-panel
workloads under the same single-core budget. Using historical process-wall
times only as an illustration, full-panel C++ would need <=2.247 s for 30x
or <=0.674 s for 100x; corrected baselines must be measured first.

Current load/factor times and output volume do not establish a physical
impossibility for 50x or 100x. They also do not guarantee those targets are
reachable. The plan measures the remaining bottlenecks, aligns complete
workloads and permits evidence-driven implementation revisions without
lowering the minimum target. Summary/hash or multicore results are reported
separately and cannot replace full-output single-core acceptance. Historical
raw results remain unchanged; no new optimization or acceptance is claimed.

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
