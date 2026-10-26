# Maintainer notes

## Comment rounds

1. Freeze the public symbol list with `tools/build_api_ledger.py`.
2. Edit one module's public headers; use `///` Google-style contracts and
   `///<` for concise fields/enum values. Do not change code or formatting.
3. Run `tools/check_comment_only.py` on the touched headers.
4. Regenerate Doxygen XML and the ledger; record documented/missing counts.
5. Run related tests (and the full suite for a release candidate).
6. Build both language sites with `-W -n --keep-going`.
7. Update the English page, `.po` translation, page ledger and source links;
   commit only the reviewed round.

## API pages

`tools/generate_api_pages.py` rebuilds `docs/site/api/<module>.md` from the
committed ledger: one Breathe directive per top-level declaration, overloaded
functions emitted with their exact signature, template functions by name, and
multi-line string constants listed in the script's `UNRENDERABLE` set. Edit the
ledger inputs, not the generated pages; the script is deterministic, so a
reviewed round shows up as a small diff.

Doxygen emits C++14 digit separators inside `<argsstring>`, which the Sphinx C++
domain cannot parse, so `tools/normalize_doxygen_xml.py` must run between
`doxygen` and `sphinx-build`. `.readthedocs.yaml` and the `docs` workflow do
this; a local build is:

```bash
cd docs/site
mkdir -p ../_build/doxygen   # Doxygen creates one level only
doxygen Doxyfile
python tools/normalize_doxygen_xml.py ../_build/doxygen/xml
python tools/build_api_ledger.py
python tools/generate_api_pages.py
```

## Translation

```bash
cd docs/site
sphinx-build -b gettext -W --keep-going . ../_build/gettext
sphinx-intl update -p ../_build/gettext -l zh_CN
# edit locale/zh_CN/LC_MESSAGES/*.po and clear fuzzy entries
FINCORE_DOCS_LANGUAGE=zh_CN sphinx-build -b html -W -n --keep-going . ../_build/html/zh_CN
```

`tools/apply_translations.py <catalog> <json>` fills a catalog from a
msgid-to-msgstr JSON mapping, which keeps bulk translations reviewable and
avoids hand-editing wrapping. Check the totals before committing:

```bash
msgfmt --statistics -o /dev/null locale/zh_CN/LC_MESSAGES/**/*.po
```

The untranslated set must stay exactly the reviewed identifier exemptions in
`docs/iterations/iteration-10-comments-bilingual-docs/exemptions.md`; any other
untranslated entry blocks the bilingual release.

Keep identifiers, commands, paths, formulas and code blocks literal. The
glossary uses returns=收益率, drawdown=回撤, volatility=波动率, horizon=预测期,
turnover=换手率, exposure=暴露 and annualization=年化.

Doxygen auto-links are disabled (`AUTOLINK_SUPPORT = NO`) because reference
nodes cannot survive gettext: a translated paragraph that drops them fails the
strict build. Write cross-references explicitly in narrative pages instead.

## Generated artifacts

`docs/_build/` (Doxygen XML, gettext templates, `.mo`, HTML) is ignored.
`docs/site/locale/**/LC_MESSAGES/*.po` and `api-ledger*.{tsv,json}` are source
artifacts and should be committed when they change.
