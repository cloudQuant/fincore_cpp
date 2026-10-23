# Sanitizer Gate Acceptance — Full ASan+UBSan Suite

## Scope

Closes the open sanitizer gate for Iterations 4–6 by running the complete test
suite under AddressSanitizer and UndefinedBehaviorSanitizer at the final HEAD,
including the visualization-era report, factor, portfolio, runtime and
attribution changes.

## Environment and command

- Homebrew clang 22.1.8, Ninja generator, Debug build, macOS arm64.
- Flags on every target: `-fsanitize=address -fsanitize=undefined`.

```sh
cmake -S . -B /tmp/fincore-sanitize -G Ninja \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
  -DCMAKE_BUILD_TYPE=Debug \
  -DFINCORE_ENABLE_ASAN=ON -DFINCORE_ENABLE_UBSAN=ON \
  -DFINCORE_BUILD_TESTING=ON \
  -DFINCORE_DATAFRAME_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/DataFrame
cmake --build /tmp/fincore-sanitize -j 8
ctest --test-dir /tmp/fincore-sanitize --output-on-failure
```

## Result

- Configure: rc 0 (`/tmp/fincore-sanitize-configure.log`).
- Build: rc 0; only linker DWARF-parsing warnings from the fetched GoogleTest
  static archives (`/tmp/fincore-sanitize-build.log`,
  `/tmp/fincore-sanitize-rebuild.log`).
- Tests: **731/731 passed**, 0 failed across the final follow-up runs
  (`/tmp/fincore-sanitize-ctest.log`). No ASan or UBSan report was emitted.
- The rebuild after the turnover projection ran before this test pass, so the
  result covers the current source revision.

## Claim boundary

This is a local macOS arm64 sanitizer result. It does not replace the
Release suite, does not exercise the optional ImPlot GUI adapter (disabled in
this build), and does not provide Linux/Windows sanitizer evidence. Visual
rendering remains **NOT_RUN**.
