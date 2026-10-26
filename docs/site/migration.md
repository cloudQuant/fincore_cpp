# Migrating from Python fincore

`fincore_cpp` is a native library, not a Python binding. It does not provide
`import fincore` compatibility, Python exception inheritance, pandas views or
NumPy dtypes. Port workflows by matching the native typed inputs and outputs,
not by mechanically renaming Python objects.

## Mapping table

| Python / pandas | Native equivalent | Note |
|---|---|---|
| `pd.DataFrame` with a `DatetimeIndex` | `fincore::dataframe` | Index is a signed 64-bit UTC nanosecond timestamp |
| `series.values` | `get_column<double>("name")` | Returned as a span/view; copy if it must outlive the frame |
| `series.pct_change()` | `fincore::simple_returns(frame, "close")` | Drops the first observation, keeps the later timestamp |
| `series.shift(n)` | Not provided as a primitive | Most APIs take the lag explicitly (`turnover_periods`) |
| `series.rolling(w).mean()` | `rolling_engine{..., w, 252.0}.compute(...)` | Complete windows only |
| `series.std(ddof=1)` | `calc_distribution_stats(values).std` | The native field divides by the count (population), not `count - 1` |
| `df.groupby(level=0).mean()` | `aggregate_returns(...)` / table builders | Bucket ordering is explicit, not insertion order |
| `np.nan` | `NaN` in a double column | Treated as missing, never as zero |
| `raise ValueError(...)` | `fincore::input_error` / `factor_data_error` | Carries category, code and structured fields |
| `df.to_html()` / `to_excel()` | `render_html(document, ...)` / `render_xlsx(document)` | Render one typed document, not a frame |
| `corr()`, `cov()` | `correlation_breakdown`, `beta`, `tracking_error` | Pairwise alignment on shared timestamps |

## Porting checklist

1. **Find the native symbol.** Every public symbol is in the API ledger; if a
   Python call has no native counterpart, it is not implemented, and a wrapper
   that pretends otherwise will drift.
2. **Check the unit contract.** Native functions take decimal returns, not
   percentages, and never convert units implicitly.
3. **Check the alignment contract.** Pairwise APIs join on shared timestamps;
   strict ones reject duplicated or unsorted indexes. A Python query that relied
   on positional order must be rewritten, not translated.
4. **Check the failure contract.** A `NaN` result and an exception mean different
   things here; see [conventions](conventions.md).
5. **Keep the oracle budget.** Reuse the same fixture and the same reference
   values you used in Python, so a port is verified against evidence rather than
   against itself.

## Known differences that bite

- **Signed zero and NaN payloads.** Digests serialize doubles as IEEE-754 bit
  patterns; some writers normalize negative zero because the bytes are hashed.
- **Annualization defaults.** 252 for daily. A Python helper that used 365 will
  disagree by the square root of the ratio.
- **Deviation divisor.** `calc_distribution_stats` reports the population
  deviation, while pandas defaults to `ddof=1`; the difference is visible on short
  samples and vanishes only asymptotically. It also does not filter `NaN`.
- **Drawdown signs.** `max_drawdown` returns a negative fraction, while
  `gen_drawdown_table` reports a positive percentage of the prior peak.
- **Backtest verdicts.** `inconclusive` is a real outcome, not a failure; the
  five-expected-exception floor is applied on purpose.
- **Optional routes.** Provider or extension surfaces marked optional are not
  parity claims. The iteration records list what is implemented and what is
  deferred.

## What parity means here

The repository's parity records describe the implemented native surface and its
declared variances. The iteration-8 record is the authoritative scope snapshot;
later performance work does not retroactively expand it. Check the dated
acceptance records for dependency and platform status rather than inferring it
from a phase count.

For a migration review, map each Python call to a public native symbol, verify
its unit and alignment contract, and retain the same fixture or oracle budget. A
port that changes both the code and the reference values proves nothing.
