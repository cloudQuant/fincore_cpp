# Phase 83 Completion Slice — Research Segment Provenance

## Scope

Adds `research_spec.segments`: a map of segment name to a `(first, second)`
bound pair. The evaluator validates that names and both bounds are non-empty
and records the request in the result manifest, matching the source's segment
provenance without changing factor or label computation.

## Behaviour

- Segment entries appear as `segment.<name> = <first>..<second>` in the native
  manifest (the source keeps a nested mapping; this flattening is declared).
- Empty names or bounds fail with `factor_data_error` before evaluation.
- Segments do not filter observations; they are recorded request metadata, as
  in the pinned evaluator.

## Tests and gates

`FactorResearch.RecordsSegmentProvenance` asserts two manifest entries and the
fail-closed bounds/name validation. Full Release and ASan+UBSan suites:
**728/728 passed**. Visual NOT_RUN.

## Remaining research gaps

Grid/next-open labels, universe/group inputs, multi-factor evaluator assembly
and multi-segment result tables remain open.
