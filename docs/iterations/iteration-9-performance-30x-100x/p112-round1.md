# P112 R1: complete research output and strict comparison

Date: 2026-10-25. Repository/branch: `D:/source_code/fincore_cpp`, `dev`.
Base: `55d6e44ed2d9a57ef7104dbb4f1523fafff9c64e`.
Scope: the new detailed research API/exporter and their internal bridge, native
tests and Python oracle, both alpha003 runners, the complete comparison contract,
its tests, CMake registration, and this round's documentation.

This round establishes a correctness reference candidate. It does **not** accept
the 500-file/full-data performance baseline, a speed improvement, or 30x/100x.
The separately accepted empty-group prerequisite is recorded in
[p112-empty-groups.md](p112-empty-groups.md), commit `55d6e44`; its 1045 passing
legacy tests were not repeated for this delivery.

## Implementation and compatibility

`evaluate_factor_research_detailed` returns an owning, typed v2 result, keeping the
old research API and exporter intact. Its ten factor-local tables contain signals,
full panel-by-period labels including missing tails, Pearson/rank IC and both
summaries, quantile returns, turnover, spreads, stability, coverage and diagnostics.
The benchmark additionally writes factor values. The single-factor API supports
price-derived `close_to_close` and `next_open_to_open` labels. It explicitly rejects
precomputed labels and multi-factor combinations/comparisons; cross-factor tables
are empty and identified as such in the export manifest.

The v2 preparation follows the pinned Python implementation's finite sample set,
pandas/NumPy quantile interpolation and right-closed boundary placement, duplicate
boundary/loss behavior, and per-quantile observed-date turnover lag. Its correlation
calculation follows NumPy's covariance scaling and successive standard-deviation
divisions. This preserves near-perfect daily correlations whose tiny dispersion
matters to ICIR. Stability ranks include infinities while excluding NaNs.

The exporter writes UTF-8/LF, UTC ISO8601 Z timestamps, round-trip binary64 values,
empty NaNs and explicit infinities. It rejects nonempty destinations and cleans
only files created by its own failed attempt. Canonical row order is preserved.
The Python runner retains the reference library's computations and uses pandas'
normal float serialization. Shared workload manifests describe the same inputs,
configuration and serialization obligation; engine metadata remains separate.

`alpha003-complete-v2` compares all eleven CSVs in bounded chunks. It checks exact
columns, ordered unique keys, categories/counts, NaN/infinity masks, manifest row
counts and workload metadata; every floating field uses absolute error <= 1e-6.
It does not aggregate, drop rows, inner-join mismatched keys or give stability a
looser threshold. Every prewarm and measured pair is compared outside its timer.

## Correctness evidence

Reference Python checkout: `3e9ef205ef2023f92e89fddd1052d46b16429c64`.
Environment: Windows, MSVC Release `/O2 /Ob2 /DNDEBUG`, Python 3.14.2,
NumPy 2.4.1 and pandas 2.3.3.

```text
cmake --build build-win --target fincore_tests fincore_research_detailed_oracle fincore_alpha003_research -j 4
build-win/tests/fincore_tests.exe --gtest_filter=FactorResearchDetailed* --gtest_output=json:build-win/p112-focused-tests-r3.json
C:/Python314/python.exe tools/check_research_detailed_oracle.py --probe build-win/fincore_research_detailed_oracle.exe
C:/Python314/python.exe -W error -m unittest tools.tests.test_research_contract tools.tests.test_alpha003_benchmark -v
C:/Python314/python.exe benchmarks/alpha003_a_stocks/run_benchmark.py --max-symbols 50 --repeats 1 --cpp-exe build-win/fincore_alpha003_research.exe --results-dir benchmarks/alpha003_a_stocks/results-i9-p112-smoke-r1
```

- Native focused suite: **13/13 passed**, including API, missing-price/duplicate-lag,
  infinity-rank, exporter serialization and failure-cleanup cases. Result SHA-256:
  `d123356cb93fc17e9d62f34d89eb3496fc8fdc13206c73d9c721e8678ee5dc35`.
- Actual C++ evaluator/exporter against the frozen public Python oracle: **all
  ten tables passed for ties, constant and near-constant cases**, including every
  row, null mask, order and diagnostic. Oracle log:
  `build-win/p112-oracle-r3.log`, SHA-256
  `fd3db39f6af6ac016a4e623e5a01a18c6405db3520cc78a7fb3f8d9eb79e4f07`.
  Fixture SHA-256:
  `d7e84e444b98ed438e6daadd1ec4217bb894b935246569ebdbc3e5cd57ad0e8e`.
- Python comparison/orchestration tests: **18/18 passed**, warnings treated as errors.
- Two additional public-API rejection tests passed for duplicate factor keys and
  duplicate price-grid timestamps (`build-win/p112-negative-tests.json`). They
  were run separately after adding only those tests, without repeating the
  accepted thirteen-test suite. Python's non-UTC-index fixture is not claimed as
  an executed C++ error case: the native timestamp type represents UTC epoch
  nanoseconds and has no analogous timezone-bearing index input.
- Real 50-file pipeline: two warm-up pairs plus one sample pair, **all eleven
  tables and manifests passed for all three pairs**, maximum absolute difference
  `3.3306690738754696e-16`. The sample contains 9,392 factor rows, 8,580 signals,
  25,740 labels (55 missing labels), 636 IC rows, 3 IC summaries, 3,180 quantile
  returns, 3,050 turnovers, 636 spreads, 3 stability rows and one coverage row.
  Diagnostics are empty on both sides. Different equivalent floating text lengths
  are allowed; values and masks are checked. This small diagnostic is not a formal
  performance campaign and is not extrapolated to 500/full.

Smoke evidence remains in the fresh ignored result directory above. Its
`inputs.json` SHA-256 is
`455e5074bb59bf00c4de8d66ab82a8c80ad6a9b841afac104628341c473b0d10`,
`measurements.json` is
`11b0810c2084c3888e301d0e1244eebc78515ebdf6cb051bc70358997f7dbeeb`,
and `comparison.json` is
`41b10ad97ee5513973d0c4772c1cc42ac5732203a47600960904b0f776a18438`.
Build/test logs and initial failed qcut/correlation checks remain in `build-win`;
the successful result does not overwrite those earlier failures.

## Next round

Freeze this accepted source commit and reference executables, then run the paired
500/full baseline with the same single-CPU budget and full output checks. Establish
the actual stage/resource budget before optimizing. Source inspection suggests
merging duplicate per-date IC extraction and removing exporter temporary strings;
neither is a measured speedup yet. The v2 path already reserves signal/label vectors
and uses a 1 MiB export buffer, so those are not future optimization claims.
