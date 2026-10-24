# Iteration 8 — Windows 11 handoff

**Date:** 2026-10-24
**Branch:** `dev` (remote `origin` = `https://github.com/cloudQuant/fincore_cpp.git`)
**Oracle:** Python fincore `3e9ef205ef2023f92e89fddd1052d46b16429c64` (`0.5.2.dev0`)
**Native base:** C4 manifest `245ae75336912e30bcf6f34016ed4451116ec4ed719771a02d4b6e46ab0516e3`
**Goal:** complete Iteration 8 native full parity and close its acceptance on Windows 11.

## 1. What is already done

Functional implementation and the macOS acceptance legs are complete:

- 952-route split: 928 `NATIVE_TARGET` + 24 `EXTERNAL_OPTIONAL`; 0 unexplained
  gaps; split frozen for Iteration 8 execution.
- 270/270 native-compatible source runtime operations registered; the 3 live
  provider operation IDs are the only excluded ones.
- 228 Python test files accounted for in `test-port-ledger.tsv`.
- Native modules added: structured errors, data provider/snapshot models,
  extension/plugin ABI, Plotly/Bokeh documents, HTML builder, native PDF,
  factor artifact/chart-data builders, serialization/calendar/workflows and
  attribution/risk/report facades.
- macOS arm64 Release: **1044/1044** tests pass.
- macOS arm64 ASan+UBSan (Homebrew LLVM 22.1.8): **1044/1044** pass with
  `detect_container_overflow=0` due to a reproducible libc++ annotation false
  positive; heap/stack/global/UAF and UBSan remain enabled.
- Installed relocated headless consumer: build/run exit 0, prints
  `fincore_headless_consumer: ok`.
- ImPlot offscreen captures for general/factor/groups/events/specialized;
  PNG hashes and inspected images retained.
- HTML/ECharts/XLSX/Plotly/Bokeh/PDF artifact value checks: all passed.
- Route ledger: 648 `NATIVE_PASS`, 270 `NATIVE_PASS_REGISTRY`, 10
  `NATIVE_PASS_SCENARIO`, 24 `EXTERNAL_OPTIONAL`.

Evidence index: `docs/evidence/iteration-8/README.md`.
Acceptance record: `docs/acceptance/2026-10-24-iteration-8-native-parity-acceptance.md`.

## 2. Open gates (why this handoff exists)

1. **Ubuntu arm64/x86_64 platform legs are not complete.** Docker builds were
   attempted; the Docker VM compile throughput was too low (arm64 clang -O1
   reached 36/243 objects after ~10 minutes; x86_64 apt under Rosetta did not
   finish installing). A native Linux VM build was started and then stopped at
   the owner's pause request after 36 objects. **No Linux CTest result exists.**
2. **Windows is not attempted yet.** This is the new target.
3. **Performance is INCONCLUSIVE, not PASS.** Two paired C4-vs-current runs
   (5 and 10 pairs) produced equal digests but dispersion 11–26% because the
   host carried unrelated compiles/Docker work (loadavg 22–31 on 8 cores); 4/5
   CI upper bounds exceeded 1.10. Raw data:
   `docs/evidence/iteration-8/p109/performance.json` and
   `performance-quiet.log`. Do not lower the threshold; run on a genuinely
   idle host (or a dedicated Linux runner).

The acceptance record is therefore **IN_PROGRESS / PLATFORM-BLOCKED**, not
accepted. Do not mark it PASS until the Linux/Windows gates and a clean
performance run exist.

## 3. Windows 11 build plan

Prerequisites: Visual Studio 2022 (MSVC 19.4+ with C++23 support), CMake ≥3.25,
Ninja, Git, optionally LLVM/Clang-cl and GLFW for the ImPlot demo.

```bat
git clone https://github.com/cloudQuant/fincore_cpp.git
cd fincore_cpp
git submodule update --init --recursive
cmake -S . -B build-win -G Ninja -DCMAKE_BUILD_TYPE=Release ^
  -DFINCORE_BUILD_TESTING=ON ^
  -DFINCORE_DATAFRAME_SOURCE_DIR=%CD%/third_party/DataFrame
cmake --build build-win -j 8
ctest --test-dir build-win --output-on-failure
```

If MSVC rejects a C++23 construct, try `clang-cl` or `/std:c++latest`; record
the compiler and flags in the acceptance record. The expected suite is 1044
tests.

### Known portability work

- `src/runtime/runtime.cpp` includes `<fcntl.h>`, `<unistd.h>`,
  `<sys/types.h>` and uses `::open/::write/::close/::unlink` with
  `O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW` in `write_bundle_file`. Port to
  `std::ofstream` plus an exclusive-create / no-overwrite check, or use
  `_open/_write/_close/_unlink` with `_O_BINARY` under `_WIN32`. Preserve the
  no-symlink/no-overwrite behavior.
- `benchmarks/factor_alignment_benchmark.cpp` and
  `benchmarks/factor_layout_comparison.cpp` include `<unistd.h>`/`<sys/wait.h>`
  and use fork/waitpid/getrusage. They are not part of the CMake test target;
  the performance harness must be ported (spawn processes with
  `CreateProcess`/PowerShell) before a Windows performance claim.
- `examples/implot_dashboard.cpp` uses GLFW/OpenGL and `glReadPixels`; link
  `glfw3`/`opengl32` on Windows and build with `FINCORE_BUILD_IMPLOT=ON`.
  Offscreen captures should work with a hidden GLFW window.
- `tests/consumer/headless/main.cpp` uses `std::filesystem`; rebuild it from a
  relocated copy against the installed prefix, as on macOS.
- Re-run `tools/iteration8_source_manifest.py`, `tools/iteration8_route_split.py`,
  `tools/iteration8_test_port_ledger.py`, `tools/iteration8_route_ledger.py`
  and `tools/iteration8_artifact_check.py` after Windows changes; update
  `docs/evidence/iteration-8/SHA256SUMS`.

## 4. Acceptance closeout checklist

- [ ] Windows 11 Release CTest: 1044/1044 (or record exact failures and fixes).
- [ ] Windows consumer/install: exit 0 and v2 artifact manifest verified.
- [ ] Ubuntu arm64 and x86_64 native-runner CTest; declare execution mode.
- [ ] Quiet paired performance: all cases PASS or a recorded owner-accepted
      disposition; no threshold relaxation.
- [ ] Update acceptance platform/performance rows and sign-off.
- [ ] Regenerate source manifest, evidence SHA256SUMS and optionally the
      source archive.
- [ ] Do not commit hidden credentials; live provider/PyMC routes remain
      `EXTERNAL_OPTIONAL`.

## 5. Evidence and pins

- Oracle: `3e9ef205ef2023f92e89fddd1052d46b16429c64`.
- C4 source manifest: `245ae75336912e30bcf6f34016ed4451116ec4ed719771a02d4b6e46ab0516e3`;
  archive `168789ea6fabe1ed4baf636bbcda3e7e372dca53e18a5a7a9f08ef9b4906a7da`.
- Native submodules: DataFrame `90aa4acf6437d5a34f7ab8dfb6e9250c3d53d230`,
  ImPlot `7eeb9168d2e5e6b14e266d8782ecf7e649dfc3a4`, ImGui
  `4e99c0ab1a28c2b3d1f250ec0266dbf1e455ef58`.
- Route split: `docs/iterations/iteration-8-full-python-parity/route-split.tsv`.
- Route ledger: `docs/iterations/iteration-8-full-python-parity/route-ledger.tsv`.
- Test port ledger: `docs/iterations/iteration-8-full-python-parity/test-port-ledger.tsv`.
- macOS Release/ASan, captures and artifacts: `docs/evidence/iteration-8/`.
- Windows build logs should be added under `docs/evidence/iteration-8/p110/`
  with the compiler version, CMake command and CTest summary.
