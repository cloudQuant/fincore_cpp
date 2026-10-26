# A-share daily panel — provenance

Copied on 2026-10-24 from the local Backtrader test dataset:

`D:\source_code\backtrader\tests\datas\a_stocks`

The payload (`a_stocks/`) is deliberately ignored by Git because it is about
94 MB across 5,471 per-symbol files. Copy it again with:

```powershell
Copy-Item -Recurse -Force `
  D:\source_code\backtrader\tests\datas\a_stocks `
  benchmarks\alpha003_a_stocks\data\a_stocks
```

## Shape and semantics

- 5,471 stock CSVs plus `_metadata/` and the source `README.md`.
- Columns: `trade_time,open,high,low,close,volume,amount`.
- `trade_time` is the last observed source minute on the Asia/Shanghai wall
  clock (`15:00:00.000`), one row per observed trading day.
- Range: 2025-01-02 through 2025-12-31, 243 observed sessions.
- Prices are unadjusted; volume and amount keep the source units. The upstream
  README documents the first-open / last-close / sum-volume aggregation.
- The benchmark converts local timestamps to UTC by subtracting eight hours.

## Integrity pins

| File | SHA-256 |
|---|---|
| `_metadata/manifest.json` | `7ac8fea0ceb547e010a139f4a17820f6207f38ab08c762cf19257a0f39ec2cfb` |
| `_metadata/observed_calendar.csv` | `ebffd7c7a5626a43ea54f5135c86fb0c8c554255e85ee09ba9bfc5e9da324ba1` |

`observed_calendar.csv` provides the shared session grid: `first_trade_time`
(09:30 local) and `last_trade_time` (15:00 local). The benchmark uses the last
trade time as the close-time index and the first trade time as `open_time` for
next-open labels.

## Licensing

The copied directory keeps the upstream `README.md` and `_metadata/` files.
Refer to the upstream Backtrader repository for the data license and source
terms before redistributing the panel.
