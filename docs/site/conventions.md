# Data and numerical conventions

These are the rules every module follows. Where a function deviates, its own
documentation says so.

## Frames and time

The native `fincore::dataframe` is a `hmdf::StdDataFrame` whose index is a signed
64-bit UTC nanosecond timestamp. The representable range is therefore
`1677-09-21T00:12:43.145224192Z` to `2262-04-11T23:47:16.854775807Z`; a calendar
value outside it raises `input_error` rather than wrapping. Sub-day precision is
preserved exactly, and `utc_year` floors toward earlier time — one nanosecond
before the epoch is year 1969, not 1970.

Index requirements belong to each API, and they differ on purpose:

| Requirement | Where it appears |
|---|---|
| Strictly ascending, duplicate-free | Risk estimation, backtests and walk-forward validation — a duplicated or unsorted index raises `input_error` |
| Sortedness not required, rows positional | Simulation paths and analysis inputs — the caller's row order is preserved and the index is copied as-is |
| Uniqueness by adjacency | Attribution alignment, where a sorted index is an unstated premise (recorded as F10-D11) |

Do not assume that a function sorts or deduplicates for you. If an API needs a
particular order, it validates it and says so.

## Returns, units and annualization

Return series are decimal simple returns unless a function states otherwise:
`0.01` means one percent. `daily_annualization` is 252. Ratios that scale by
`sqrt(annualization)` use the factor directly; `annualization_factor(frequency)`
maps `daily`/`weekly`/`monthly`/`quarterly`/`yearly` to 252/52/12/4/1 and returns
an explicit override untouched — including zero or a negative value, because
validation belongs to the caller.

Weights, prices and amounts carry their call-site units and are never converted
implicitly. A function that expects basis points says so.

## Missing values and alignment

`NaN` is the native missing numeric marker; it is not a small number and it is
not zero. Three behaviours coexist and each function documents which one it uses:

- **Drop the observation** (pairwise where two series are involved). Most scalar
  risk and return metrics do this.
- **Reject non-finite input** entirely. Stress testing and some risk kernels
  refuse a `NaN` return rather than skipping it.
- **Propagate**: a window that touches a missing value yields `NaN` for that
  window instead of a partial statistic.

Alignment is equally explicit: strict equality, common index, inner join, or a
documented outer policy. A missing label is never silently replaced with zero.

When loading columns, `hmdf::nan_policy::dont_pad_with_nans` keeps the value as
given; padding policies insert `NaN` for absent positions. Pick the one that
matches the frame's own contract, and remember that an empty result is often
returned as an **empty typed column**, not as a missing column.

## Ownership and lifetime

- `make_series` and `load_column` copy or take ownership; a frame returned by a
  function owns its values. `validate_returns` returns a copy, so writing through
  the result never touches the input.
- `portfolio_inputs` copies at ingestion, and `materialize()` returns fresh frames
  on every call.
- Some accessors return views (a column span, a `std::string_view`). A view is
  valid only while its frame is alive and unmodified.
- `analysis_context` caches computed metrics, which makes one instance
  single-threaded by construction. Share frames freely; share a context only
  behind your own lock.

## Determinism and digests

Results that need to be compared, stored or replayed carry a digest instead of
raw floats:

- Runtime digests length-prefix every field and tag each value with its declared
  type, so neither separators nor a `bool`-versus-`1` collision can make two
  different payloads hash the same.
- Float values are serialized as lowercase IEEE-754 bit patterns, so `NaN`
  payloads and signed zeros survive a round trip. Some writers normalize negative
  zero to positive zero because the bytes are hashed.
- A digest that covers inputs answers "same inputs?"; one that covers outputs
  answers "same result?". The factor model fingerprint hashes inputs, so two runs
  agree before their numbers are compared.

Determinism is per function, not global: a seeded simulation reproduces a stream
inside this library, and only functions that document a NumPy mapping promise the
same numbers as the reference implementation.

## Errors

All library errors derive from `fincore_error` and carry a category, a code and
structured fields:

| Situation | Type |
|---|---|
| Bad argument, missing or mis-typed column, embedded NUL, invalid index | `input_error` |
| Malformed factor panel or configuration | `factor_data_error` |
| Domain validation in metrics (for example an out-of-range cutoff) | returns `NaN`, not an exception |

Out-of-range *values* inside a well-formed frame are usually answered with `NaN`
(for example a cutoff outside `(0, 1)`), while a structural problem throws. Tests
compare rejection messages exactly, so two different diagnoses cannot collapse
into one passing assertion.

## Reports and files

A report document has its own schema version, separate from the runtime
serialization envelope and from artifact-bundle manifests. Writers reject unsafe
destinations — an empty path, a directory that cannot be created, a pre-existing
file where the contract forbids one — and clean up partial output where they say
they do. CSV is an exchange format; it does not carry the provenance of a result
model, which is what the digest and the JSON manifest are for.

## Frequencies and labels

Input period names are canonical words; letter aliases such as `ME`, `QE`, `YE`
are produced on output so a converted frequency means the same boundary in a
modern date range. Label ordering is part of the contract: weekly aggregation
orders rows by calendar year and ISO week label rather than by bucket start,
which can place a January observation after a December row of the same year.
