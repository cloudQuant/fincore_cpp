# Iteration 10 — Google-style comments and bilingual docs — Acceptance Record

**Date:** 2026-10-26 (Asia/Shanghai)
**Base:** `dev` at `8a1174fe5a08fd524f6728536fc0b34c55d59c27`
**Status:** **IN_PROGRESS** — P126–P132 pass on this host; P133 (platform
publication) is **PLATFORM_PENDING** and P134 (final close-out) is therefore
not claimed. No local build result is presented as a live-site PASS.

Plan: [README](../iterations/iteration-10-comments-bilingual-docs/README.md),
[requirements](../iterations/iteration-10-comments-bilingual-docs/requirements.md),
[design](../iterations/iteration-10-comments-bilingual-docs/design.md),
[acceptance gates](../iterations/iteration-10-comments-bilingual-docs/acceptance.md),
[reviewed exemptions](../iterations/iteration-10-comments-bilingual-docs/exemptions.md).

## 1. P126 — inventory and denominators

The public symbol ledger is generated from Doxygen XML by
`docs/site/tools/build_api_ledger.py` and committed as
[`api-ledger.tsv`](../../site/api-ledger.tsv) /
[`api-ledger-summary.json`](../../site/api-ledger-summary.json).

| Measure | Value |
|---|---:|
| Public `.hpp` files under `include/fincore` | 117 |
| Ledger rows (public symbols, overloads separate) | 2484 |
| Documented in this round | 2484 |
| Missing documentation | 0 |
| Functions with an explicit `@param` list | 341 |
| Functions with parameters but no `@param` list | 296 |
| Distinct public function names | 735 |
| Overloaded names (multiple signatures) | 50 |

`@param` lists are used where the signature alone does not fix units,
alignment, lifetime or failure modes, as required by
[requirements.md](../iterations/iteration-10-comments-bilingual-docs/requirements.md).
The 296 remaining rows are recorded per module with the sampled justification
in [exemptions.md](../iterations/iteration-10-comments-bilingual-docs/exemptions.md);
they are **not** counted as documented contracts.

Two ledger defects found while freezing the inventory were fixed before the
comment rounds were measured: symbols declared in the root `fincore` namespace
lost their namespace prefix, and overload sets collapsed to one row. Both
would have hidden real API from the denominator.

## 2. P127 — toolchain and bilingual pilot

Toolchain (pinned in
[`docs/requirements-docs.txt`](../../requirements-docs.txt), resolved set in
[`docs/requirements-docs.lock.txt`](../../requirements-docs.lock.txt)):
Doxygen 1.9.7, Sphinx 9.2.0, Breathe 5.1.0, MyST-Parser 5.1.0, sphinx-intl
2.4.0, sphinx-rtd-theme 3.1.0.

The pilot exercised functions, overloads, a class with members, value structs,
enums, typedefs, namespace-scoped types and free calendar functions through
Doxygen → Breathe → MyST → Sphinx, in both languages, with
`-W -n --keep-going`. Three toolchain limits were found and handled without
global suppression:

- Doxygen auto-links identifiers inside prose. gettext cannot carry those
  reference nodes into a translated message, so bilingual builds reported
  `i18n.inconsistent_references`. `AUTOLINK_SUPPORT = NO` is set in
  `docs/site/Doxyfile`; cross-references are written explicitly in the
  narrative pages instead.
- Doxygen copies C++14 digit separators into `<argsstring>`
  (`1 '000 '000.0`), which the Sphinx C++ domain cannot parse.
  `docs/site/tools/normalize_doxygen_xml.py` rewrites only generated argument
  strings, before Breathe reads them, and is wired into both platform builds.
- Sphinx does not interpret roles inside translated Breathe paragraphs, so
  identifiers in a `msgstr` must stay plain text. The three auto-generated
  `Subclassed by …` inheritance lists are consequently left in English as
  reviewed identifier exemptions.

## 3. P128–P131 — comment rounds

All 97 modified headers were verified with
`docs/site/tools/check_comment_only.py`, which strips comments while preserving
string literals, raw strings and C++14 digit separators, then compares the
remaining token stream against `HEAD`:

```
checked=97 failures=0
```

Three real token changes were caught by that check: a trailing comma added
after the last enumerator of `fee_treatment` and of `rolling_metric` (both
removed, keeping the enumerator comments), and a false positive in the checker
itself, which read the C++14 digit separator in `10'000.0` as the start of a
character literal and was fixed. Documentation-markup defects were also
corrected in place, with no behaviour change: Python roles (`:func:`,
`:class:`, `:struct:`) left inside C++ comments, a `100.` line start that
Doxygen reparsed as an ordered list, a Markdown-looking printf pattern that
Sphinx parsed as a link, and single backticks around values containing quotes.

Regression evidence for this round, from the same working tree:

```
cmake -S . -B build-win -DFINCORE_BUILD_TESTING=ON   (Ninja, MSVC 19.42, Release)
cmake --build build-win --parallel                   exit code 0
ctest --test-dir build-win -j 8
100% tests passed, 0 tests failed out of 1092
Total Test time (real) = 54.57 sec
```

Doxygen produced zero warnings (`docs/_build/doxygen/doxygen.log`).

`src`, `tests`, `tools`, `examples` and `benchmarks` are inventoried per file in
[`p131-inventory.tsv`](../iterations/iteration-10-comments-bilingual-docs/p131-inventory.tsv)
(287 files; 17 with three or more explanatory comment blocks, 24 sparse, 246
with no explanatory block). This round added implementation-invariant notes to
the four files where the code is least inferable from its contract —
`src/detail/mt19937.cpp` (NumPy seed multiplier and mask-based bounded draw),
`src/portfolio/round_trips.cpp` (signed-price FIFO matching and the
close-and-reopen remainder), `src/risk/garch.cpp` (why the parameters are
reparameterized before the unconstrained search) and
`src/factor/data.cpp` (bin interval convention and edge widening). Those four
were also verified comment-only. The remaining audit is deliberately deferred,
so **P131 is PARTIAL**, not passed.

## 4. P132 — bilingual site content

Generated pages, one per public module, produced by
`docs/site/tools/generate_api_pages.py` from the committed ledger: 13 module
pages plus the overview page, emitting 888 Breathe directives for top-level
declarations; member documentation is rendered through the owning class or
struct directive, and overloaded free functions are emitted with their exact
signature so Breathe resolves each alternative.

Strict builds on this host:

```
sphinx-build -E -b html -W -n --keep-going . ../_build/html/en        build succeeded
sphinx-build -E -b html -W -n --keep-going -D language=zh_CN . ../_build/html/zh-cn
                                                                       build succeeded
```

Translation state of the committed `zh_CN` catalogs:

| Measure | Value |
|---|---:|
| Catalogs | 28 |
| Message entries | 3232 |
| Translated | 3229 |
| Untranslated (reviewed identifier exemptions) | 3 |
| Fuzzy | 0 |
| HTML pages built per language | 30 |
| Broken internal links (`href`/`src` resolved on disk) | 0 |
| Absolute machine paths in built HTML | 0 |
| External `http(s)` links inside site Markdown | 0 |

Translations were applied with `docs/site/tools/apply_translations.py` from
per-module JSON mappings, and validated with `msgfmt --check` before each
build. Glossary terms follow
[`maintainer/README.md`](../../site/maintainer/README.md).

Language switching is rendered in the sidebar of every page. `sphinx_rtd_theme`
3.1.0 includes `searchbox.html` directly and ignores `html_sidebars`, so
`_templates/layout.html` injects the switcher into the theme's `menu` block;
`_static/language_switch.js` rewrites the two links from the current path so a
switch keeps the same page and version, and degrades to the other language's
home page when scripting is off. The static hrefs are derived from
`FINCORE_DOCS_BASEURL` / `READTHEDOCS_CANONICAL_URL`, so Pages and Read the
Docs each get their own prefix; a local build without either variable shows the
placeholder `https://docs.example.invalid/fincore_cpp/...`, which the script
still rewrites to the sibling directory when previewing the assembled site. Verified in the built output: the switcher and
the script are present in both languages (`grep fc-language-switch` on
`en/api/factor.html` and `zh-cn/api/factor.html`).

The full suite was re-run on the final tree: `100% tests passed, 0 tests failed
out of 1092`.

Generated output (`docs/_build/`, `.mo`) stays out of Git; the `.po` catalogs,
ledger and generated API pages are source artifacts and are committed.

## 5. P133 — publication: PLATFORM_PENDING

Configuration is in the repository and is buildable, but no live site was
verified, so this gate is not claimed.

Observed on the pushed `dev` branch (run
[`40246178068`](https://github.com/cloudQuant/fincore_cpp/actions/runs/40246178068)):
the **Linux** job installed Doxygen 1.9.8 and the pinned Python 3.13
dependencies, generated the XML, and completed **both** strict builds
(`-W -n --keep-going`, `fail_on_warning`) for English and Chinese. The run then
failed only at `actions/configure-pages`, because Pages is not enabled:
`Get Pages site failed … Not Found`. Two earlier configuration defects were
found and fixed by the same evidence: the docs pins required Python 3.13
(Sphinx 9.2.0 rejects 3.12), and Doxygen could not create `docs/_build/doxygen`
on a fresh checkout because it only creates one directory level.

The workflow now separates the two concerns: a `build` job that must pass and
uploads the assembled site as a normal artifact, and a `pages` job that probes
`GET /repos/.../pages` and only configures, uploads and deploys when Pages
exists. Run [`40246627229`](https://github.com/cloudQuant/fincore_cpp/actions/runs/40246627229)
on the restructured workflow is **success** — `build` passed, `pages` reported
the not-enabled notice, `deploy` was skipped. Until the owner enables Pages the
deliverable stays a verified **deployable artifact**, which is not a
publication PASS.

- [`.readthedocs.yaml`](../../../.readthedocs.yaml) — v2 config, Ubuntu 24.04,
  Python 3.13, Doxygen apt package, `pre_build` creates the output directory,
  runs Doxygen and the XML normalizer, `fail_on_warning: true`. **No RTD
  account or project is configured on this host**, so neither language project
  has been built or visited.
- [`.github/workflows/docs.yml`](../../../.github/workflows/docs.yml) — builds
  both languages with the same strict flags, assembles
  `/fincore_cpp/en/latest/` and `/fincore_cpp/zh-cn/latest/`, and gates Pages
  deployment on repository enablement. `has_pages` is `false` and
  `GET /repos/cloudQuant/fincore_cpp/pages` returns 404.

Required runbook before P133 can be marked PASS (four combinations, each with
home, installation, one deep API page, language switch, version label, static
assets and search checked from the same commit):

1. Push this iteration to `dev`, confirm the `docs` workflow run succeeds.
2. Repository owner enables Pages with "GitHub Actions" as the source (a
   shared-settings change that needs owner action; it was not performed here).
3. Create the two RTD projects (`en`, `zh_CN`) importing the same config and
   activate translations.
4. Record the four verified URLs and their commit; only then set P133 PASS.

Enabling Pages and creating RTD projects are administrator actions on shared
infrastructure, so they were deliberately left to the owner rather than
executed from this session. The deliverable at this point is a verified
**deployable artifact**, which is not a publication PASS.

## 6. Out-of-scope observation

The repository's business `ci` workflow fails on `ubuntu-latest` with
`'constexpr' evaluation operation count exceeds limit of 33554432` inside
`/usr/include/c++/13/bits/char_traits.h` (GCC 13 constexpr budget). The same
job failed identically on the two pushes before this iteration started
(runs `40198219958` and older), while `windows-latest` and `macos-latest` pass.
This is a pre-existing Linux toolchain gate, not documentation work, and no
comment round changed a non-comment token, so it is recorded here rather than
fixed inside iteration 10.

## 7. Open items

| ID | Item | Status |
|---|---|---|
| P131 | Per-file implementation/test/tool comment audit | PARTIAL |
| P133 | RTD ×2 and Pages ×2 live checks | PLATFORM_PENDING |
| P134 | Iteration close-out | NOT_RUN, blocked by P133 |
| E-01 | 3 `Subclassed by` identifier lists left in English | reviewed exemption |
| E-02 | 2 multi-line string constants not rendered as declarations | reviewed exemption |
| E-03 | 296 functions without `@param` lists | reviewed exemption, per module |
| E-04 | 17 precise `nitpick_ignore` entries | reviewed exemption |
