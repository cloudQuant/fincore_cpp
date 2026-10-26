# Windows portability changes — Iteration 8

Date: 2026-10-24. Scope: make the accepted native surface build, test and
install reproducibly with MSVC 19.42 on Windows 11. POSIX behaviour is
unchanged; every platform-specific branch is compiled only on Windows.

## Repository and build

1. **Restored the ECharts 5.5.0 notice set.** `licenses/echarts-5.5.0/`
   (`LICENSE.txt`, `NOTICE.txt`, `manifest.json`) is a hard configure
   dependency in `cmake/fincore_report_assets.cmake` but was absent from the
   tree. The files were restored from the official npm package
   `echarts@5.5.0`; `assets/report/echarts.min.js` matches the pinned
   SHA-256 `42f8329d…3946`.
2. **Byte-preserving checkout.** `.gitattributes` marks the pinned browser
   asset, fixtures, ledgers and evidence as `-text`. With the host's
   `core.autocrlf=true`, Git had converted the asset to CRLF (45 extra bytes),
   failing the CMake digest check before compilation.
3. **MSVC source encoding.** `CMakeLists.txt` adds `/utf-8` for MSVC, removing
   C4819 warnings and preventing codepage-dependent parsing.
4. **Sanitizer mapping.** `cmake/fincore_sanitizers.cmake` emits
   `/fsanitize=address` for MSVC ASan and fails fast for MSVC UBSan with
   guidance to use Clang/GCC. No sanitizer was enabled on the Windows
   Release leg.
5. **Asset embedder.** `cmake/fincore_report_assets.cmake` now reads the asset
   as hex and emits 16 KiB string chunks decoded once into a static
   `std::string`. This avoids the MSVC string-literal limit (a 60,000-byte
   literal still fails; 16,000 bytes is safe) and CMake's text-mode newline
   translation, which had changed every LF in the embedded JS to CRLF. The
   generated report now contains the pinned asset byte-for-byte
   (`windows-asset-embedding.json`).

## Runtime

6. **Exclusive artifact writes.** `src/runtime/runtime.cpp` uses
   `_wopen/_write/_close/_wunlink` with `_O_BINARY | _O_NOINHERIT` on Windows;
   POSIX keeps `open/write/close/unlink` with `O_NOFOLLOW`. `O_EXCL` still
   refuses an existing entry (including a reparse point), the symlink/reparse
   parent checks are unchanged, and writes remain chunked.
7. **Windows artifact-name defence.** Windows-only validation rejects
   alternate-data-stream colons, reserved device names (`CON`, `NUL`,
   `COM1`–`COM9`, `LPT1`–`LPT9`, with or without an extension), and components
   ending in a dot or space. These names are accepted on POSIX exactly as
   before.
8. **No `__int128`.** `src/metrics/drawdown.cpp` computes UTC day spans with
   an exact int64 floor-divmod decomposition. Results are identical to the
   previous `__int128` implementation for every `int64` timestamp and the code
   now compiles on MSVC.
9. **Raw NUL literals.** 31 character literals in 13 source/test files
   contained a literal NUL byte between quotes. GCC/Clang tolerated it; MSVC
   rejects it with C2137. They are now written as `'\0'`.

## Tests and tooling

10. **Windows file locking.** The factor-research export test scopes its
    `std::ifstream` objects before removing the temporary tree; Windows refuses
    to delete files that still have open handles.
11. **Reparse-point test.** The artifact-bundle test creates a directory
    junction when `create_directory_symlink` lacks the Windows symbolic-link
    privilege. The library rejects the junction as a non-directory parent and
    the no-escape assertions still run.
12. **Evidence tools.** Python reads/writes use explicit UTF-8 and LF;
    `iteration8_source_manifest.py` includes `licenses` and `.gitattributes`;
    `iteration8_artifact_check.py` accepts an artifact directory outside the
    repository and can skip PDF rasterization when `pdftoppm` is unavailable
    (PDF text checks remain mandatory).

## Not exercised on Windows

- Ubuntu arm64/x86_64 CTest: no Linux runner, WSL or Docker on the host.
- P109 paired performance: the C4 baseline archive is unavailable.
- Windows ASan/UBSan, ImPlot offscreen captures and the GLFW demo: not run.
- PDF page rasterization: `pdftoppm` is absent; PDF text values were checked.
