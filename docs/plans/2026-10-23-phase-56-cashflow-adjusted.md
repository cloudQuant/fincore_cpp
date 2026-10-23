# Phase 56 Native Cashflow-Adjusted Returns Plan

**Goal:** Add `cashflow_adjusted_returns` and `cashflow_adjusted_twr` with
explicit event timing and fee treatment, separate from the plain TWR in
Phase 53.

**Baseline:** `dev` after Phase 54 (oracle pin `4b8f31b`). Python source:
`fincore/performance/cashflows.py` (lines 276–391).

**Files:** `include/fincore/performance/cashflows.hpp` (new),
`src/performance/cashflows.cpp` (new), `CMakeLists.txt`,
`tests/unit/performance_cashflows_test.cpp` (new), `tests/CMakeLists.txt`.

## Native API

```cpp
enum class fee_treatment { net, gross };

dataframe cashflow_adjusted_returns(
    const dataframe& valuations,
    const std::optional<dataframe>& cashflows = std::nullopt,
    const std::optional<dataframe>& fees = std::nullopt,
    fee_treatment treatment = fee_treatment::net,
    value_column = "value", amount_column = "amount",
    timing_column = "timing", fees_column = "fees");

double cashflow_adjusted_twr(/* same parameters */);
```

The ledger is a frame with `amount` and `timing` double columns; `timing`
takes `0` (end) or `1` (start), the explicit numeric encoding of the Python
`"end"`/`"start"` metadata. Duplicate dates are allowed and summed by timing.
Fees are a frame with a `fees` column aligned to a subset of valuation dates.

## Contract

```text
valuations: >=2 rows, unique increasing UTC index, finite, non-negative,
            all but the last strictly positive
ledger dates must exist in the valuation index; amounts finite; timing in {0,1}
fees finite, dates must exist; first-date flows and fees must be zero
cashflow-adjusted capital base > 0; adjusted closing >= 0; all finite
end timing:   (close + gross_fee - flow) / (open + start_flow) - 1
start timing: (close + gross_fee) / (open + start_flow) - 1
net treatment excludes fees from closing
TWR compounds (1+r) and rejects non-finite / unrepresentable results
```

## Scope note

The source's currency/FX branch is out of this slice: the native boundary has
no currency parameters, so only same-currency (reporting) flows are
representable; non-reporting-currency inputs are impossible rather than
silently converted. The Series-with-scalar-timing path maps to a uniform
`timing` column.

## Tests

Ledger net/gross, uniform end/start, same-day aggregation, terminal zero,
first-date flow/fee rejection, and NUL validation with oracle-exact returns
and TWR values.

## Gates

Clean detached oracle at `4b8f31b`; full CTest; LLVM 22 ASan+UBSan;
acceptance record and README ledger.
