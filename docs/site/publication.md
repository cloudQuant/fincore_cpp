# Publication and platform status

This page records the deployment state; it is not a substitute for checking a
live site.

## GitHub Pages — live

Verified on 2026-10-26 against `cloudQuant/fincore_cpp`:

- Pages is enabled with `"build_type": "workflow"`. A branch source
  (`legacy`) breaks `deploy-pages`, so the source must stay "GitHub Actions";
  re-read `GET /repos/.../pages` after any UI change.
- The `github-pages` environment carries `custom_branch_policies: true` and
  allows `master` only. A deploy job on any other branch finishes **failed with
  zero steps and a 404 log**, which is the policy rejecting it rather than a
  build problem. The allow-list is UI-only with the token used here.
- `docs` workflow run
  [`40286395187`](https://github.com/cloudQuant/fincore_cpp/actions/runs/40286395187)
  was the first with `build`, `pages` and `deploy` all green; later runs on
  `master` repeated it. Publication therefore happens by merging into `master`.

Live URLs, each returning 200 on 2026-10-26:

- Landing: `/fincore_cpp/`
- English: `/fincore_cpp/en/latest/`
- Chinese: `/fincore_cpp/zh-cn/latest/`
- Deep pages checked in both languages: `installation.html`,
  `guides/metrics.html`, `api/metrics.html`, plus `conventions.html` in Chinese
- Static asset: `/fincore_cpp/en/latest/_static/css/theme.css`

The language directories are `en/latest/` and `zh-cn/latest/` — probing `en/` or
`zh_CN/` returns 404 and says nothing about the deployment. A Pages build keeps
the version in the path segment (`latest`) and has no Read the Docs style version
switcher.

## Read the Docs — one project, English build

The repository ships one `.readthedocs.yaml`, so Read the Docs hosts **one
project**, `fincore_cpp`, building the English source. No translation projects
are created: the Chinese documentation is the GitHub Pages build, and the
language switch in the top-right corner of every page sends Chinese readers
there while keeping the page path.

Owner steps, once an account is available:

1. Sign in at readthedocs.org with the account that can see
   `cloudQuant/fincore_cpp`.
2. Import the repository once, as project **`fincore_cpp`** (slug
   `fincore-cpp`), default branch `master`, language **English**. The build uses
   `.readthedocs.yaml` automatically: Ubuntu 24.04, Python 3.13, Doxygen from apt,
   the pinned Sphinx requirements, and `fail_on_warning: true`.
3. Leave translations empty. The Chinese site is already live on Pages, and the
   switch links the two hosts, so a second RTD project would duplicate it.
4. After the first build, check the home page, installation, one deep API page
   and the language switch on `https://fincore-cpp.readthedocs.io/en/latest/`.

Until that build answers 200, RTD is **platform-pending**, not PASS.

## Rule

A local HTML build is "deployable artifact"; a platform build without a live
page check is "online check pending"; neither is a publication PASS. If one
language or platform fails, the previously verified site must remain available
and the failed combination must be recorded.
