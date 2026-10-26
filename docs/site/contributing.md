# Contributing to code and documentation

## Code changes

Build and run the full C++ suite, then add targeted tests for new behavior.
Keep public signatures and contracts explicit; update the English API comment,
the Chinese translation and the public symbol ledger in the same change.

## Comment standard

Public declarations use `///` Google-style contracts: a verb phrase such as
“Returns …”, units, alignment/missing-value rules, ownership, and verified
failure modes. Implementation comments explain non-obvious algorithms and
invariants, not line-by-line mechanics. Do not invent complexity, thread
safety or exception guarantees.

## Documentation changes

The English source is authoritative. Generate gettext catalogs, update Chinese
translations, and build both languages with strict warnings. The docs source is
under `docs/site`; generated HTML/XML and `.mo` files stay out of Git.

```bash
cd docs/site
doxygen Doxyfile
sphinx-build -b gettext -W --keep-going . ../_build/gettext
sphinx-intl update -p ../_build/gettext -l zh_CN
sphinx-build -b html -W -n --keep-going . ../_build/html/en
FINCORE_DOCS_LANGUAGE=zh_CN sphinx-build -b html -W -n --keep-going . ../_build/html/zh_CN
```

## Review and evidence

Keep behavior changes, documentation-only changes and generated-artifact
updates in separate commits. Record the actual commit, checks and exceptions;
do not cite a local build as an online deployment.
