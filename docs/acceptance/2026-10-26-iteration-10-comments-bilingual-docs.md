# Iteration 10 — Google-style comments and bilingual docs — Acceptance Record

**Date:** 2026-10-26 (Asia/Shanghai)
**Base:** `dev` at `8a1174fe5a08fd524f6728536fc0b34c55d59c27`
**Status:** **IN_PROGRESS** — P126–P132 pass on this host and P134's local gate
is complete (§7); P133 (platform publication) is **PLATFORM_PENDING** so P134 is
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
(287 files: 106 marked `reviewed_this_round` with a one-line note naming the
invariant recorded or why none was needed, 12 with several explanatory blocks
but not yet audited, 13 sparse, 219 with no explanatory block). Auditing may
legitimately end in "no comment to add": `src/risk/specs.cpp` is such a case,
because its validation clauses are the entire contract. This round audited and,
where warranted, extended the following implementation files:

| File | Invariant recorded |
|---|---|
| `src/detail/mt19937.cpp` | NumPy's seed multiplier; mask-plus-rejection bounded draw |
| `src/portfolio/round_trips.cpp` | Signed-price FIFO rule; block may close and reopen |
| `src/risk/garch.cpp` | Why parameters are reparameterized before the unconstrained search |
| `src/factor/data.cpp` | Bin interval convention and why outer edges are widened |
| `src/factor/performance.cpp` | NaN-safe variance guard; rank-then-Pearson; membership ignores non-finite factor values |
| `src/optimization/portfolio_solver.cpp` | Gradient-scale normalization; feasibility restored by projection; Armijo test |
| `src/risk/evt.cpp` | Automatic threshold steps one representable value below the percentile because exceedances are strict |
| `src/simulation/bootstrap.cpp` | Rejection threshold removes modulo bias; one continuous stream defines reproducibility |
| `src/portfolio/transactions.cpp` | First-day gross book is halved when no previous book exists |
| `src/data/snapshots.cpp` | Closed supported-type list; column order is part of the digest |
| `src/risk/backtesting.cpp` | Log-space binomial mass with lgamma/log1p; CDF summed from the mode outward |
| `src/factor/serialization.cpp` | Length-prefixed big-endian encoding, quiet-NaN normalization, sorted map order, versioned tag |
| `src/metrics/drawdown.cpp` | NaN freezes wealth; matching the peak closes the episode and advances its timestamp |
| `src/risk/walk_forward.cpp` | Newton-refined inverse normal; type-6 quantile; NUL-terminated digest fields |
| `src/optimization/numerics.cpp` | Corner cases bypassed; fixed-iteration bisection with long-double sums |
| `src/simulation/monte_carlo.cpp` | 252 must be shared by calibration and horizon scaling |
| `src/calendar.cpp` | Boundary-day windows because the epoch is not a whole number of days |
| `src/dataframe.cpp` | No NaN padding; type-restricted lookup reports a wrong-typed column as such |
| `src/metrics/rolling.cpp` | Right-aligned labels, no short-window padding, windows rebuilt per step |
| `src/metrics/returns.cpp` | NaN periods compound flat; a wiped-out series reports -1.0 |
| `src/risk/models.cpp` | Digest pairs timestamp with value; ES tail is inclusive and degrades to the threshold |
| `src/simulation/scenarios.cpp` | crash/spike shock the upper middle element; only NaN input is rejected |
| `src/factor/pit.cpp` | Revision uniqueness key; `effective_from` then `known_at` then `as_of` tie-break |
| `src/optimization/risk_parity.cpp` | Positive weight floor; degenerate portfolios get a large finite penalty, not NaN |
| `src/performance/cashflows.cpp` | Flows bucketed by valuation date; a spurious -100% period is rejected |
| `src/risk/specs.cpp` | Audited with no change: the validation clauses are the whole contract |
| `src/factor/costs.cpp` | Opening book charged as a trade; capacity is the tightest single-asset limit; ties take the earliest date then first asset |
| `src/optimization/frontier.cpp` | Tolerance-scaled PSD test; maximum-return endpoint is a box corner, not a solve; interior points warm-start; diagnostics mirror the constraints actually solved |
| `src/optimization/objectives.cpp` | `min_weight` overrides `short_allowed`; unmapped sectors dropped; feasibility probes drop the ceiling they test; simplex branch cannot reach a negative bound |
| `src/metrics/ratios.cpp` | NaN-only and finite-only filters differ; flat-series guard scaled by magnitude; strict benchmark sign split excludes zero days; degenerate third-moment downside reports infinity |
| `src/simulation/paths.cpp` | Modulo-bias rejection threshold; Latin-hypercube resamples until the stratum survives rounding; seed price row dropped from the return frame |
| `src/performance/returns.cpp` | Floor day division before the epoch; bracket ends exclude the rate of -1; scaled NPV carries an anchor factor; sign changes read the sign bit; zero-net days dropped |
| `src/factor/calendar.cpp` | Sorted disjoint digit ranges probed by their end; strict UTF-8 rejection; every digit run needs a unit; backshift counts levels rather than rows |
| `src/factor/timedelta.cpp` | Monday-first weekday offset; an off-session roll consumes one step; half-open business count; a gap in the data becomes a holiday; sub-day remainder stays floored |
| `src/metrics/helpers.cpp` | NaT sentinel rejected by both index validators; uniqueness on a sorted copy versus stored order; calendar labels land on month ends from January 1970; column spans alias the frame; flatten is row-major |
| `src/rolling_engine.cpp` | Beta's two moments share one divisor so the ratio is scale-free; a flat or NaN benchmark is caught by a negated comparison |
| `src/viz/chart_data.cpp` | Sorted means duplicates allowed; one non-finite print invalidates a month; the two year-row passes must match; histogram bins are half-open with the maximum folded in |
| `src/data/providers.cpp` | Only the unadorned UTC form parses; batch symbols sorted so map order cannot leak; a lenient batch still keys failures; `to_returns` drops steps and loses positional alignment |
| `src/attribution/brinson.cpp` | BHB allocation keeps the three terms summing to the excess; cumulative effects add while wealth compounds; sector returns average while sector weights add; an omitted benchmark zeroes selection |
| `src/attribution/fama_french.cpp` | Singularity test reads the normal equations rather than the design; the variance shift is translation-invariant and kept for cancellation; beta is the MKT coefficient with the intercept at index zero; a negative idiosyncratic residual clamps to zero |
| `src/factor/portfolio.cpp` | Period validated before any row is read; floored day normalization; Sunday-first weekday test where the trading calendar is Monday-first; a non-finite cell adds no growth; the first cumulative row is after one period |
| `src/report/risk.cpp` | Non-finite metrics and cells carry no value instead of NaN text; the refit column set follows the fitted distribution, so headers rather than positions are the contract |
| `src/viz/html_builder.cpp` | Escaping matches the reference down to the apostrophe form and covers every interpolating site; a value that will not fit the buffer prints as the NaN marker; only a finite non-zero figure earns a colour class |
| `src/viz/render_model.cpp` | Renderer-name normalization is ASCII-only but shared by registration and lookup; the model is copied before a renderer runs, so a callback cannot disturb its callers |
| `src/data/contracts.cpp` | Validation binds to construction, not to the public fields; remaining attempts clamps at zero so an overrun reads as "none left" |
| `src/analysis_context.cpp` | Mutable caches reached from const calls make the context single-threaded by construction; counts record computations, not requests; one error covers an unknown key and an unservable metric |
| `src/metrics/alpha_beta.cpp` | A zero benchmark is down-side here, pinned by `IncludesZeroBenchmarkInDownSelection`, where the ratio helpers exclude it from both halves; NaN-only and finite-only pairing filters coexist by intent |
| `src/metrics/timing.cpp` | The three models differ only in the third design column and Cornell turns collinear without a market crossing zero; rejections are relative to the matrix scale; index two is the reported timing premium |
| `src/metrics/consecutive.cpp` | A flat or NaN day ends both runs; ties keep the earliest dates; a run totals simple returns rather than compounding |
| `src/metrics/perf_stats.cpp` | Bootstrap samples carry draw positions rather than dates, so order-sensitive statistics describe the synthetic sequence; Omega's pinned annualization is harmless only because its required return is zero |
| `src/metrics/yearly.cpp` | A year's row is labelled at its January while the metric sees only that year; a partial final year is unmarked and empty years are absent |
| `src/attribution/factor_regression.cpp` | `alpha` is reserved so inputs cannot collide with the output; Householder QR avoids squaring the design's condition number; an exactly constant series reports R-squared as zero |
| `src/report/models.cpp` | A metric's JSON kind follows number/boolean/text precedence rather than validation, and every branch emits the same five keys |
| `src/simulation/result.cpp` | An unbounded statistic raises here rather than returning NaN, with a single path's dispersion as the deliberate exception; the seven percentile cutoffs are fixed |
| `src/factor/tears.cpp` | Chart series drop non-finite points instead of leaving holes; a mixed-lag turnover input renders only its first lag |
| `src/attribution/performance.cpp` | Uniqueness compares adjacency, so a sorted index is an unstated premise registered as F10-D11; an asset without loadings is dropped silently while a total absence errors |
| `src/extensions/discovery.cpp` | Manifest JSON emits keys in fixed alphabetical order so a fingerprint survives member reordering; an entry-point value allows exactly one separator |
| `src/attribution/factor_prediction.cpp` | The coefficient table is read positionally, so its shape, index, ordering and labels are re-derived rather than trusted |
| `src/factor/workflows.cpp` | The seven workflows share one operation id and builder and differ only in exposed columns; the id list decides whether an operation is discoverable at all |
| `src/attribution/factor_attribution.cpp` | Finiteness is re-checked after each addition so an overflow names the statistic it fed; decomposition frames carry row positions rather than dates to allow concatenation |
| `src/attribution/factor_exposures.cpp` | Selector rules are re-declared per module rather than shared, so a change must be mirrored across regression, prediction and decomposition |
| `src/operations.cpp` | The catalog appends domain tables in a fixed, non-alphabetical order, so enumerations and digests depend on that sequence |
| `src/runtime/utilities.cpp` | The short-path accumulator seeds at negative zero to preserve signed-zero results; a zero shape dimension skips the overflow test |
| `src/report/xlsx.cpp` | Column names are bijective base twenty-six; malformed bytes are substituted one at a time while a legal but XML-disallowed code point is replaced whole |
| `src/report/portfolio.cpp` | The cumulative curve reads a missing return as no growth while the drawdown curve lets it through; `align_positions` repeats the adjacency uniqueness pattern registered as F10-D11 |
| `src/attribution/style.cpp` | Exposures clamp at zero before weighting; the volatility window grows until it reaches the cap; Welford's update keeps a population divisor; a zero contribution means nothing to attribute, not no effect |
| `src/metrics/bayesian.cpp` | Cone percentiles are cross-sectional over the paths at each step; a zero starting value is read as excess rather than level; a bound name normalizes negative zero so one width cannot have two identities |
| `src/metrics/stats.cpp` | A standard deviation below the floor means no dispersion rather than a small one; central moments are population form in one pass; Hurst lags turn logarithmic above thirty periods and deduplicate |
| `src/metrics/risk.cpp` | The quantile sorts a by-value copy so the caller's column keeps its order; one non-finite observation withdraws the whole risk figure instead of being dropped |
| `src/metrics/perf_attrib.cpp` | The cash column is type-checked yet excluded from attribution, and may be absent entirely |
| `src/portfolio/positions.cpp` | Row denominators skip missing positions, so an all-missing row's infinite ratio folds back to unavailable; cash is required here and optional in the attribution list |
| `src/factor/report.cpp` | Report moments drop infinities as well as NaN and divide by the usable count; skew and kurtosis are the biased population forms matching the reference call |
| `src/extensions/snapshot.cpp` | An extension identifier needs two dots so it cannot claim the namespace root; whitespace-only fields count as absent, and the two governance options must arrive together |
| `src/risk/report.cpp` | Comparison is relative with an absolute floor so an expected zero is still decidable; counts, status and digests are identity-compared while only derived statistics get tolerance |
| `src/errors.cpp` | Error payloads are UTF-8 JSON, escaping only control characters, unlike the ASCII-escaped snapshot form; reserved names overwrite free-form fields except an empty code |
| `src/metrics/basic.cpp` | An explicit annualization is returned untouched, zero or negative included; validation is the caller's |
| `src/metrics/frequencies.cpp` | Aliases use the period-end spellings ME/QE/YE so a converted frequency names the same boundary |
| `src/performance/inference.cpp` | A flat series reports a zero Sharpe rather than an undefined one, which the standard-error form reads as no risk adjustment |
| `src/detail/digest.cpp` | The rotation helper is never called with a zero distance, which would shift by the full width |
| `src/performance/disclosures.cpp` | Every disclosure line is emitted, with `unspecified` for missing text and `none` for a missing benchmark |
| `src/runtime/serialization.cpp` | Floats ride as lowercase IEEE bit patterns so NaN payloads and signed zeros survive; any other hex spelling is refused; integers are strings so they stay beyond the 53-bit mantissa |
| `src/simulation/correlation.cpp` | The target correlation need only be finite, so an infeasible constant matrix is written out rather than clamped |
| `src/simulation/parameters.cpp` | An unrecognized period annualizes as daily and the scaling is the volatility square root; NaN is skipped while an infinity stops the estimation |
| `src/simulation/base.cpp` | NaN and infinity are rejected with distinct messages rather than skipped as calibration would; a wrongly typed column of the right name reads as missing |
| `src/portfolio/capacity.cpp` | The trimmed window narrows the per-ticker and transaction views only, never the headline days-to-liquidate figure |
| `src/version.cpp` | The version string is a compiled-in literal, so a binary reports the version it was built from |
| `src/optimization/portfolio_solver.hpp` | Value and gradient are trusted as one function described twice, and feasibility is only what the projection enforced, which is why validation follows |
| `src/attribution/fama_french_patch.cpp` | Audited with no change: the static_assert already names the alias's purpose |
| `src/risk/garch_internal.hpp` | Audited with no change: the header already declares itself source-tree-only, outside the installed API |
| `src/factor/analysis.cpp` | The civil-date arithmetic shifts the year to March to avoid a month table; quantile statistics bucket finite values only and use a cross-quantile denominator |
| `src/runtime/runtime.cpp` | Runtime digests length-prefix every field and tag each value with its declared type, so neither separators nor a bool-versus-one collision can alias two payloads |
| `src/report/html.cpp` | Three escapers use three apostrophe spellings to match their own output format; both languages ride in one document and switch through a body class |
| `src/runtime/operations_portfolio.cpp` | The eight-hour pairing window is a raw duplicate of the public default, so the two have to move together |
| `src/runtime/operations_metrics.cpp` | Absent and mis-typed optional inputs are distinct outcomes, so a wrong type is never silently defaulted |
| `src/report/renderers.cpp` | A non-finite axis coordinate is written as null in place so the arrays stay index-aligned, unlike the chart builders that drop the point |
| `src/factor/research_export_internal.hpp` | The writer seam lets tests capture rows without touching the filesystem; the public export is the same path with the real writer |
| `src/report/pdf.cpp` | The margin constants are page edges rather than sizes, so the text box sits asymmetrically on the page |
| `src/factor/research_detailed_internal.hpp` | Audited with no change: the sink seam, panel row order and stability path are already stated |
| `src/viz/plotly_document.cpp` | SVG geometry collapses a non-finite coordinate to zero while the JSON channels write null, since a path cannot hold a gap and a trace can |
| `src/viz/bokeh_document.cpp` | The shared helpers are duplicated per backend and must stay identical, because both are compared against the same reference |
| `src/viz/implot_backend.cpp` | The axis runs in days and the tick formatter rounds back to nanoseconds, so a value an ulp under a whole day cannot label the previous date |
| `src/factor/artifacts.cpp` | Artifact bytes pin the classic locale and rewrite negative zero because they are compared and hashed |
| `src/runtime/operations_factor.cpp` | Audited with no change: the banner already states that typed lookups fail rather than coerce |
| `src/factor/research.cpp` | `ranks` requires a finite vector and averages ties; a cross-section is one day of finite values, and a spreadless day preprocesses to zeros |
| `src/factor/research_detailed_export.cpp` | The CSV and file counts are one contract with the manifest included; only shortest-form UTF-8 reaches the writer because the bytes are digested |
| `src/factor/research_detailed.cpp` | Ranks number only the non-NaN entries, so a value's rank depends on how many were skipped, and infinities rank as extremes |

Every edit above was verified comment-only and is covered by the re-run below.
Every one of the 106 files under `src/` is now audited and carries a note naming
the invariant recorded or why none was needed. The remaining 181 files are under
`tests/`, `tools/`, `examples/` and `benchmarks/`, and are deliberately not
claimed as audited, so **P131 is PARTIAL**, not passed.

One audit ended in a registered question rather than a comment: the
sector-free `min_variance` branch solves over the unit simplex, so a lowered
weight bound cannot be reached there, and the existing cases do not distinguish
the two because their unconstrained optimum is interior. No behaviour was
changed; **F10-D10** in
[`review-findings.md`](../iterations/iteration-10-comments-bilingual-docs/review-findings.md)
records the observation, and the `short_allowed` header note now states which
combination returns a long-only book.

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
| Message entries | 3233 |
| Translated | 3230 |
| Untranslated (reviewed identifier exemptions) | 3 |
| Fuzzy | 0 |
| HTML pages built per language | 30 |
| Broken internal links (`href`/`src` resolved on disk) | 0 |
| Absolute machine paths in built HTML | 0 |
| External hyperlinks in Markdown link syntax | 0 |

Translations were applied with `docs/site/tools/apply_translations.py` from
per-module JSON mappings, and validated with `msgfmt --check` before each
build. Glossary terms follow
[`maintainer/README.md`](../../site/maintainer/README.md).

The header note that F10-D10 describes added one msgid, which was translated in
the same round, so the entry count moved from 3232 to 3233 and the three
untranslated rows remain the reviewed `Subclassed by` exemptions. The
external-link measure counts Markdown link syntax only; the one `http` URL in
the site's Markdown is the `git clone` address inside a fenced block in
`installation.md`, which the link checker does not follow.

One operational limit surfaced while rebuilding: an incremental Chinese build
over doctrees left from a previous Doxygen XML aborted inside
`sphinx.domains.cpp` (`assert parent_symbol`) while resolving a stale
cross-reference, and it passed with `-E`. CI is unaffected because each run
builds fresh; `docs/site/README.md` now documents `-E` plus the missing
`normalize_doxygen_xml.py` step.

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

Every count above comes from the pushed tree: the 2484 ledger symbols, 888
generated directives, 60 built pages and 28 catalogs were measured after the
last edit. `builtin_catalog()` was also re-measured on this commit
(`operations=270`), so the native usage guide's registration claim is current
rather than inherited from iteration 8.

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
publication PASS. Later comment rounds kept it green: run
[`40251622323`](https://github.com/cloudQuant/fincore_cpp/actions/runs/40251622323),
pushed with the `short_allowed` note and the enlarged 3233-entry catalog, is
likewise **success**.

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

### Update after the owner enabled Pages (2026-10-26)

Pages is now enabled on the repository with `build_type: workflow` and
`html_url` `https://cloudquant.github.io/fincore_cpp/`. The consequence in CI is
visible on run [`40253764660`](https://github.com/cloudQuant/fincore_cpp/actions/runs/40253764660):
`build` **success**, `pages` **success** (the precheck now finds the site and
`configure-pages` accepts it), `deploy` **failure**. The deploy annotation is
`Branch "dev" is not allowed to deploy to github-pages due to environment
protection rules`, and both live URLs still answer 404 because nothing has been
deployed yet.

The remaining blocker is the auto-created `github-pages` environment: it carries
a `branch_policy` protection rule with `custom_branch_policies: true` and an
empty allow-list, so no branch can deploy. The allow-list could not be set from
this host: the environment `PUT` accepts neither `deployer_policies` nor
`deployable_branches` or `protection_rules`, the branch-list endpoints return
404, and the GraphQL schema exposes no mutation for it, so the only remaining
paths are the Settings UI or giving `dev` branch protection and switching to
`protected_branches: true`, which changes push governance and was not done.

A consequence worth knowing: because the deploy step now runs instead of being
skipped, the `docs` workflow reports **failure** on every `dev` push until the
allow-list is filled (runs `40254185674` and `40254528959`). The failure is left
visible rather than suppressed by another precheck, since a green workflow would
be a weaker signal than a 404 on the live URL.

**Owner step left for P133:** Settings → Environments → `github-pages` →
Deployment branches → add `dev` (and `master` if wanted), then re-run the `docs`
workflow on `dev`. Until that is done, both Pages combinations stay
**PLATFORM_PENDING**, and no local artifact is presented as a live site.

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
Checked at the job level on run [`40250234684`](https://github.com/cloudQuant/fincore_cpp/actions/runs/40250234684):
`windows-latest` **success**, `macos-latest` **success**, `ubuntu-latest`
**failure**. This is a pre-existing Linux toolchain gate, not documentation work,
and no comment round changed a non-comment token, so it is recorded here rather
than fixed inside iteration 10. The `docs` workflow, which is the one this
iteration owns, is green on the pushes carrying its comment rounds (runs
`40252622142`, `40252994147` and `40253152898`).

## 7. P134 — close-out verification on the final tree

The whole gate was re-run end to end after the last comment round, in the order
the site README documents:

| Step | Result |
|---|---|
| `doxygen Doxyfile` + `normalize_doxygen_xml.py` | 0 warnings (empty `doxygen.log`), 1 XML file normalized |
| `build_api_ledger.py` | 2484 symbols, 2484 documented, 0 missing, 296 `@param` exemptions |
| `generate_api_pages.py` | 13 module pages unchanged apart from two declaration line numbers shifted by the `short_allowed` note |
| `sphinx-build -b gettext` + `sphinx-intl update` | 3233 entries, 3230 translated, 3 exemptions, 0 fuzzy |
| Strict HTML builds `-E -W -n --keep-going` | English, `zh_CN` and `zh-cn` all succeeded |
| `docs/site/tools/check_built_site.py` | `pages=60 broken_links=0 machine_path_pages=0 fuzzy=0`, exit code 0 |
| CMake Release + `ctest` | `100% tests passed, 0 tests failed out of 1092` |

The gate above ran on `385db07` (forty-fourth audited file) and has been re-run
after every subsequent comment batch through `2e738e7`, the seventy-fifth: each
`ctest` run reported `100% tests passed, 0 tests failed out of 1092`. The docs
artifacts stayed current throughout because no public header changed after the
`short_allowed` note, which is the only comment-round edit that reaches Doxygen.

`check_built_site.py` is new in this round: the link, machine-path and catalog
measures used to be ad hoc queries, and committing them makes the numbers above
reproducible by anyone who rebuilds the site.

P134 is therefore **PARTIAL**: every local close-out item is complete and
re-measurable, but the phase's gate also requires the four live site/language
checks, which belong to P133 and cannot be substituted by a local build.

## 8. Open items

| ID | Item | Status |
|---|---|---|
| P131 | Per-file implementation/test/tool comment audit | PARTIAL |
| P133 | RTD ×2 and Pages ×2 live checks | PLATFORM_PENDING |
| P134 | Iteration close-out | PARTIAL — local gate complete, live checks owed by P133 |
| E-01 | 3 `Subclassed by` identifier lists left in English | reviewed exemption |
| E-02 | 2 multi-line string constants not rendered as declarations | reviewed exemption |
| E-03 | 296 functions without `@param` lists | reviewed exemption, per module |
| E-04 | 17 precise `nitpick_ignore` entries | reviewed exemption |
