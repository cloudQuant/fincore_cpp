# Performance method and limits

A performance claim is only meaningful with six things attached: the workload,
the data scale, the CPU and thread settings, the environment, the statistic, and
how noise was handled. This page states how the repository's numbers were
produced so a reader can judge, or reproduce, them.

## How the harness measures

The paired harness runs the pinned Python reference and the native candidate on
the same machine, alternating them, with one CPU pinned for the measured child:

- Each measured run is a **child process** whose affinity is narrowed before
  `CreateProcess`/`fork` and verified after start, so the parent's scheduler
  choices cannot leak into the measurement.
- The wall interval starts immediately before the child is created and ends when
  it exits, so interpreter start-up and loader time are **inside** the
  measurement for both sides.
- Warm-up runs are excluded; the observer (digest capture, counters) runs after
  the stop clock, except the allocation counter, whose atomics fire inside the
  measured call.
- CPU time and peak working set come from the operating system's process
  accounting, not from a self-report.
- Correctness is a **gate**, not a separate report: the research tables produced
  during the run are compared by digest before the timing is accepted.

Pairs are alternated and summarised by the median, so a single slow run cannot
become the headline. A campaign that cannot separate the noise is recorded as
INCONCLUSIVE rather than rounded up into a speed-up.

## What the numbers mean

| Result | Reading |
|---|---|
| **41.20x** on the Alpha003 500-file development batch | Wall-clock ratio for that workload, five measured pairs, same CPU |
| **34.46x / 35.63x** on the 1,000 / 2,000-file diagnostic pairs | The ratio moves with scale; these are separate diagnostic pairs, not a gate |
| Peak working set | Reported separately; a faster run that uses much more memory is not a free win |
| Digest equality | Every accepted timing run also reproduced the reference tables |

The x-factor is a ratio of wall-clock time for **one workload** on **one host**.
It is not a claim about every API, about the full corpus, or about a different
machine. The historical CSV-export benchmark and the current HTML research
benchmark are different workloads and are not interchangeable.

## Reproducing a measurement

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel --target fincore_alpha003_research
python benchmarks/alpha003_a_stocks/run_benchmark.py --help
```

The benchmark sources and the data-preparation steps are committed; the generated
HTML reports and CSV payloads are not. Read the driver's `--help` for the
supported pair counts, tolerance ceiling and output locations, and keep the
default `--tolerance` (it must be finite and at most `1e-6`).

## Limits and honest gaps

- Measurements were taken on a single desktop-class machine with one core pinned;
  no cloud or container claim is implied.
- A noisy environment produces INCONCLUSIVE campaigns. Those records stay in the
  repository; they are not summarised into a stable speed-up.
- Memory is reported as peak working set, which is not the same as the allocator's
  live bytes; the native harness tracks both when the allocation hooks are built.
- Comparing a workload that materialises every intermediate row against one that
  streams will always look dramatic; compare outputs before you compare times.

For the exact campaign records — pair counts, machine, dates and verdicts — see
the iteration 9 closeout and the acceptance records linked from the repository
root.
