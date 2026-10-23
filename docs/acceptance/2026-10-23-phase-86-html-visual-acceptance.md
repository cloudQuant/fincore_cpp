# Phase 86 Acceptance — Headless HTML Visual Verification

## Scope

Adds `examples/render_factor_report.cpp` (excluded from the default build) to
render a synthetic factor report to an HTML file, then verifies the artifact
in headless Chrome via Playwright. This is a render of synthetic report data,
not a desktop screenshot.

## Artifact

Command:

```sh
cmake --build /tmp/fincore-dev-verify --target fincore_report_render
/tmp/fincore-dev-verify/fincore_report_render /tmp/factor-report.html
```

- HTML: 19,379 bytes, title `Factor Report`.
- Browser checks: five `<h2>` headings (`Factor summary`, `Information
  coefficient`, `Factor returns`, `Turnover and autocorrelation`, `Metadata`),
  **4 inline SVG charts**, **11 tables**, body 5081 px tall at 1440 px width.
- Full-page PNG: `/tmp/factor-report.png`, 1440 x 5145, 479,353 bytes. The
  image was inspected: sections, metric cards, table columns and both
  cumulative/IC chart pairs render with axes, labels and growth/correlation
  units; there is no clipping or overlap at the captured width.
- Rendered table audit confirms column alignment: quantile statistics keep
  min/max/mean/std/count/count%; the information summary has all eight
  columns; the returns summary keeps alpha/beta/top/bottom/spread rows.

## Claim boundary

This verifies the **HTML artifact** visually. It does not verify the optional
ImPlot GUI adapter, which remains **NOT_RUN**, and it is not a desktop
screenshot. Native document bytes remain non-identical to Python.

## Gates

The added example target is excluded from the default build; the full Release
and ASan+UBSan suites remain **731/731 passed**.
