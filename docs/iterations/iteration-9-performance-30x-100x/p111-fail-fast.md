# P111: stop campaigns after output mismatch

2026-10-25, `D:/source_code/fincore_cpp`, `dev`, base
`3a2fe692de926427b492bd280e2c403b294a3e38`.

The complete-output alpha003 campaign now stops after a failed warm-up comparison
or measured pair. It preserves all completed process observations and the failed
comparison, records the requested pair count and stop reason, and does not spend
later samples measuring an already incorrect candidate. Successful workloads and
timed child commands are unchanged. The console summary now names the external
whole-process wall measurement, consistent with the acceptance definition, instead
of presenting the internal pipeline total as the headline ratio.

Environment metadata additionally records SciPy and NumPy's compiled BLAS/SIMD
configuration outside timed intervals. Local readback identifies SciPy 1.17.0 and
the existing Windows balanced power scheme; no system settings were changed.

Validation: `C:/Python314/python.exe -W error -m unittest
tools.tests.test_alpha003_benchmark -v` passed 3/3. The new case proves that a
warm-up mismatch retains its evidence and prevents further process launches.
Existing pair-order/failure-retention and non-overwrite checks still pass.
`host_identity` readback returned the new fields successfully. No performance
campaign or accepted P112 oracle was repeated for this tooling change.

Scope: alpha003 orchestration, its focused test file and this record. The generic
reference/candidate campaign and separate rolling-engine workload are still being
implemented in other files and are excluded from this commit.
