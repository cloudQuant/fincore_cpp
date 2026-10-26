# Publication and platform status

This page records the deployment state; it is not a substitute for checking a
live site.

## GitHub Pages

The repository has an admin-configured Pages site disabled at the start of
iteration 10. Verified on 2026-10-26 against `cloudQuant/fincore_cpp`:
`has_pages` is `false` and `GET /repos/.../pages` returns 404, so no Pages site
exists yet. The [`docs` workflow](../../.github/workflows/docs.yml) builds
both languages with strict Sphinx checks and uploads one Pages artifact:

- English: `/fincore_cpp/en/latest/`
- Chinese: `/fincore_cpp/zh-cn/latest/`

Deployment is enabled only after the Pages site is configured to use GitHub
Actions and the workflow has run on `dev` or `master`. Enabling Pages is a
repository-administrator action on shared infrastructure and is deliberately
left to the owner; this documentation set does not claim it. The four checks
still required are English/Chinese × Pages/RTD home, installation, one deep API
page, language switch, version label, static assets and search.

## Read the Docs

The versioned [`.readthedocs.yaml`](../../.readthedocs.yaml) is ready for two
linked RTD projects (`en` and `zh_CN`). The repository does not have an RTD
account/project configured in this environment, so RTD build and online checks
are **platform-pending**, not PASS.

## Rule

A local HTML build is "deployable artifact"; a platform build without a live
page check is "online check pending"; neither is a publication PASS. If one
language or platform fails, the previously verified site must remain available
and the failed combination must be recorded.
