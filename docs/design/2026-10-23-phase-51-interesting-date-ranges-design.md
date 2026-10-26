# Phase 51 Native Interesting Date Ranges Design

## Source contract and defect

`fincore/metrics/timing.py::extract_interesting_date_ranges` iterates a
20-entry `OrderedDict` of `(start, end)` timestamps, slices
`returns.loc[start:end]`, skips empty slices, and returns an ordered mapping.
On a tz-aware index the naive-label slice raises inside the caught block and
every window is skipped; the clean oracle confirms `count=0` for a UTC daily
index and `count=3` for the same dates as a tz-naive index.

Native decision: apply the pinned windows as UTC calendar dates to the
native UTC-ns index and document the source divergence. The oracle goldens
are taken from the tz-naive working path, because the native index is always
UTC and reproducing the skip-all defect would make the API useless.

## Implementation

- `crisis_periods()` is a function-local static vector in insertion order
  with `utc_timestamp` boundaries (Dotcom, Lehman, 9/11, US downgrade,
  Fukushima, US Housing, EZB IR Event, Aug07, Mar08, Sept08, 2009Q1,
  2009Q2, Flash Crash, Apr14, Oct14, Fall2015, Low Volatility Bull Market,
  GFC Crash, Recovery, New Normal).
- Each window selects rows with `start <= index <= end`, preserving original
  row order and values, and returns a frame that owns its copied index and
  values. No sorting or alignment.

## Oracle evidence

Root `/tmp/fincore-phase51-python.brssWS`, detached at `4b8f31b`, module path
asserted, exit `0`, zero stderr bytes. Script SHA-256
`52443772f9bab3bbeded4f601c490c62bddb22c7980dd37f751bf5801b987aad`; raw
output SHA-256
`47e34fd12bf4433b826711f2661d447c260443a5d36e6b29e94c1a98805ee4db`.

Goldens: 78-day 2008 daily fixture → Lehman n=62
(`2008-08-01…2008-10-01`, values 0.007…0.068), Sept08 n=31
(`2008-09-01…2008-10-01`), GFC Crash n=78 (`2008-07-25…2008-10-10`); a
two-day 2020 fixture → one New Normal window with 2 rows.

## Tests

Four timing tests: pinned order and boundaries on the 2008 fixture, New
Normal/empty behavior, selector/NUL precedence, and frame ownership after the
source frame is destroyed.
