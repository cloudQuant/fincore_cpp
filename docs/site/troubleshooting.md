# Troubleshooting

Each section below starts from what you see, then names the cause. If a symptom
is not covered, the message text and the frame shape are the two things that make
an issue actionable — include both.

## Configure or build fails

**`CMake 3.25 or higher is required`.** The project uses C++23 and modern CMake
features. Upgrade CMake rather than lowering the requirement.

**A `cxx_std_23` feature error from the compiler.** MSVC 19.4+, Clang 17+ or
GCC 13+ is required. On Windows, run CMake from a Visual Studio developer shell
so the compiler is discoverable.

**`add_subdirectory ... does not contain a CMakeLists.txt`.** The pinned
submodules are missing:

```bash
git submodule update --init --recursive
```

**A consumer-provided DataFrame is ignored.** Either point the build at it with
`-DFINCORE_DATAFRAME_SOURCE_DIR=...`, or make `find_package(DataFrame CONFIG)`
discoverable in `CMAKE_PREFIX_PATH`.

## Runtime errors

| Message | Cause and fix |
|---|---|
| `Invalid date values` | A calendar value outside the representable range, a bad month/day/hour, or an ISO string with a malformed field. The parser accepts at most nine fractional digits and rejects trailing text. |
| `column name must not contain an embedded NUL` | A `std::string_view` whose length includes a `'\0'`. Pass the actual name; the library refuses to truncate at the terminator. |
| `stress testing does not accept NaN returns` | Stress scenarios reject non-finite input instead of skipping it. Clean or drop the observations first. |
| `unknown viz renderer 'x'` | Renderer names are trimmed and ASCII-lowercased; register the name you actually look up. |
| `input_error` from a risk API | Most risk estimators require a strictly ascending, duplicate-free index. Sort and deduplicate before calling. |

## Numbers that surprise you

- **`NaN` instead of a value.** Usually too few usable observations, a window that
  is not complete, or a genuinely undefined statistic (zero deviation, no
  drawdown). Check `std::isnan` rather than assuming zero.
- **`calmar_ratio` returns `NaN`.** The series never declined, so the maximum
  drawdown is zero and the ratio is undefined — not infinite.
- **A backtest says `inconclusive`.** Fewer than three paired observations or
  fewer than five expected exceptions. 250 days at 99% expects 2.5 exceptions, so
  it cannot pass; that is a sample-size verdict, not a model verdict.
- **Drawdown signs disagree.** `max_drawdown` returns a negative fraction;
  `gen_drawdown_table` reports a positive percentage of the prior peak.
- **A Sharpe ratio looks too high.** Check the annualization: daily data uses
  252. A ratio computed with 365 differs by `sqrt(365 / 252)`.

## Documentation build fails

**Breathe reports missing compounds.** Doxygen has not run, or its XML is stale.
The order is `doxygen Doxyfile` → `normalize_doxygen_xml.py` →
`sphinx-build`. Running Sphinx first, or normalizing *before* regenerating, will
leave raw XML in place and the strict build then fails with two warnings.

**`AssertionError` inside `sphinx.domains.cpp`.** Stale doctrees: a reused
environment still holds C++ domain symbols from the previous XML. Build with
`-E` or delete the output directory.

**`check_built_site.py` refuses to run.** One language build is missing. Build
both (`FINCORE_DOCS_LANGUAGE=zh_CN` for the Chinese one); the checker now fails
instead of reporting a single-language result as green.

**A heading stays English in the Chinese build.** A section heading that starts
with a number (`## 1. ...`) is parsed by MyST in a way that leaves it untranslated
even when the catalog entry exists. Write plain headings.

**`fuzzy` entries after editing a page.** `sphinx-intl update` marks changed
source strings fuzzy. Translate them and clear the flag; a fuzzy entry counts as
untranslated and fails the site check.

## Publication problems

**The Pages deploy job fails with zero steps and no log.** The `github-pages`
environment is rejecting the branch. Its allow-list contains `master` only, so
publish by merging into `master`.

**`deploy-pages` fails while the build passed.** Pages source must be "GitHub
Actions" (`build_type: workflow`). A switch to a branch source breaks the Actions
deployment; re-read `GET /repos/.../pages` after touching the setting.

**Read the Docs shows only one language.** By design: one RTD project builds the
English source, and the language switch sends Chinese readers to the GitHub Pages
build of the same page.

## Reporting a problem

Include the commit, compiler and platform, the smallest frame shape that
reproduces the issue, the exact message, and whether the debug or release build
differs. For a documentation issue, include the page URL and the sentence you
expected.
