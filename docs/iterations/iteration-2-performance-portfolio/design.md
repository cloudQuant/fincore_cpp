# Iteration 2 — Performance and Portfolio: Design

**Status:** Revised plan. [Shared contract](../delivery-contract.md) applies.

## 1. Native leaves and reuse

Add `include/fincore/performance/{returns,cashflows,inference,disclosures}.hpp`
and matching source/test entries. Capacity types may use
`include/fincore/portfolio/capacity.hpp`; transaction liquidity primitives
should extend the existing native transaction leaf unless the phase design
justifies a different owner. Python source path and native layout need not
be identical, but one canonical implementation is required. Register actual
new source/test TUs and public consumer tests in CMake.

No new solver target is assumed. Phase 55 compares an internal bracketed
solver or a pinned dependency using measured parity and maintenance costs.
Share private IRR preprocessing and solve routines between MWR and XIRR.
Do not commit to an inaccurate solver merely to avoid a dependency.

## 2. IRR and calendar design before implementation

Characterize preprocessing separately from the solver: validation order,
stable equal-period aggregation, sign transition count, finite/degenerate
flows and representable rate domain. Solve the scaled NPV in log-gross-rate
space, then convert to a rate. Preserve convergence status internally and
map public failure results to the pinned contract.

The native date model must preserve the oracle's local-calendar-day
semantics. The phase design chooses explicit normalized local dates or
additional timezone metadata at the boundary; it cannot reinterpret local
midnight as a UTC date and claim equality. Include daylight-saving and
same-local-day cases as applicable to the declared supported input contract.
XIRR's 365-day convention is explicit, including leap-year fixtures.

Phase 56 freezes start/end cashflow timing, valuation ownership and units.
53 remains simple return chaining; 56 composes it after adjusting returns.
Use owning cashflow/disclosure/capacity result types and stable label order.

## 3. Liquidity and disclosures

Phase 59 first inventories existing liquidation/turnover/slippage primitives;
add only missing behavior with explicit zero/missing volume, cash column,
asset-label and date alignment semantics. Phase 58 then composes those
primitives into `assess_liquidity`. Do not rename this composition as a
capacity sweep.

57b preserves both context fields and emitted disclosure text. Freeze field
order, missing applicability, Unicode and template semantics separately
from future HTML escaping. Phase 57 can reuse a validated private normal
quantile helper after its contract is proven, rather than depending on all
of Iteration 1.

## 4. Sequence

After 39: 55 solver/date contract → 54; 53 can run independently and precedes
56's TWR composition; 59 → 58. 57 and 57b are independent subject to their
actual helpers. Phase numbers are identifiers, so this corrects the old
54-before-its-solver and 58-before-its-primitives inversions without renumbering.

Each design freezes exact signatures, error/shape tables, numerical budgets,
fixtures, benchmark workload and reusable helper mappings before coding.
