# Iteration 8 evidence index

All paths are relative to this directory. Raw failures are retained beside the
qualified passes; no failed run was deleted.

| Phase | Evidence | Status |
|---|---|---|
| P100 | `p100/new-pin-survival.json`, `p100/route-split-validation.log` | survival 64 identical + 7 body-changed + 10 plan-derived; 0 missing |
| P101 | Release CTest log `p109/macos-release-ctest.log` | errors/DataFrame semantics tests pass |
| P104 | Release CTest log (factor artifacts/serialization/timedelta/workflows) | pass |
| P105 | `p105/registered-operation-ids.txt`, `p105/operation-registration.txt` | 270/270 native, 3 optional excluded |
| P106 | `p106/captures/manifest.json` and `.png` files | 5 offscreen panels, hashes retained |
| P107 | Release CTest log (extension snapshot/discovery tests) | pass |
| P108 | `p108/artifact-manifest.json`, `p108/artifacts/`, `p108/report-page-1.png` | all value checks passed |
| P109 | `p109/macos-release-ctest.log`, `p109/performance.json` | 1044/1044 release; performance status per JSON |
| P110 | `p110/macos-asan-*.log`, `p110/macos-asan-qualification.txt`, `p110/consumer-*`, platform logs | release/ASan/consumer; Ubuntu rows per logs |
| P110 Windows | `p110/windows-build.log`, `p110/windows-ctest.log`, `p110/windows-consumer-*`, `p110/windows-artifact-manifest.json`, `p110/windows-portability-changes.md` | clean build 246/246; CTest 1044/1044; relocated consumer exit 0; 13/13 artifact value checks |

Windows performance is not claimed: the Iteration 7 C4 archive is absent and
the gate cannot be executed (`p110/windows-performance-blocker.txt`). The PDF
page raster check is declared skipped on Windows because `pdftoppm` is not
installed; PDF text values passed.

The macOS logs linked from the acceptance record were restored from the
previous source archive as loose files and verified against `SHA256SUMS`.
Windows logs are also loose files. All evidence is included in the regenerated
source archive.

The Python test-port ledger is under
`docs/iterations/iteration-8-full-python-parity/test-port-ledger.tsv`; the route
ledger and split are in the same folder.
