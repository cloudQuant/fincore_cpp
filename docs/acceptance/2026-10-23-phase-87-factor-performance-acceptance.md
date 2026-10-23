# Phase 87 Acceptance — Native Factor Pipeline Performance Study

## Scope

Adds a standalone benchmark target (`fincore_factor_benchmark`, excluded from
the default build and install) that measures the compute-once factor pipeline
and its report renderers on dense and sparse synthetic panels. No production
code path changes; the library and tests are unchanged.

## Method

`benchmarks/factor_pipeline_benchmark.cpp` builds prepared panels directly
with deterministic factors and two forward horizons, rank-assigns five
quantiles per date, runs `analyze_factor` (periods 1D/5D, turnover lags 1/5,
monthly IC, portfolio inputs on), copies the model, builds the four-section
report, and renders HTML and XLSX. Analysis time is the minimum of three
runs; copy/report/render are single runs. The benchmark prints one
`schema=1` header and one line per case.

Command:

```
cmake --build /tmp/fincore-dev-verify --target fincore_factor_benchmark
/tmp/fincore-dev-verify/fincore_factor_benchmark
```

Environment: Apple M2 (arm64), macOS 26.5.1, Apple clang 17.0.0, CMake
Release, raw log `/tmp/fincore-phase87-benchmark.log`.

## Measurements

```
CASE dense  dates=250 assets=100  rows=25000 analyze_ms=95.345  fingerprint_stable=1 copy_ms=0.899 isolated=1 report_ms=3.287 html_ms=0.404 html_bytes=59008  xlsx_ms=1.050 xlsx_bytes=165768 rss_mb=38.2
CASE sparse dates=250 assets=1000 rows=12500 analyze_ms=263.877 fingerprint_stable=1 copy_ms=3.067 isolated=1 report_ms=2.338 html_ms=0.387 html_bytes=58970  xlsx_ms=0.995 xlsx_bytes=165655 rss_mb=109.3
```

- Repeating the analysis produces an identical fingerprint, and copying the
  model plus mutating the copy leaves the source unchanged (deep ownership).
- Report projection (3.3/2.3 ms), HTML (0.4 ms, ~59 KB) and XLSX (~1 ms,
  ~166 KB) are small compared with analysis.
- The wide sparse panel is ~2.8x slower than the dense panel despite half
  the rows (264 ms vs 95 ms) and raises peak RSS from 38 MB to 109 MB.
  Per-asset bookkeeping across the 1000-name universe, not row count,
  dominates this workload.
- Model copies scale with panel width (0.9 ms vs 3.1 ms).

## Limits

Single process, single machine, synthetic panels; no Python-side timing was
measured, so no cross-language performance claim is made. Report rendering is
measured once per case, not as a repeated-render amortization study. The
benchmark is not part of CTest and does not replace the numerical isolation
tests. Sanitizer not re-run; visual NOT_RUN.

## Gates

Full headless CTest: **710/710 passed** with the new target present but not
built by default.
