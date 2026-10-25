# Data and numerical conventions

## Frames and time

The native `fincore::dataframe` is a `hmdf::StdDataFrame` whose index is a
signed 64-bit UTC nanosecond timestamp. Index order and uniqueness are
validated by the individual APIs; do not assume that every function sorts or
deduplicates implicitly.

## Returns, units and annualization

Return series are decimal simple returns unless a function states otherwise.
`0.01` means one percent. Annualization factors are passed explicitly where an
API exposes them; `daily_annualization` is 252. Portfolio weights, prices and
amounts carry their call-site units and are not converted implicitly.

## Missing values and alignment

`NaN` is the native missing numeric marker. Multi-input APIs document their
alignment policy: strict equality, common index, inner join, or an explicit
outer policy. A missing label is not silently replaced with zero. Windowed
metrics state their warm-up and complete-window rules in the API reference.

## Randomness

Simulation and bootstrap APIs accept an explicit seed where documented. A
matching seed is not a cross-language RNG-identity claim unless the reference
implementation is also the pinned algorithm. Read each result's failure
status and diagnostics instead of assuming convergence.

## Reports and files

Report schema versions and artifact-bundle manifests are separate version
domains from the runtime serialization envelope. Writers reject unsafe or
preexisting destinations and clean up partial output where documented. CSV is
an exchange format; it does not carry the full provenance of a result model.
