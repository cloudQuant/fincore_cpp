# Layout Alignment — Portfolio Domain Acceptance

## Scope

Approved post-Iteration-3 layout refactor: move the position, transaction and
round-trip APIs out of `metrics` into the `portfolio` domain, matching the
pinned Python package (`fincore/portfolio/{positions,transactions,round_trips}.py`).

| Before | After |
|---|---|
| `include/fincore/metrics/positions.hpp` | `include/fincore/portfolio/positions.hpp` |
| `include/fincore/metrics/transactions.hpp` | `include/fincore/portfolio/transactions.hpp` |
| `include/fincore/metrics/round_trips.hpp` | `include/fincore/portfolio/round_trips.hpp` |
| `src/metrics/positions.cpp` | `src/portfolio/positions.cpp` |
| `src/metrics/transactions.cpp` | `src/portfolio/transactions.cpp` |
| `src/metrics/round_trips.cpp` | `src/portfolio/round_trips.cpp` |

Headers were moved with `git mv`; no alias/forwarding header was created and
no library symbol changed. `fincore::metrics.hpp` no longer includes the
portfolio headers, so it exports only the metrics domain. Cross-domain users
include the concrete portfolio header (`perf_attrib.hpp` and
`capacity.hpp` were updated; same-domain transaction/round-trip headers now
include `portfolio/...`).

Tests: `positions_test`, `transactions_test`, `round_trips_test` include the
new paths; `metrics_umbrella_test` now includes the three portfolio headers
explicitly to keep its position/transaction/round-trip coverage while the
umbrella stays metrics-only. CMake source paths point at `src/portfolio/`.
No test file was moved (tests remain flat under `tests/unit/`).

## Verification

- Grep for `metrics/{positions,transactions,round_trips}` across
  `include/`, `src/`, `tests/` and the CMake source list returns no matches.
- Release CTest: **661/661 passed**.
- LLVM 22 ASan+UBSan: **661/661 passed**, no sanitizer reports.
- ImPlot compile/link unchanged.

Historical design/acceptance records keep their original paths as historical
evidence; this record is the current layout contract. The Python portfolio
package has no public umbrella (`__all__` empty), so no native `portfolio.hpp`
umbrella is introduced.
