# Iteration 6 slice 2 Acceptance — XLSX and Native Packaging

## XLSX renderer (Phase 90)

- One worksheet per `report_section` (sheet name = section key, sanitized and
  truncated to 31 characters; duplicate names receive `_2` suffixes; an empty
  document gets one `report` sheet).
- Each sheet stacks: a metrics header (`metric/label/value/unit`) and metric
  rows, then each table as a key/title row, header row and typed rows, then
  each series as a key/title row, axis-label row and x/y rows.
- Cell types: numbers (`<v>`), inline strings (`t="inlineStr"`, XML-escaped,
  `xml:space="preserve"`), booleans (`t="b"`), missing cells omitted. Text is
  always stored as inline text, never as an Excel formula, so untrusted text
  beginning with `=`/`+`/`-`/`@` cannot execute.
- Dependency-free writer: stored (uncompressed) ZIP with fixed DOS timestamps
  (1980-01-01), CRC-32, deterministic entry order and workbook/sheet/styles
  parts. Byte repeatability is native, not byte equality with Python
  openpyxl/pandas output.
- Validation: native tests pin the ZIP magic, determinism, sheet parts,
  escaped text and number/boolean cells. The probe workbook
  `/tmp/phase90.xlsx` was loaded with openpyxl 3.1.5: sheets `['first']`, rows
  `metric/label/value/unit`, `ratio/Ratio/1.5/x`, a blank row,
  `t/Table`, `name/value`, `A&B/0.25`.
- Declared differences: Python's writer emits one object column per metric
  and pandas object cells; native emits typed metric columns. Native styles
  are a minimal default style (openpyxl warns about the missing default style
  but loads the file). Sheet-name collision handling is native.

## Native packaging (Phase 93 slice)

- `cmake/fincore_install.cmake` installs only the `fincore` component:
  library, public headers, `fincoreTargets.cmake`, a relocatable
  `fincoreConfig.cmake`/version file and README/LICENSE. The optional ImPlot
  adapter and GUI dependencies are **not** installed, preserving the
  GUI/core separation.
- The config uses `find_dependency(DataFrame CONFIG)` only when the consumer
  has not already provided `DataFrame::DataFrame`.
- Installed to `/tmp/fincore-phase93-install2`; a downstream consumer
  (`/tmp/fincore-phase93-consumer`) configured with
  `CMAKE_PREFIX_PATH`, built and ran:
  `version=0.1.0 sharpe=6.928203230275507` (exit 0).
- The install command uses `--component fincore` so fetched DataFrame/gtest
  install rules do not populate the fincore prefix.

## Declared gaps

- XLSX is not byte-identical to Python and does not reproduce pandas
  object-cell formatting; no charts/images/style variants.
- The ImPlot adapter has no install/export component; GUI consumers build it
  from source.
- Supported-platform CI matrix, benchmark regression and migration notes are
  not executed in this environment (macOS local build only).
- 88b field completion, factor research/PIT/portfolio constructor, full factor
  tears, ImPlot tears and the Phase 87 study remain open.

## Gates

Full headless CTest: **701/701 passed**. Sanitizer: not re-run (no claim).
ImPlot adapter compile/link unchanged.
