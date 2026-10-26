# Contributing to code and documentation

## Before you start

Open an issue for anything larger than a bug fix or a documentation correction, so
scope is agreed before you spend time on it. Security reports go through
`SECURITY.md`, never a public issue. Small, focused pull requests are reviewed
faster than large ones.

## Code changes

Build and run the full suite, then add targeted tests for new behaviour:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Rules that reviewers check:

- Public signatures and contracts stay explicit; a new public declaration needs a
  comment, a Chinese translation and a refreshed API ledger in the same change.
- A numeric change comes with the fixture or oracle that justifies it. Do not
  weaken an assertion or loosen a tolerance to make a platform pass.
- A performance change states its workload, machine and statistic; a noisy result
  is recorded as INCONCLUSIVE rather than summarised upward.
- Optional surfaces (ImPlot, extensions) stay optional: no core target may start
  depending on a GUI library.

## Comment standard

Public declarations use `///` Google-style contracts: a verb phrase such as
"Returns ...", units, alignment and missing-value rules, ownership, and verified
failure modes. Implementation comments explain non-obvious algorithms and
invariants, not line-by-line mechanics. Do not invent complexity, thread-safety or
exception guarantees.

A comment-only change must be provably comment-only:

```bash
python docs/site/tools/check_comment_only.py <files>
```

## Documentation changes

The English source is authoritative. Generate the catalogs, translate the new
strings, then build both languages with strict warnings:

```bash
cd docs/site
doxygen Doxyfile
python tools/normalize_doxygen_xml.py ../_build/doxygen/xml
sphinx-build -b gettext -W --keep-going . ../_build/gettext
sphinx-intl update -p ../_build/gettext -l zh_CN
sphinx-build -b html -W -n --keep-going . ../_build/html/en
FINCORE_DOCS_LANGUAGE=zh_CN sphinx-build -b html -W -n --keep-going . ../_build/html/zh_CN
cd ../..
python docs/site/tools/check_built_site.py
```

Documentation rules that are enforced, not suggested:

- Every page has a Chinese entry in `locale/zh_CN/LC_MESSAGES/`; untranslated or
  fuzzy entries fail the site check.
- A heading must not start with a number (`## 1. ...`), or MyST leaves it
  untranslated even when the catalog has the string.
- Identifiers, commands, paths and formulas stay literal in the translation.
- Generated HTML, Doxygen XML, `.pot`/`.mo` files and benchmark outputs stay out
  of git.
- A page that describes a failure mode names the real message, not a paraphrase.

## Commits and pull requests

- One topic per commit: behaviour, documentation, and generated-artifact updates
  do not share a commit.
- Say what you ran and what you could not verify — platform, hardware, external
  service. A local build is not an online deployment.
- If a platform is unavailable, record it as pending with the exact flip path
  instead of claiming success.

## Review and evidence

Reviewers look for: the contract is stated where a reader will look for it; the
test would fail if the behaviour regressed; the documentation says what the code
does rather than what it was intended to do; and any number that appears in prose
can be reproduced from the repository.

Acceptance records under `docs/acceptance/` and iteration notes under
`docs/iterations/` are the project's memory. Update them when a change alters
what is true, and never rewrite history to look cleaner than it was.
