## What changes

<!-- One paragraph: the behaviour or document that changes, and why. -->

## Verification

<!-- Commands you ran and their results. Say what you could not verify. -->

- [ ] `ctest --test-dir build --output-on-failure`
- [ ] both language builds (`-W -n --keep-going`)
- [ ] `python docs/site/tools/check_built_site.py`
- [ ] `tools/check_comment_only.py` for comment-only changes

## Notes for the reviewer

<!-- Trade-offs, follow-ups, or anything deliberately left out. -->
