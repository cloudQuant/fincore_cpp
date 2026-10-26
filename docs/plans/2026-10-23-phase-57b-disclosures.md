# Phase 57b Native Disclosure Context Plan

**Goal:** Add `disclosure_context` and `render_disclosure`: the GIPS-aware
text template with its established defaults.

**Baseline:** `dev` after Phase 57 (oracle pin `4b8f31b`). Python source:
`fincore/performance/disclosures.py` (whole module).

**Files:** `include/fincore/performance/disclosures.hpp` (new),
`src/performance/disclosures.cpp` (new), `CMakeLists.txt`,
`tests/unit/performance_disclosures_test.cpp` (new), `tests/CMakeLists.txt`.

## Native API

```cpp
struct disclosure_context {
    std::string convention = "TWR";
    std::string sample_period;
    std::string data_quality;
    std::string fees = "gross-of-fees";
    std::string cashflows = "none";
    std::string benchmark;
    std::string risk_free;
    bool annualized = true;
    std::vector<std::string> notes;
    std::string return_type;
    std::string units;
    std::string frequency;
};
std::string render_disclosure(const disclosure_context&);
```

## Contract

Eleven fixed lines in the source order; empty fields render `unspecified`
except `benchmark`, which renders `none`; `annualized` renders `yes`/`no`;
notes append `Notes: a; b` only when non-empty. Defaults match the Python
dataclass exactly. This is text output only, not HTML/XLSX rendering.

## Tests

Default, fully custom with notes, and all-empty contexts compared against
exact multiline strings from the oracle.

## Gates

Clean detached oracle at `4b8f31b`; full CTest; LLVM 22 ASan+UBSan;
acceptance record and README ledger.
