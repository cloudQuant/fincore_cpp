# Reports, runtime and extensions

Report schema 2 is a shared typed document. Native HTML, XLSX, PDF and
Plotly/Bokeh projections consume the same computed values; they are not a
promise of Python/pandas byte identity. Artifact bundles write a versioned
manifest and reject unsafe or preexisting destinations according to their
API contract.

The runtime catalog exposes typed operations with explicit inputs and
deterministic serialization where documented. Sessions cache deterministic
operations, own their inputs/outputs and reject calls after close.

Extension discovery consumes the native snapshot/hook ABI described by the
public extension headers. Optional plotting adapters are not part of the core
CMake consumer contract and must be enabled explicitly.
