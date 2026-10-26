# Contributing

Thanks for taking the time to contribute. This repository is a C++23 library with
a bilingual documentation site; both are part of the same change surface.

## Before you start

- Open an issue for anything larger than a bug fix or a documentation correction,
  so the scope can be agreed before you spend time on it.
- Read [`docs/site/contributing.md`](docs/site/contributing.md) for the rules that
  apply to documentation changes.
- Security reports go through [`SECURITY.md`](SECURITY.md), never a public issue.

## Build and test

```bash
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The suite is 1092 cases; it is expected to pass in full before a pull request is
reviewed.

## Documentation changes

```bash
python -m venv .venv-docs
.venv-docs/bin/pip install -r docs/requirements-docs.txt
cd docs/site
doxygen Doxyfile
python tools/normalize_doxygen_xml.py ../_build/doxygen/xml
sphinx-build -b html -W -n --keep-going . ../_build/html/en
FINCORE_DOCS_LANGUAGE=zh_CN sphinx-build -b html -W -n --keep-going . ../_build/html/zh_CN
cd ../..
python docs/site/tools/check_built_site.py
```

Rules that are checked, not suggested:

- English is the source; every new or changed page needs its Chinese catalog
  entries in `docs/site/locale/zh_CN/LC_MESSAGES/`. Untranslated or fuzzy entries
  fail the site check.
- `tools/check_comment_only.py` proves a comment round changed only comments.
  Do not mix behaviour changes into a comment change.
- Public headers are the API reference. If you change a public declaration, run
  `tools/build_api_ledger.py` and regenerate the module pages.
- Generated HTML, XML, `.pot`/`.mo` files and benchmark outputs stay out of git.

## Pull requests

Keep a pull request to one topic. Include the commands you ran and their results,
and say what you could not verify (platform, hardware, external service). A
change that silently depends on unavailable infrastructure will be asked to
record that state instead of claiming success.

## Code of conduct

Participation is covered by [`CODE_OF_CONDUCT.md`](CODE_OF_CONDUCT.md).
