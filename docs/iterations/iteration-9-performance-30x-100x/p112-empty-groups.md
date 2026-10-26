# P112 prerequisite: safe preparation without group metadata

Date: 2026-10-25. Repository/branch: `D:/source_code/fincore_cpp`, `dev`.
Base commit: `c761f29ac02a60622119abc71775fd1d27fa6dc8`.
Accepted scope: `src/factor/data.cpp`, its existing unit-test file, and this record.

The complete-output work exposed a pre-existing access violation in the legacy
research tests. Both preparation paths omit the group vector when there is no
mapping, but later indexed it while materializing rows. The offending accesses
are inherited from `332eb08d`; the new detailed evaluator was not the cause.
The fix keeps `has_group=false` and the group string empty without indexing that
vector. Group-aware binning without a mapping now raises `factor_data_error`
before its shared quantizer can index the same absent vector. The added test
exercises this invalid configuration through both preparation entry points.

Validation used MSVC Release (`/O2 /Ob2 /DNDEBUG`) in the existing `build-win`:

```text
cmake --build build-win --target fincore_tests -j 4
build-win/tests/fincore_tests.exe --gtest_filter=-FactorResearchDetailed* --gtest_output=json:build-win/p112-empty-groups-tests-final.json
```

The final run passed **1045/1045** legacy/guard tests, with no skipped tests,
exit zero. JSON SHA-256: `cc7e13fffe488bbdd8611285ad87f2473e07c17546c4cf3bb697de806461a54e`.
The initial uncorrected run terminated with Windows access violation `0xc0000005`;
its log remains at `build-win/p112-legacy-tests.log`. An intermediate four-line
repair passed 1044 tests; the final result above also includes the independently
identified group-binning guard and its new regression test.

P112 detailed-result sources are concurrently present in the checkout and linked
into this development test executable, but its new tests were excluded from this
legacy compatibility run. Those sources, runners and exports are **not accepted
or staged by this prerequisite round**. The final guarded data preparation code
is the only library change in this scoped commit. This is correctness evidence,
not a performance measurement or a 30x/100x acceptance.
