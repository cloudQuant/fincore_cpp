# Phase 33 Native Common-Sense Ratio Design

## Scope decision

Phase 32 completes every successful local public calculation path in Python
`fincore.attribution.style`. Phase 33 therefore leaves the attribution/style
line and selects the smallest useful dependency-closed parity gap in the
whole Python package: `common_sense_ratio`.

The selection audit ranked the nearby candidates as follows:

1. **Selected — `common_sense_ratio`.** Python only composes `tail_ratio` and
   `win_rate`; both leaves are already accepted public native APIs. The change
   fits the existing ratio header/source and registered tests, adds no CMake
   entry, and automatically reaches `<fincore/metrics.hpp>`.
2. **`mar_ratio` and `var_excess_return`.** Both are small, but each adds an
   additional annualization or risk interpretation. They remain separate
   scalar parity debt rather than being bundled into this slice.
3. **Weekly/monthly consecutive statistics.** Native calendar aggregation is
   not equivalent to pandas `W-SUN`/month-end resampling across gaps and year
   boundaries, so a direct wrapper would silently change source semantics.
4. **EVT `hill_estimator`.** Its formula is local, but it opens a new advanced-
   risk result/API/CMake surface and requires tail, threshold, and empty-tail
   policy decisions.
5. **Provider, solver, plot, report, and stub routes.** These remain outside a
   dependency-closed Plan A scalar slice.

Python also exposes `stats.common_sense_ratio`, but that function is only a
compatibility delegate to `ratios.common_sense_ratio`. The flat native
`fincore` namespace must expose one symbol, not duplicate declarations in
`metrics/stats.hpp` and `metrics/ratios.hpp`.

This phase adds no embedded Python/pandas runtime, NumPy/SciPy dependency,
provider, network call, solver, new source target, plot source, report,
package, or GUI runtime.

## Public API and exact composition

Extend `<fincore/metrics/ratios.hpp>` with:

```cpp
double common_sense_ratio(
    const dataframe& returns,
    std::string_view column = returns_column);
```

`<fincore/metrics.hpp>` already includes `ratios.hpp`, `risk.hpp`, and
`stats.hpp`, so the existing facade exports the function without another
include or CMake change.

After rejecting an embedded NUL and validating/selecting the genuine-double
column, let `raw_count` be its unfiltered row count. The implementation follows
this control flow:

```text
if raw_count < 2:
    return NaN

tr = public tail_ratio(returns, column)
wr = public win_rate(returns, column)

if isnan(tr) or isnan(wr) or wr == 0:
    return NaN
if wr == 1:
    return +infinity
return tr * wr / (1 - wr)
```

The implementation must call the accepted public `tail_ratio` and public
`win_rate` once each. It must not copy their percentile, finite-filter, or
win-count formulas, and it must not use the private `finite_quotient`, because
that helper converts a zero denominator to NaN while the source requires
`win_rate == 1` to return positive infinity.

Only `std::isnan` gates delegated results. A legitimate positive infinity is
not normalized to NaN, and the final IEEE quotient is not clamped. The
accepted leaves guarantee `win_rate` is in `[0,1]` or NaN.

## Validation order and edge semantics

Before any DataFrame C-string boundary, `column.find('\0')` rejects an embedded
NUL with `fincore::input_error`. The selected column is then resolved with
public `require_double_column` before the raw-length shortcut. Therefore an
absent column, an empty column selector, a wrong-typed column, and a
short/index-mismatched column remain structural failures even when the frame
has fewer than two rows. A bounded non-NUL-terminated `string_view` remains
valid because the public boundary copies its exact extent; a valid zero-row
frame whose selected column is genuinely double returns NaN.

The raw-length rule deliberately precedes finite filtering. This preserves a
source-visible distinction:

- one positive finite observation in a one-row frame returns NaN;
- the same positive observation plus one NaN has raw length two, delegates to
  the accepted leaves, and returns positive infinity;
- two NaNs return NaN;
- zero remains in the win-rate denominator and is not a win;
- no wins returns NaN; and
- all finite wins with at least two raw rows returns positive infinity.

Named columns use the same exact selector for validation and both delegates.
No DataFrame is returned, no index is transformed, and the function retains
no view after the call.

## Deliberate infinity divergence

Python drops NaN but retains signed infinity in `tail_ratio` and `win_rate`.
NumPy percentile arithmetic then commonly turns an infinity-containing tail
into NaN. The accepted native `tail_ratio` and `win_rate` instead use
`detail::finite_values`, which drops NaN and both signed infinities.

Phase 33 composes those already accepted native leaves rather than creating a
third cleaning policy. For example, `{.01, +inf, -.02, .03}` is evaluated
natively from `{.01, -.02, .03}` and returns approximately
`3.294117647058822`, while the current Python source returns NaN with a
RuntimeWarning. The same native policy applies to negative infinity. This
divergence must be explicit in tests and acceptance documentation.

## Source oracle and mutation-resistant tests

The mandatory Anaconda source oracle records these cases:

```text
mixed                 1.4594594594594592
zeros_mixed           1.0833333333333333
all_win               inf
no_win                nan
single                nan
one_finite_two_raw    inf
nan_mixed             3.294117647058822
all_nan               nan
zero_tail             19.0
positive_infinity     nan  # Python source behavior
negative_infinity     nan  # Python source behavior
```

Tests must lock:

- the exact function-pointer signature and the one-argument default call;
- finite and zero-containing Python goldens;
- direct parity with public `tail_ratio` and `win_rate` over several finite
  fixtures, making copied or altered formulas fail;
- raw one-row versus two-row-one-finite behavior;
- all-win positive infinity, zero-win/all-NaN/undefined-tail NaN branches;
- named-column selection, embedded-NUL rejection, a legal bounded
  non-NUL-terminated selector, and structural errors before the length
  shortcut;
- accepted native filtering of both signed infinities; and
- visibility through `<fincore/metrics.hpp>`.

The leaf consumer includes only `<fincore/metrics/ratios.hpp>` among fincore
headers. Both leaf and facade uses must be added before the declaration so a
genuine missing-symbol compile RED is retained; no stale CTest result may be
cited after that failed build.

## Acceptance boundary

Acceptance requires current focused/full CTest plus fresh AppleClang headless,
Homebrew LLVM 22 ASan+UBSan with registration audit and diagnostic scan,
local ImGui/ImPlot/demo compile-link-only without launching the GUI, and a
fresh invalid-ImGui configuration failure matched across physical line wraps.
Actual FetchContent and local dependency provenance must be recorded.

The phase does not claim closure of every scalar metric. `mar_ratio`,
`var_excess_return`, conditional ratios, calendar-resampled consecutive
statistics, EVT, solver/provider paths, aliases, reports, and plots remain
separate candidates.
