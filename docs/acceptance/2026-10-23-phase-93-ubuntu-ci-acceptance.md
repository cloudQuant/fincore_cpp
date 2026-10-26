# Phase 93 Acceptance — Ubuntu CI Leg

## Scope

Reproduces the `ubuntu-latest` matrix leg of `.github/workflows/ci.yml`
locally: clean clone, pinned DataFrame checkout, Release configure, build and
`ctest --output-on-failure`.

## Environment

- Lima 2.x VM `fincore-ci`, Ubuntu 24.04 LTS arm64, kernel
  `6.8.0-87-generic`, 4 vCPU, 4 GiB RAM.
- Toolchain installed from Ubuntu archives: cmake 3.28.3, g++ 13.3.0
  (`Ubuntu 13.3.0-6ubuntu2~24.04.1`), Ninja 1.11.1.
- Source: clean clone of this repository at commit
  `a6a9fff9f63e02a2d6b31f48231d7388db20bd49` inside the guest.
- DataFrame: clean checkout at the CI pin
  `90aa4acf6437d5a34f7ab8dfb6e9250c3d53d230`, passed as
  `-DFINCORE_DATAFRAME_SOURCE_DIR` (the same content the workflow checks out
  into `dataframe/`). GoogleTest is fetched by CMake at tag `v1.15.2`
  (`b514bdc898e2951020cbdca1304b75f5950d1f59`).

## Commands

```sh
git clone /Users/yunjinqi/Documents/new_projects/fincore_cpp /tmp/fincore-linux-src
cd /tmp/fincore-linux-src && git checkout a6a9fff
cmake -S . -B /tmp/fincore-linux-build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DFINCORE_DATAFRAME_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/DataFrame
cmake --build /tmp/fincore-linux-build -j 4
ctest --test-dir /tmp/fincore-linux-build --output-on-failure -j 4
```

## Result

- Configure: rc 0 (183 build edges generated).
- Build: rc 0; GCC emitted only non-fatal notes (DataFrame C++17 ABI notes).
- Tests: **735/735 passed**, 0 failed, total test time 2.02 s.
- Evidence copied to
  `/Users/yunjinqi/Documents/new_projects/fincore-linux-evidence/`:
  `configure.log`, `build.log`, `ctest.log`, `environment.log`, `run.log`
  (`configure_rc=0`, `build_rc=0`, `ctest_rc=0`).

## Claim boundary

This is an Ubuntu 24.04 arm64 result obtained through Lima/vz, not a
GitHub-hosted runner. It exercises the same configure/build/test commands and
the same pinned DataFrame commit. Windows remains **NOT_RUN**, and no Linux
visual/ImPlot rendering is claimed.
