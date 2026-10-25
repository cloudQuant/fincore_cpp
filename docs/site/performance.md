# Performance method and limits

Performance claims must state the workload, data scale, CPU/thread settings,
environment, statistic and noise handling. The native library and the Python
reference are different implementations; a throughput comparison is not a
claim of Python API equivalence.

The repository's acceptance records contain the raw benchmark method and
dated results. The Iteration 9 and alpha003 records explicitly preserve
INCONCLUSIVE outcomes; do not summarize a noisy run as a stable speedup.
Historical CSV and HTML benchmarks use different workloads and are not
interchangeable.

For a reproducible run, use the committed benchmark source and the documented
data preparation steps. Large generated results are not part of the docs site.
