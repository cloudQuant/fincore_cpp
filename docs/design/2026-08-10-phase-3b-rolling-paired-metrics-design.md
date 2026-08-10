# fincore C++ Phase 3B Rolling Paired Metrics Design

## Purpose and bounded scope

Phase 3B adds the five Python rolling metrics that consume one strategy-return
series and one benchmark-return series but produce a single timestamped numeric
series:

- `roll_alpha`
- `roll_beta`
- `roll_up_capture`
- `roll_down_capture`
- `roll_up_down_capture`

They belong together because they share a one-time common-timestamp alignment,
completed-window endpoint index, and existing scalar metric delegation. This
phase deliberately does not add the two-column `roll_alpha_beta` or
`rolling_regression`, the full-index `rolling_beta`, multi-factor DataFrames,
or changes to `rolling_engine`. Those require a separate multi-column output
and paired full-index contract.

## Decision

The established C++ DataFrame boundary aligns the two input columns with
`align_on_common_index`, retaining common timestamps in the left/strategy
frame's supplied order. Python's two-Series helper can use a concat-based
outer-shaped intermediate in some mismatched-index cases; Phase 3B follows the
accepted native common-index contract instead of reproducing that artifact.

After this one-time alignment, every complete window is materialized as two
ordinary one-column DataFrames and delegated to the corresponding accepted
public scalar function. This preserves the existing C++ filtering and
non-finite behavior within each metric. It avoids duplicating alpha, beta, or
capture mathematics in the rolling facade and avoids a new global filter.

## Public API and output shape

`include/fincore/metrics/rolling.hpp` will add:

```cpp
dataframe roll_alpha(const dataframe& returns, const dataframe& benchmark,
                     std::size_t window = 252U, double risk_free = 0.0,
                     frequency period = frequency::daily,
                     std::optional<double> annualization = std::nullopt,
                     std::string_view returns_column_name = returns_column,
                     std::string_view benchmark_column_name = benchmark_column);

dataframe roll_beta(const dataframe& returns, const dataframe& benchmark,
                    std::size_t window = 252U, double risk_free = 0.0,
                    frequency period = frequency::daily,
                    std::optional<double> annualization = std::nullopt,
                    std::string_view returns_column_name = returns_column,
                    std::string_view benchmark_column_name = benchmark_column);

dataframe roll_up_capture(const dataframe& returns, const dataframe& benchmark,
                          std::size_t window = 252U,
                          frequency period = frequency::daily,
                          std::string_view returns_column_name = returns_column,
                          std::string_view benchmark_column_name = benchmark_column);

dataframe roll_down_capture(const dataframe& returns, const dataframe& benchmark,
                            std::size_t window = 252U,
                            frequency period = frequency::daily,
                            std::string_view returns_column_name = returns_column,
                            std::string_view benchmark_column_name = benchmark_column);

dataframe roll_up_down_capture(
    const dataframe& returns, const dataframe& benchmark,
    std::size_t window = 252U, frequency period = frequency::daily,
    std::string_view returns_column_name = returns_column,
    std::string_view benchmark_column_name = benchmark_column);
```

Each result has exactly one double column named by `returns_column_name` and
contains one row for every complete aligned window. Its index is the final
timestamp of that window from the aligned left input. Thus an aligned source of
length `n` and a window of `w` has `n - w + 1` result rows when `n >= w`, or a
typed empty result otherwise. The functions preserve left input order; they do
not sort time.

## Computation and edge behavior

All public calls first validate both requested columns through
`align_on_common_index`. Empty genuine double columns are valid: if alignment
is empty the result is an empty typed series. Missing columns, wrong-typed
columns, empty names, and index/column length mismatches retain `input_error`.
`window == 0` also throws `input_error`; no empty/short input bypasses that
check.

The aligned pair is sliced before calling the scalar metric:

- `roll_alpha` resolves `annualization_factor(period, annualization)` once and
  delegates every window to `alpha` with that factor and `risk_free`.
- `roll_beta` forwards `risk_free` to `beta`. It accepts `period` and
  `annualization` for Python-signature compatibility but deliberately ignores
  both, exactly as the accepted scalar beta path does.
- The three capture functions resolve `annualization_factor(period, nullopt)`
  once and respectively delegate to `up_capture`, `down_capture`, and
  `up_down_capture`.

There is no new finite-only prepass. In particular, NaN and infinity behavior
is the behavior of the chosen scalar delegate for that window: alpha/beta own
their paired-value rules, while captures own their finite-pair and benchmark
sign selection. A no-usable-sample scalar result is stored as its normal NaN.

## Verification boundary

The local Python oracle is used only for fully finite, identically indexed
Series where native scalar delegation and Python agree. With UTC days
2021-01-01 through 2021-01-06, returns
`[.01, -.02, .03, 0, -.01, .02]`, benchmark
`[.005, -.01, .02, .01, -.02, .01]`, and window `3`, the endpoint values are:

- alpha: `[-.3431833620809164, -.8357653939371649,
  1.6310200676280466, 1.313133510455331]`
- beta: `[1.6666666666666659, 1.571428571428571,
  .8461538461538458, .6666666666666666]`
- up capture: `[6.637108874074204, .9752504552841719,
  .9752504552841719, .9866347325869022]`
- down capture: `[1.079619499803848, 1.079619499803848,
  .9262522584870747, .9262522584870747]`
- up/down capture: `[6.147637084435837, .903327936797512,
  1.0528994087173726, 1.0651900964845744]`.

Tests additionally pin left-order common-index alignment, empty and short
alignment, window-one and zero behavior, custom column names, custom alpha
annualization/risk-free inputs, beta's ignored annualization inputs, invalid
structural inputs for both sides, and a NaN fixture compared directly against
the corresponding public scalar window delegate. The acceptance gate will use
public headers only, a fresh AppleClang headless suite, a fresh Homebrew LLVM
ASan+UBSan suite, and the existing invalid-ImGui diagnostic. A positive ImPlot
build remains unavailable until the user supplies a real Dear ImGui checkout.
