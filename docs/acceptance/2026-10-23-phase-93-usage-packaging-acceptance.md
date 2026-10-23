# Phase 93 Acceptance — Native Usage, Install and Migration Notes

## Scope

Adds `docs/guides/native-usage.md` covering requirements, build/test/install
commands, component-only installation, downstream consumption, data/error
conventions, Python-to-native migration, supported platforms, sanitizer and
benchmark status, and third-party notices. Re-verifies the install/export and
downstream consumer at the current revision.

## Verified evidence

At commit containing this slice, on macOS arm64 / Apple clang 17 / CMake
Release:

```
cmake --install /tmp/fincore-dev-verify --prefix /tmp/fincore-phase93-install3 \
  --component fincore            # rc=0, library + headers + package files only
cmake -S /tmp/fincore-phase93-consumer -B .../build3 \
  -DCMAKE_PREFIX_PATH=/tmp/fincore-phase93-install3   # rc=0
cmake --build .../build3 -j 6                         # rc=0
.../build3/consumer
version=0.1.0 sharpe=6.928203230275507                # rc=0
```

A plain install (without `--component fincore`) also installs the fetched
GoogleTest targets; the usage guide directs consumers to the component form.
The consumer supplies its own DataFrame checkout and links
`fincore::fincore` through `find_package(fincore 0.1 CONFIG REQUIRED)`.
Re-verified again at commit `9ad642a` after the research/visual slices:
component install, downstream configure and build rc 0, consumer prints
`version=0.1.0 sharpe=6.928203230275507` (run rc 0).

## Documented migration facts

- No Python bindings or pandas objects are delivered; capability mapping is
  documented per function family and cross-linked to the program parity audit
  and machine ledger.
- Renames cover cumulative returns, VaR, Treynor, position stacking, factor
  positions, research capabilities, portfolio optimization and simulation
  helpers.
- Seven unresolved capabilities and the deferred/non-port scope decisions are
  listed rather than hidden.
- Timestamps are UTC nanoseconds, numeric frames use genuine `double`
  columns, and owning models keep mutation isolation.

## Declared gaps

- A pinned GitHub Actions workflow exists for Ubuntu and macOS. Its macOS
  steps were reproduced in a clean clone at commit `e9d1cef` with DataFrame
  `90aa4acf`: configure/build/ctest rc 0, **726/726 passed**. Its Ubuntu leg
  was reproduced in a Lima Ubuntu 24.04 arm64 VM at commit `a6a9fff` with the
  same DataFrame pin and cmake 3.28.3 / g++ 13.3.0 / Ninja 1.11.1:
  configure/build/ctest rc 0, **735/735 passed**. Windows remains **NOT_RUN**
  (no runner).
- Sanitizer status is tracked in the separate sanitizer gate record: full
  ASan+UBSan suite **735/735 passed** at `a6a9fff`; no later slice claims a
  run beyond that record.
- The benchmark baseline is recorded but not enforced as an automated
  performance-regression gate.
- The optional ImPlot adapter is not installed with the package.
- No Python wheels, bindings or drop-in package compatibility are provided.

## Gates

Full headless CTest at this revision: **714/714 passed**. This slice adds no
code and no test count; the install/consumer evidence above is from the same
build tree.
