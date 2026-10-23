# Phase 47 Native Capture-Return Design

## Source contract

`fincore/metrics/ratios.py::up_capture_return` / `down_capture_return`
(lines 1405–1516): inner-align, select strategy returns where the benchmark
sign matches the window, return NaN on an empty window, otherwise the
annualized (CAGR) return of the selection. `annual_return` uses
NaN-as-zero compounding with the selection length as the exponent
denominator; a single observation is accepted.

## Native decisions

- One private `capture_return_impl` with an `upward` flag; two public leaves.
- Alignment through `align_on_common_index`; both selectors NUL-checked first.
- CAGR implemented in the helper with the same compounding rule as native
  `annual_return`, avoiding construction of intermediate frames.
- No alignment-policy or timezone parameters; inner alignment is the only
  native behaviour.

## Oracle evidence

Root `/tmp/fincore-phase47-python.eEXGHW`, detached at `4b8f31b`, module path
asserted, exit `0`, zero stderr bytes, `FAILURES=0`, post-run checkout clean.
Script SHA-256
`57e2e63d766f072589fa2846c4a88c97a2899894c01024963cfd11dd61692d15`; raw
output SHA-256
`d79dc852ca66e2e2fcb9b7083b74699773578fdb7b4e364fed75aadda455b018`.

Goldens: up `1.5166094207700231`, down `1.473483204701317`,
annualization-52 up `0.20978452786208313`, down `0.20547717849603586`,
single up-market period `11.274002099240244`, NaN-pair up
`-0.6513934704548094`, down `0.65725622476063`; all-up/all-down windows NaN.

## Tests

Five ratio tests plus the umbrella consumer: goldens, empty and single
windows, NaN pairs, selectors, NUL missing-column errors, and lifetime
ownership.
