# Phase 59d Design — Exposures and Daily Transaction Bars

## Source contracts

- `get_sector_exposures`: sum positions by the caller's symbol→sector map,
  append cash; unmapped assets are excluded (source warns only).
- `compute_style_factor_exposures`: inner-aligned positions/risk panels;
  `sum(position*factor)/sum(|position|)` with NaN products skipped;
  inf→NaN→0.
- `compute_sector_exposures` / `compute_cap_exposures`: long/short/gross/net
  shares over frozen category definitions; zero totals become NaN then 0;
  cap buckets use inclusive `[lower, upper]` boundaries.
- `compute_volume_exposures`: active nonzero shares require finite positive
  volumes; common columns; per-row linear quantile of `shares/volume` ×100;
  zero/inf→0; percentile in [0, 1].
- `daily_txns_with_bar_data`: absolute daily amount per symbol joined with
  close price and volume; symbol-major ordering.

## Native decisions

- Panels must share one timestamp index (`input_error` otherwise); the source
  inner-aligns, but the native boundary is strict to avoid silent date loss in
  exposure math.
- Category definitions are ordered spans (`sector_definition {id, name}`),
  fixing output column order; cap buckets are internal frozen definitions.
- Exposure percentages use 0 for zero denominators and non-finite values,
  matching the source's `replace(inf, nan).fillna(0)`.
- Volume quantiles skip NaN fractions and use the same linear interpolation as
  the other native percentile code; empty pools produce 0.
- Daily bars are owning records in symbol-major then date order; a missing
  price/volume column or date throws.

## Oracle evidence

Root `/tmp/fincore-phase59d-python.H8TKLD`, detached at `4b8f31b`; main
script SHA-256
`d729f5a89ae67f411fb16db5f294a6aa427110b2fba0723b958e338d89ec983c`, raw
output SHA-256
`7610b74b1ddb3da3fc5ecdd9bda35d712860ff5ff47fc434334cb08d9e4c248d`;
string-sector script SHA-256
`067a169a698fb676befb227b18dd16f87c4602bdf4959c04e787678970ba9fb4`, raw
output SHA-256
`3b3516fba463e3f6c76375ae2ac85f72d9646ecf5bf8a18b9c87e7205780b565`.

Goldens: sector sums Financials `[-50,-80,30,-40]`, Materials
`[120,60,-5,105]`, cash 1000; style
`[0.4647058823529412, 0.07142857142857148, -0.03461538461538462,
0.5293103448275862]`; sector bundle long/short/gross/net as printed; cap
bundle; volume exposures `long [8.3333, 3.3333, 4.8701, 6.4141]`,
`short [10, 13.3333, 2.2222, 5.7143]`, `gross [10, 4.1667, 4.2857, 5.7143]`;
daily bars AAA Jan2 (10/11/200), AAA Jan3 (20/12/150), BBB Jan2 (5/19/60),
CCC Jan4 (3/17/45).

## Tests

Three positions tests and one transactions test with those goldens plus
volume validation errors.
