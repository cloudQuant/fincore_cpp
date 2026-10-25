# Quickstart

This example uses a small synthetic frame. It does not download market data
and does not imply an investment result.

```cpp
#include <fincore/dataframe.hpp>
#include <fincore/metrics/ratios.hpp>

#include <vector>

int main() {
    const std::vector<fincore::timestamp> index{
        fincore::utc_timestamp(2025, 1, 2),
        fincore::utc_timestamp(2025, 1, 3),
        fincore::utc_timestamp(2025, 1, 6)};
    fincore::dataframe returns;
    returns.load_index(std::vector<fincore::timestamp>(index));
    returns.load_column<double>(
        "close", {0.01, -0.02, 0.015},
        hmdf::nan_policy::dont_pad_with_nans);
    const double sharpe = fincore::sharpe_ratio(returns);
    return sharpe == sharpe ? 0 : 1;
}
```

Build the example as a downstream consumer of the installed package. The
sample is intentionally small; it checks that headers, installation and the
link interface are usable, not that a factor is profitable.

## Next steps

- Read [data and numerical conventions](conventions.md) before preparing a
  real panel.
- Use the [API reference](api/index.md) for the typed result and configuration
  structures.
- Review the [performance notes](performance.md) before interpreting a
  benchmark or comparing workloads.
