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

## Read the Docs — live, one project

Read the Docs hosts **one project**, `fincore_cpp` (slug `fincore-cpp`), building
the English source from `master` with `.readthedocs.yaml`. No translation
projects are created: the Chinese documentation is the GitHub Pages build, and
the language switch in the top-right corner sends Chinese readers there at the
same page.

Verified live on 2026-10-26 (build 35413440, 84 s, commit `da90c0d`):

| URL | Status | Bytes |
|---|---|---|
| `/en/latest/` | 200 | 51,969 |
| `/en/latest/installation.html` | 200 | 40,013 |
| `/en/latest/api/metrics.html` | 200 | 674,060 |
| `/en/latest/_static/css/theme.css` | 200 | 137,285 |
| `/en/latest/searchindex.js` | 200 | 926,324 |

The canonical link on every RTD page points at the RTD URL itself. The Chinese
entry of the switch resolves to
`https://cloudquant.github.io/fincore_cpp/zh-cn/latest/api/metrics.html` on the
API page — clicking it on the live RTD site lands on that page with the Chinese
title and `lang="zh-CN"`.

Owner notes: the project was imported with default branch `master`, language
English, no translations. `build.os` and the Doxygen pre-build step live in
`.readthedocs.yaml`, so a future configuration change is a repository change and
shows up in the next build.

## Rule

A local HTML build is "deployable artifact"; a platform build without a live
page check is "online check pending"; neither is a publication PASS. If one
language or platform fails, the previously verified site must remain available
and the failed combination must be recorded.
