# Versions and release notes

## What version you are reading

The project tracks three version domains, and they are deliberately separate:

| Domain | Where it lives | Current value |
|---|---|---|
| Native library | `include/fincore/version.hpp` (`version_major`/`minor`/`patch`) | `0.1.0` |
| Python reference pin | `include/fincore/version.hpp` (`pinned_version_*`) | `0.5.2.dev0` |
| Documentation site | `docs/site/conf.py` (`version`/`release`) | `0.1.0` |

`fincore::version_string` is the native version constant. Note that
`fincore::version()` deliberately returns the **pinned Python** version, not the
native one — it exists so consumer metadata checks can compare against the frozen
oracle. If you want the library version at run time, read
`fincore::version_string`.

## Current status

`0.1.0` is in development and **no git tag has been cut yet**. `master` is the
published branch, and every push to it rebuilds the documentation site. That
means "latest" on the site and `master` in the repository are the same tree.

What is stable enough to build on:

- The public headers under `include/fincore`, documented symbol by symbol in the
  API ledger.
- The CMake package contract (`fincore::fincore`, optional `implot` component).
- The report document schema and the artifact bundle shape.
- Error categories and their structured fields.

What may still move without a deprecation cycle, because `0.x` makes no
compatibility promise:

- Internal `fincore::detail` helpers, which are outside the public surface.
- Optional extension and plotting adapters.
- Diagnostic field names inside `diagnostics` maps.

## Change log for this cycle

Recorded by area rather than by commit; the linked records carry the evidence.

**Library and correctness**

- Native C++23 surface across core data, metrics, risk, portfolio, factor,
  attribution, optimization, reporting, runtime and simulation modules.
- 1092 C++ tests, including oracle comparisons and consumer build checks.
- Documented failure model: structural problems throw, undefined statistics
  return `NaN`, and fits report status instead of a plausible number.

**Documentation**

- One bilingual site (English and Simplified Chinese) with a language switch on
  every page; the Chinese build is a full translation, not a stub.
- Every public symbol documented; the few reviewed exemptions (three
  `Subclassed by` lists, two multi-line string constants, functions without
  `@param` lists) are recorded in the iteration-10 exemptions file rather than
  hidden in the build.
- Tutorial, cookbook, task guides, migration mapping and troubleshooting built
  from the same headers the reference is generated from.

**Tooling and evidence**

- `check_comment_only.py` proves comment rounds change only comments.
- `check_built_site.py` checks both language builds for broken links, machine
  paths and untranslated catalogs, and refuses to run when one build is missing.
- `write_sitemap.py` derives the sitemap from the deployed artefact.
- Paired benchmark harness with digest-gated correctness and recorded
  INCONCLUSIVE outcomes.

See [publication](publication.md) for what is live, and the iteration records in
`docs/iterations/` for dated acceptance evidence.

## How a release will be cut

1. Update `version.hpp` and `docs/site/conf.py` together, then run the full gate
   (`ctest`, both strict Sphinx builds, the site checker).
2. Tag `v<version>` on the commit that passed, and push the tag.
3. Confirm the documentation site rebuilt from the tag's tree, and record the
   deployed URLs in the publication page.
4. Write the release notes from the iteration records, not from memory.
