# Phase 38 Native Adjusted Sharpe Ratio Implementation Plan

**Goal:** Add the next bounded public ratio slice: native adjusted Sharpe ratio
with Python-compatible raw-count gating, default daily Sharpe behavior,
NaN-only biased moments, dampening, clamp boundaries, selector validation, and
nonfinite result classes.

**Architecture:** Add one scalar declaration to the existing ratios leaf and
one definition in the existing ratios target. Reuse native `sharpe_ratio` for
the base value. Add only a private NaN-only biased-moment helper in
`ratios.cpp`, because public native `skewness`/`kurtosis` have intentionally
different finite filtering and clean-count contracts. Reuse the two existing
registered test translation units; add no target, dependency, adapter, or
facade.

**Tech stack:** C++23, local cloudQuant/DataFrame, CMake, GoogleTest, a clean
detached Python source oracle run with the mandatory Anaconda interpreter,
AppleClang, Homebrew LLVM 22 ASan+UBSan, and compile/link-only local
ImGui/ImPlot evidence.

**Baseline:** Start from the clean committed Phase 37 head
`722528acdabd1102594a729a2e6478ef3fad39a5`. Preserve all earlier commits and
documents. Do not amend, rebase, merge, push, release, or create a Python
adapter.

## Global constraints

- Add exactly `double adjusted_sharpe_ratio(const dataframe&, double = 0.0,
  std::string_view = returns_column)` to `metrics/ratios.hpp`. Do not add
  `period`, annualization, overloads, aliases, automatic-input APIs, ndarray
  or Series APIs, result types, or facade includes.
- Reject embedded NUL in `column` before every DataFrame access with exactly
  `column name must not contain an embedded NUL`. Then call
  `require_double_column`, take the raw-size `< 4` gate, call native
  `sharpe_ratio(returns, risk_free, daily_annualization, column)`, and return
  early only for `std::isnan`. Passing `column` explicitly is required: the
  two-argument native overload path would silently select `returns_column`.
- Preserve native Sharpe's default daily annualization and its own NaN-aware
  cleaning/sample-standard-deviation behavior. Do not reimplement it or add a
  finite-result filter.
- Add a private `ratios.cpp` helper only. It must use existing `valid_values`
  (NaN only), calculate population `m2/m3/m4` in retained row order, use the
  `m2 <= (epsilon * mean)^2` zero-variance rule, calculate biased skew
  `m3 / (m2 * sqrt(m2))` and Fisher excess kurtosis `m4 / (m2 * m2) - 3`, and
  retain all nonfinite arithmetic. Do not call public `skewness`, public
  `kurtosis`, or `detail::finite_values`; do not change `stats.cpp`.
- Substitute zero for each NaN moment only after the non-NaN Sharpe gate.
  Use raw DataFrame count for `n`, dampening, and the 19/20 clamp selection.
  Express clamps as `std::max(lower, std::min(upper, adjustment))` exactly;
  do not use `std::clamp`, reorder operands, add a finite check, or normalize
  signed zero.
- Preserve input value order, duplicate/nonmonotonic index labels, bounded
  non-NUL-terminated selectors, structural-validation precedence, and scalar
  ownership. Do not sort, align, deduplicate, mutate inputs, or retain input
  storage.
- Every Python invocation must use
  `/Users/yunjinqi/opt/anaconda3/bin/conda run -n base python` (with
  `--no-capture-output` permitted for retained logs). Do not use the dirty
  current Python checkout as clean-source acceptance evidence.
- Keep phase evidence outside the repository under uniquely named
  `/tmp/fincore-phase38-*` roots. Do not create or use `.superpowers` paths or
  copy historical Superpowers workflow language.
- Do not add/alter CMake targets, DataFrame/ImGui/ImPlot checkouts,
  dependencies, providers, networks, reports, packages, plots, GUI runtime,
  calendar resampling, optimization, regression, EVT, MAR, conditional
  Sharpe, capture-return, Burke, Treynor, M-squared, Sterling, or other ratio
  slices.
- Before every commit below, define and use this exact-index gate in the same
  shell. It fails closed when pre-existing staged user work, an unexpected
  status, or an omitted expected path is present; retain its unique temporary
  directory as the staging-scope evidence.

  ```sh
  assert_exact_index() {
    phase38_index_root=$(mktemp -d /tmp/fincore-phase38-index.XXXXXX)
    printf '%s\n' "$phase38_index_root"
    printf '%s\n' "$@" | LC_ALL=C sort \
      >"$phase38_index_root/expected-name-status.txt"
    git diff --cached --name-status | \
      awk -F '\t' '{print $1 " " $2}' | LC_ALL=C sort \
      >"$phase38_index_root/actual-name-status.txt"
    diff -u "$phase38_index_root/expected-name-status.txt" \
      "$phase38_index_root/actual-name-status.txt"
  }
  ```

## Task 160: Record the design and executable plan

**Files:**

- Create: `docs/design/2026-09-20-phase-38-adjusted-sharpe-ratio-design.md`
- Create: `docs/plans/2026-09-20-fincore-cpp-phase-38-adjusted-sharpe-ratio.md`

1. Record the current-source route selection, Python export/operation route,
   dirty-checkout boundary, exact two-argument signature, no-`period` error,
   native API, control flow, private moment divergence from public stats,
   validation/order/ownership rules, full oracle, test matrix, exact source
   scope, exclusions, and acceptance limits.
2. Keep the two candidate files unstaged and obtain independent reviews in
   parallel: source-contract, C++ compatibility, test/mutation, build-gate,
   and plan-quality. Correct every Critical, Important, or Minor finding;
   re-review changed material rather than accepting an unreviewed fix.
3. Check and commit only these two paths without amending history:

   ```sh
   set -e
   git diff --check
   git add \
     docs/design/2026-09-20-phase-38-adjusted-sharpe-ratio-design.md \
     docs/plans/2026-09-20-fincore-cpp-phase-38-adjusted-sharpe-ratio.md
   assert_exact_index \
     'A docs/design/2026-09-20-phase-38-adjusted-sharpe-ratio-design.md' \
     'A docs/plans/2026-09-20-fincore-cpp-phase-38-adjusted-sharpe-ratio.md'
   git diff --cached --check
   git commit -m "docs: plan phase 38 adjusted sharpe ratio"
   ```

## Task 161: Add native adjusted Sharpe ratio

**Files:**

- Modify: `include/fincore/metrics/ratios.hpp`
- Modify: `src/metrics/ratios.cpp`
- Modify: `tests/unit/ratios_test.cpp`
- Modify: `tests/unit/metrics_umbrella_test.cpp`

### 161.1 Fresh clean Python source oracle

1. Make one uniquely named detached clean Python worktree at
   `2354bb560788f372502151a8b1b53accf536a6c0`, beneath a unique durable root,
   and capture pre-run tracked *and ignored* status. Do not use the existing
   dirty checkout or an installed `site-packages` package:

   ```sh
   python_oracle_root=$(mktemp -d /tmp/fincore-phase38-python.XXXXXX)
   python_source_root="$python_oracle_root/source"
   git -C /Users/yunjinqi/Documents/new_projects/fincore worktree add --detach \
     "$python_source_root" 2354bb560788f372502151a8b1b53accf536a6c0
   git -C "$python_source_root" rev-parse HEAD
   git -C "$python_source_root" status --ignored --short \
     >"$python_oracle_root/status-before.txt"
   test ! -s "$python_oracle_root/status-before.txt"
   ```

   Record the absolute root, commit, target-file hashes, Python and NumPy/SciPy
   versions, command exit status, stderr size, script hash, raw-output hash,
   pre/post status logs, and current import path.
2. Reproduce the design's complete oracle with the mandated interpreter and
   bytecode disabled. The oracle script must import `fincore`, print
   `fincore.__file__` and `sys.path`, and fail unless the resolved module path
   is under `Path(os.environ["FINCORE_PHASE38_PYTHON_SOURCE"]).resolve()`.
   The invocation must set that variable and `PYTHONPATH` to the detached root;
   redirect stdout/stderr separately and preserve the exit code:

   ```sh
   set +e
   FINCORE_PHASE38_PYTHON_SOURCE="$python_source_root" \
   PYTHONPATH="$python_source_root" PYTHONDONTWRITEBYTECODE=1 \
   /Users/yunjinqi/opt/anaconda3/bin/conda run --no-capture-output -n base \
     python "$python_oracle_root/oracle.py" \
     >"$python_oracle_root/oracle.raw.txt" \
     2>"$python_oracle_root/oracle.stderr.txt"
   oracle_exit=$?
   set -e
   test "$oracle_exit" -eq 0
   git -C "$python_source_root" status --ignored --short \
     >"$python_oracle_root/status-after.txt"
   diff -u "$python_oracle_root/status-before.txt" \
     "$python_oracle_root/status-after.txt"
   ```

   It
   must print the exact Python signature; the `period=` TypeError; ordinary
   and nonzero-risk-free goldens; raw `n=0..4`; raw-four with 0/1/2/3 clean
   values; all-NaN raw ten; upper/lower clamps at raw 19/20/49/50 with control
   fields; zero/positive/negative constant cases; signed-infinite returns;
   NaN/+infinity/-infinity risk-free; the nonconstant near-zero-variance
   fixture; raw-twenty/clean-two; and warning observations. Preserve raw text;
   warnings are not native assertions.
3. Require the classifications and ordinary finite values shown in the design.
   If a clean source oracle differs, stop and update the design before native
   implementation; do not silently tune C++ toward stale evidence.

### 161.2 Genuine dual-consumer RED

1. Before adding the production declaration or definition, add to both
   existing test translation units:

   - a `double (*)(const dataframe&, double, std::string_view)` exact
     `static_assert` using
     `std::is_same_v<decltype(&fincore::adjusted_sharpe_ratio), signature>`;
     do not use a `static_cast`, because the direct address expression must
     also fail if a forbidden overload is added; and
   - a default-column call on at least four finite values that has a finite
     expected result.

   `ratios_test.cpp` must continue to include only
   `<fincore/metrics/ratios.hpp>` among fincore headers. Do not stage these
   interim changes or run CTest after the expected failure.
2. Make Ninja continue across both consumers and retain a failed build log:

   ```zsh
   evidence_root=$(mktemp -d /tmp/fincore-phase38-red.XXXXXX)
   printf '%s\n' "$evidence_root"
   set +e
   cmake --build build --target fincore_tests --parallel 4 -- -k 0 \
     >"$evidence_root/task-161-red-build.log" 2>&1
   red_exit=$?
   set -e
   test "$red_exit" -ne 0
   rg -n 'ratios_test\.cpp' "$evidence_root/task-161-red-build.log"
   rg -n 'metrics_umbrella_test\.cpp' "$evidence_root/task-161-red-build.log"
   rg -n 'adjusted_sharpe_ratio' "$evidence_root/task-161-red-build.log"
   ```

   Record `red_exit` and prove both translation units were attempted. The only
   root cause may be the intentionally absent Phase 38 symbol.

### 161.3 Minimal implementation and test matrix

1. Add the declaration and definition after retaining the RED evidence. The
   definition must follow the design's literal order: NUL validation;
   `require_double_column`; raw `< 4`; native
   `sharpe_ratio(returns, risk_free, daily_annualization, column)`; only-NaN
   Sharpe early return; local NaN-only biased moments; independent NaN-to-zero
   moment fallbacks; raw-count dampening; exact nested clamp; direct scalar
   product.
2. Add mutation-resistant tests with these named concerns:

   - exact signature and default call in both leaf and umbrella consumers;
   - ordinary `2.179402814215353` and risk-free `0.001` result
     `1.637451311586237`;
   - raw sizes `0..4`, raw ten all NaN, and raw-four clean counts 0/1/2/3,
     including `-4.090878742872842` and `4.520724794501948`;
   - nonconstant near-zero variance
     `[1.0, nextafter(1.0, 2.0), 1.0, nextafter(1.0, 2.0)]`, asserting the
     source `8.755998418330334e16` and equality with base native Sharpe; this
     kills a mutant that recognizes only exactly-zero `m2` and clamps 1.1x;
   - upper/lower clamp values and multipliers for all four raw-size boundaries
     19, 20, 49, and 50;
   - raw twenty with eighteen NaNs followed by `[.01, -.02]`, asserting
     `-4.864154602806124` and multiplier `1.3`; this must use raw count rather
     than the two cleaned observations for both dampening and clamp family;
   - zero/positive/negative constants with exact NaN/+inf/-inf classes;
   - four-value return-infinity arrays `[+inf, .01, -.02, .03]`,
     `[-inf, .01, -.02, .03]`, and `[+inf, -inf, .01, -.02]`, plus
     NaN/+inf/-inf `risk_free` values, never a short input that can pass at the
     raw-size gate;
   - default/named/bounded selectors, where named and bounded selectors use a
     valid custom column on a frame that has no `returns` column; this must
     fail if the base Sharpe call loses its explicit `column`. Also cover
     duplicate/nonmonotonic index and the cancellation-order values
     `[-1e16, 1.0, 1e16, 1.0]` versus `[1.0, 1e16, 1.0, -1e16]`, asserting
     distinct results (approximately `4.860555523805895e-16` versus `0.0`),
     plus local input/selector lifetime; and
   - NUL-before-DataFrame precedence (valid prefix and malformed frame), empty
     and missing selector, wrong column type, and index/value mismatch.

   Use tolerances only for finite numerical reductions. Assert classifications
   and signs exactly. Include no public-stats regression or CMake change.
3. Run and retain actual current-tree evidence, substituting neither historical
   test counts nor focused output for full-suite evidence:

   ```sh
   cmake --build build --parallel 4
   ctest --test-dir build -N
   ctest --test-dir build -R 'Ratios|MetricsUmbrella' \
     --output-on-failure --parallel 4
   ctest --test-dir build -R \
     'Dataframe|Basic|Returns|Drawdown|Ratios|Stats|PerfStats|AnalysisContext|Rolling' \
     --output-on-failure --parallel 4
   ctest --test-dir build --output-on-failure --parallel 4
   git diff --check
   git diff --cached --check
   ```
4. Independently inspect the diff against the four-path allowlist. Stage and
   commit only those paths:

   ```sh
   set -e
   git add include/fincore/metrics/ratios.hpp \
     src/metrics/ratios.cpp \
     tests/unit/ratios_test.cpp \
     tests/unit/metrics_umbrella_test.cpp
   assert_exact_index \
     'M include/fincore/metrics/ratios.hpp' \
     'M src/metrics/ratios.cpp' \
     'M tests/unit/metrics_umbrella_test.cpp' \
     'M tests/unit/ratios_test.cpp'
   git diff --cached --check
   git commit -m "feat: add native adjusted sharpe ratio"
   ```
5. Use exactly one bounded source-writing worker for this task; in parallel,
   use up to four independent read-only reviewers for contract, C++ safety,
   tests, and gate evidence. The primary agent must inspect the committed diff
   and independently accept or reject it. Correct every Critical, Important,
   or Minor finding in new scoped commits and rerun affected gates; never amend
   accepted history.

## Task 162: Accept Phase 38

**Files:**

- Modify: `README.md`
- Create: `docs/acceptance/2026-09-20-phase-38-adjusted-sharpe-ratio-acceptance.md`

1. After the Task 161 implementation and corrective-review chain is fixed,
   record the narrow route/exclusions, source dirty-boundary and clean oracle,
   public export/operation route, exact native API/control flow, moment-helper
   rationale, raw/clean count distinction, clamp operand order, validation,
   selector/index/ownership behavior, all nonfinite classifications, genuine
   RED, implementation/review chain, and exact evidence limits.
2. From the committed candidate, collect separately rooted, fresh evidence:

   - AppleClang Debug headless configure/build/registration/focused/full CTest
     with a root-owned clean official GoogleTest FetchContent checkout, ImPlot
     and demo off, plus warning scan;
   - Homebrew LLVM 22 Debug ASan+UBSan headless configure/build/registration/
     strict-runtime full CTest, sanitizer diagnostic scan, compile/link flag
     anchors, runtime linkage observation, and UBSan symbol audit;
   - local DataFrame/ImGui/ImPlot Debug configure with tests, adapter, and demo
     build on, complete compile/link and CTest registration evidence, adapter
     exact-once/artifact/linkage/warning audits, and explicit proof the demo
     was never executed and no GUI was launched; and
   - a distinct absent-path invalid-ImGui configure that exits `1`, creates no
     build target/test/demo, and matches the complete expected diagnostic after
     physical-line-wrap normalization.

   Record actual commands, roots, source SHAs, tool versions, CMake/Ninja
   counts, test counts, warnings, FetchContent provenance, statuses, and what
   was deliberately not run. Do not reuse old roots, copied counts, or local
   site-packages as source/current-build proof.

   Use the following executable configure contracts rather than substituting a
   pre-existing `build/` cache. Each positive root starts from a newly detached
   copy of the final candidate and must record its own normal GoogleTest
   FetchContent checkout (`origin`, tag/commit, cleanliness, and cache entries
   for source/disconnected overrides).

   **AppleClang headless:**

   ```sh
   candidate_sha=$(git rev-parse HEAD)
   apple_root=$(mktemp -d /tmp/fincore-phase38-apple.XXXXXX)
   apple_source="$apple_root/source"
   apple_build="$apple_root/build"
   git worktree add --detach "$apple_source" "$candidate_sha"
   test -z "$(git -C "$apple_source" status --porcelain)"
   cmake -S "$apple_source" -B "$apple_build" -G Ninja \
     -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=/usr/bin/cc \
     -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
     -DFINCORE_DATAFRAME_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/DataFrame \
     -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF \
     -DFINCORE_BUILD_IMPLOT_DEMO=OFF -DFINCORE_ENABLE_ASAN=OFF \
     -DFINCORE_ENABLE_UBSAN=OFF -DFETCHCONTENT_QUIET=OFF
   cmake --build "$apple_build" --parallel 4
   ctest --test-dir "$apple_build" -N
   ctest --test-dir "$apple_build" -R 'Ratios|MetricsUmbrella' \
     --output-on-failure --parallel 4
   ctest --test-dir "$apple_build" --output-on-failure --parallel 4
   ```

   Audit its complete configure/build output for warnings and record actual
   zero/nonzero counts. Verify that `$apple_build/_deps/googletest-src` exists,
   has a clean Git worktree and official origin, and that no nonempty
   `FETCHCONTENT_SOURCE_DIR_GOOGLETEST`, fully-disconnected, or updates-
   disconnected cache override supplied the dependency.

   **Homebrew LLVM 22 ASan+UBSan headless:**

   ```sh
   llvm_root=$(mktemp -d /tmp/fincore-phase38-llvm.XXXXXX)
   llvm_source="$llvm_root/source"
   llvm_build="$llvm_root/build"
   git worktree add --detach "$llvm_source" "$candidate_sha"
   test -z "$(git -C "$llvm_source" status --porcelain)"
   cmake -S "$llvm_source" -B "$llvm_build" -G Ninja \
     -DCMAKE_BUILD_TYPE=Debug \
     -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm/bin/clang \
     -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
     -DFINCORE_DATAFRAME_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/DataFrame \
     -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=OFF \
     -DFINCORE_BUILD_IMPLOT_DEMO=OFF -DFINCORE_ENABLE_ASAN=ON \
     -DFINCORE_ENABLE_UBSAN=ON -DFETCHCONTENT_QUIET=OFF
   cmake --build "$llvm_build" --parallel 4
   ctest --test-dir "$llvm_build" -N
   ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:symbolize=1 \
   UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
   ASAN_SYMBOLIZER_PATH=/opt/homebrew/opt/llvm/bin/llvm-symbolizer \
   ctest --test-dir "$llvm_build" -R 'Ratios|MetricsUmbrella' \
     --output-on-failure --parallel 4
   ASAN_OPTIONS=detect_leaks=1:abort_on_error=1:symbolize=1 \
   UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
   ASAN_SYMBOLIZER_PATH=/opt/homebrew/opt/llvm/bin/llvm-symbolizer \
   ctest --test-dir "$llvm_build" --output-on-failure --parallel 4
   ```

   Apply the same FetchContent provenance checks. Retain strict-runtime logs;
   scan them for ASan/UBSan/LSan and `runtime error:` diagnostics; capture the
   `ratios.cpp` compile command and `fincore_tests` final-link command and
   count address/undefined sanitizer flags; then record `otool -L`, a focused
   `DYLD_PRINT_LIBRARIES=1` run, and `nm -a` evidence for UBSan handlers. A
   configured sanitizer flag without those compile/link/runtime checks is not
   sufficient evidence.

   **Local ImGui/ImPlot positive compile/link gate:**

   ```sh
   implot_root=$(mktemp -d /tmp/fincore-phase38-implot.XXXXXX)
   implot_source="$implot_root/source"
   implot_build="$implot_root/build"
   git worktree add --detach "$implot_source" "$candidate_sha"
   test -z "$(git -C "$implot_source" status --porcelain)"
   cmake -S "$implot_source" -B "$implot_build" -G Ninja \
     -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=/usr/bin/cc \
     -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
     -DFINCORE_DATAFRAME_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/DataFrame \
     -DFINCORE_IMGUI_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/imgui \
     -DFINCORE_IMPLOT_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/implot \
     -DFINCORE_BUILD_TESTING=ON -DFINCORE_BUILD_IMPLOT=ON \
     -DFINCORE_BUILD_IMPLOT_DEMO=ON -DFINCORE_ENABLE_ASAN=OFF \
     -DFINCORE_ENABLE_UBSAN=OFF -DFETCHCONTENT_QUIET=OFF
   cmake --build "$implot_build" --parallel 4
   ctest --test-dir "$implot_build" -N -V
   ctest --test-dir "$implot_build" --output-on-failure --parallel 4
   ```

   Record detached source and each local dependency's clean head; use verbose
   registration to show the adapter test exactly once; audit adapter/demo
   artifacts and their actual Ninja link commands/`otool -L` dependencies;
   scan warnings by family; and inspect processes after CTest. Do not invoke
   `$implot_build/fincore_implot_demo` or any demo target: demo execution and
   GUI launch are **NOT_RUN**, while compilation/linking the executable is
   required.

   **Invalid-ImGui expected-negative configure:**

   ```sh
   invalid_root=$(mktemp -d /tmp/fincore-phase38-invalid-imgui.XXXXXX)
   invalid_source="$invalid_root/source"
   invalid_build="$invalid_root/build"
   absent_imgui="$invalid_root/absent-imgui-source"
   git worktree add --detach "$invalid_source" "$candidate_sha"
   test ! -e "$absent_imgui"
   set +e
   cmake -S "$invalid_source" -B "$invalid_build" -G Ninja \
     -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=/usr/bin/cc \
     -DCMAKE_CXX_COMPILER=/usr/bin/c++ \
     -DFINCORE_BUILD_TESTING=OFF -DFINCORE_BUILD_IMPLOT=ON \
     -DFINCORE_BUILD_IMPLOT_DEMO=OFF -DFINCORE_ENABLE_ASAN=OFF \
     -DFINCORE_ENABLE_UBSAN=OFF \
     -DFINCORE_DATAFRAME_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/DataFrame \
     -DFINCORE_IMPLOT_SOURCE_DIR=/Users/yunjinqi/Documents/new_projects/implot \
     -DFINCORE_IMGUI_SOURCE_DIR="$absent_imgui" \
     >"$invalid_root/configure.log" 2>&1
   invalid_exit=$?
   set -e
   test "$invalid_exit" -eq 1
   test ! -e "$absent_imgui"
   ```

   Normalize physical line wraps in `configure.log` and require the full
   dynamic-path diagnostic stanza
   `FINCORE_IMGUI_SOURCE_DIR must name a Dear ImGui checkout with all required
   files; received '$absent_imgui'` plus its CMake call site. Record the
   normalized expected-versus-actual comparison. Confirm no Ninja/Makefile,
   test executable, ImPlot adapter, or demo artifact exists; build, CTest,
   demo, and GUI are **NOT_RUN** for this expected failure.
3. Keep the acceptance document and README unstaged for fresh independent
   document and gate reviews. Correct all Critical, Important, and Minor
   findings, rerun affected evidence/reviews, then stage and commit only:

   ```sh
   set -e
   git add README.md \
     docs/acceptance/2026-09-20-phase-38-adjusted-sharpe-ratio-acceptance.md
   assert_exact_index \
     'A docs/acceptance/2026-09-20-phase-38-adjusted-sharpe-ratio-acceptance.md' \
     'M README.md'
   git diff --cached --check
   git commit -m "docs: accept phase 38 adjusted sharpe ratio"
   ```

## Task 163: Add non-self-referential traceability

**Files:**

- Modify: `docs/acceptance/2026-09-20-phase-38-adjusted-sharpe-ratio-acceptance.md`

1. Record Task 161's complete implementation/correction chain and exact
   scopes, then Task 162's initial acceptance commit and exact scope. This is
   documentation-only; neither state nor predict Task 163's own SHA.
2. Run a current focused build/test and diff checks, then commit only the
   acceptance document without amending history:

   ```sh
   set -e
   cmake --build build --parallel 4
   ctest --test-dir build -R 'Ratios|MetricsUmbrella' \
     --output-on-failure --parallel 4
   git diff --check
   git add docs/acceptance/2026-09-20-phase-38-adjusted-sharpe-ratio-acceptance.md
   assert_exact_index \
     'M docs/acceptance/2026-09-20-phase-38-adjusted-sharpe-ratio-acceptance.md'
   git diff --cached --check
   git commit -m "docs: trace phase 38 adjusted sharpe ratio"
   ```

## Final Phase 38 review

Audit final ancestry and exact scopes; current Python source and clean-oracle
provenance; no-period Python boundary; native signature/defaults; NUL-before-
DataFrame ordering; raw versus cleaned sample gates; native-Sharpe reuse;
private biased-moment semantics and public-stats non-regression; independent
NaN fallbacks; raw-count dampening and 19/20/49/50 clamps; exact nested-clamp
operand ordering; nonfinite classifications; selector/index/ownership tests;
no CMake/dependency drift; and no-GUI/runtime wording.

Configure a unique fresh AppleClang headless root from the final committed
head, build fully, audit registration, run the focused `Ratios|MetricsUmbrella`
selection and full CTest, then reproduce the mandatory clean Python oracle.
Publish separate specification and task-quality verdicts. A `GO` can be only a
bounded Phase 38 acceptance verdict: it is not authorization to merge, push,
release, launch a GUI, invoke providers, or make live-service claims.
