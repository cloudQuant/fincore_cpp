# Phase 75 Acceptance — Offscreen ImPlot Visual Verification

## Scope

The ImPlot dashboard demo gains a headless capture mode
(`FINCORE_IMPLOT_CAPTURE=<path.ppm>`): it creates a hidden GLFW window with a
macOS OpenGL 3.2 core context, renders three frames, reads the framebuffer
with `glReadPixels`, writes a PPM, and exits. The interactive path is
unchanged. This is an offscreen render of synthetic data, not a desktop
screenshot.

## Defects found and fixed during visual review

1. **Heatmap assertion** — a single year/month passed one custom tick to
   `SetupAxisTicks`, producing "n_ticks must be at least 2" three times per
   frame. Single-label axes now use the axis label with hidden tick labels.
2. **Hard-coded heatmap span** — month ticks always spanned `0.5..11.5`
   regardless of the actual column count. The span now ends at
   `columns - 0.5`.
3. **Raw day-number axes** — series charts plotted days since the epoch with
   default numeric formatting (scientific tick labels). A date formatter now
   renders `YYYY-MM-DD` ticks.
4. **Repeated single labels** — the single-asset correlation matrix and
   single-group bar chart repeated the same label on every tick; they now use
   one axis label with hidden ticks.

## Verified artifact

```sh
cmake --build /tmp/fincore-implot-build --target fincore_implot_demo
FINCORE_IMPLOT_CAPTURE=/tmp/implot-dashboard.ppm /tmp/fincore-implot-build/fincore_implot_demo
# PPM -> PNG via Pillow
```

- Window 1280 x 2400 logical, 2560 x 4800 framebuffer, hidden.
- Capture rc 0 with **no ImGui error output**.
- Inspected image shows all panels: cumulative returns with date axes, monthly
  heatmap with a single `2021` year label and month ticks, rolling Sharpe,
  drawdown fill, grouped bars, histogram and correlation matrix.
- Missing heatmap months print `nan` (accurate missingness) rather than a
  fabricated zero.

## Claim boundary

This verifies the ImPlot **offscreen visual output** on macOS. Interactive
window behaviour, input handling, Linux/Windows rendering and the installed
adapter remain unverified. No desktop screenshot was taken.

## Gates

Full Release and ASan+UBSan suites remain **733/733 passed**; the demo target
is excluded from the default build.
