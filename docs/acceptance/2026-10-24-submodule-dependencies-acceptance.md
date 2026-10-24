# Submodule Dependencies Acceptance

## Scope

`DataFrame`, `ImPlot` and `Dear ImGui` are now pinned git submodules under
`third_party/`, so a recursive clone builds without external checkouts or
`-D` dependency paths.

## Pins

| path | repository | commit |
| --- | --- | --- |
| `third_party/DataFrame` | `cloudQuant/DataFrame` | `90aa4acf6437d5a34f7ab8dfb6e9250c3d53d230` |
| `third_party/implot` | `cloudQuant/implot` | `7eeb9168d2e5e6b14e266d8782ecf7e649dfc3a4` |
| `third_party/imgui` | `cloudQuant/imgui` | `4e99c0ab1a28c2b3d1f250ec0266dbf1e455ef58` |

The DataFrame pin is the commit the previous CI checkout used; the ImPlot pin
is the locally validated adapter revision; the ImGui pin is the current
`cloudQuant/imgui` master commit. All three are fixed commits, not branches.

## Build wiring

- `FINCORE_DATAFRAME_SOURCE_DIR`, `FINCORE_IMPLOT_SOURCE_DIR` and
  `FINCORE_IMGUI_SOURCE_DIR` default to empty cache paths.
- Immediately after the options, each variable falls back to the matching
  `third_party/` tree when that tree exists; explicit cache values still win.
- `.github/workflows/ci.yml` checks out with `submodules: recursive` and no
  longer checks out DataFrame separately or passes a dependency path.

## Verification

- Clean recursive clone of commit `bbd7de9` (with local submodule remotes) on
  macOS arm64, no dependency overrides: Release configure/build/ctest rc 0,
  **735/735 passed**.
- The same commit on Ubuntu 24.04 arm64 (Lima, cmake 3.28.3, g++ 13.3.0,
  Ninja 1.11.1) with `submodule update --init --recursive`: submodule init rc
  0, configure/build/ctest rc 0, **735/735 passed** (3.26 s). Evidence in
  `/Users/yunjinqi/Documents/new_projects/fincore-linux-evidence/`
  (`submodule-init.log`, `configure2.log`, `build2.log`, `ctest2.log`,
  `submodule-run.log`).
- `FINCORE_BUILD_IMPLOT=ON` plus `FINCORE_BUILD_IMPLOT_DEMO=ON` builds
  `fincore_implot_demo` against the ImGui submodule (4e99c0a, ImGui 1.93.1
  WIP) and ImPlot 7eeb916; the offscreen capture run
  (`FINCORE_IMPLOT_CAPTURE=/tmp/implot-submodule.ppm`) exited 0 and produced a
  49,152,017-byte PPM.
- `git submodule status` after initialization reports exactly the three pins.

## Claim boundary

735 tests is the count at commit `bbd7de9`; uncommitted work in the local
worktree is not part of this change. Windows CI remains **NOT_RUN**, and no
interactive GUI verification is claimed (the demo check is offscreen only).
