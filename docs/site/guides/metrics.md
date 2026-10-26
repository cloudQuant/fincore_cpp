# Metrics and rolling windows

Metric functions consume a `fincore::dataframe` and return typed scalars or
series. Units are decimal returns unless the API says otherwise. `NaN` inputs
are missing; a window is evaluable only when its documented completeness rule
is satisfied.

- Scalar risk/return metrics are in `include/fincore/metrics/`.
- Rolling and multi-column statistics are in `metrics/rolling.hpp` and
  `rolling_engine.hpp`.
- Date-wise and period summaries preserve the input ordering rules stated in
  their API entries.

Use the numerator/denominator conventions described by the function rather
than assuming that two similarly named metrics annualize or handle ties the
same way. The API reference is generated from the public headers.
