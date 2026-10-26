# Phase 14 Factor Attribution Decomposition Design

**Date:** 2026-08-12

## Decision

Phase 14 adds the deterministic, DataFrame-native decomposition slice of
Python `fincore.attribution.fama_french.FamaFrenchModel.attribution_decomposition`.
It deliberately remains a one-shot function over caller-supplied return and
factor tables.  It is not a stateful Fama-French model, a standard-factor
selector, a provider, or a second regression implementation.

The native operation delegates exactly once to the accepted Phase 12
`fit_factor_regression` public leaf.  It then combines the fitted alpha and
each coefficient with the raw arithmetic mean of its selected factor column.
The result is an ordered, typed DataFrame rather than a dynamic Python dict so
that it can stay in the native DataFrame pipeline without inventing a map-like
facade.

The source's `specific_return` label is retained literally.  In the Python
implementation it is the **sum of factor contributions**, not a residual or
idiosyncratic-return estimate.  `unexplained` is likewise the arithmetic
remainder after alpha and that sum.  The C++ API documents those source-defined
meanings instead of renaming them to more conventional finance terms.

## Public shape

The new leaf is `include/fincore/attribution/factor_attribution.hpp` and is
later exported by `include/fincore/attribution.hpp`.

```cpp
inline constexpr std::string_view factor_attribution_component_column{
    "component"};
inline constexpr std::string_view factor_attribution_contribution_column{
    "contribution"};

dataframe factor_attribution_decomposition(
    const dataframe& returns,
    const dataframe& factor_returns,
    std::span<const std::string_view> factor_column_names,
    double risk_free_rate = 0.0,
    std::string_view returns_column_name = returns_column);
```

The result has synthetic ordinal timestamp index `0..K+3`, exactly two
columns, and no other metadata:

| Row | `component` (`std::string`) | `contribution` (`double`) |
| --- | --- | --- |
| 0 | `alpha` | fitted intercept |
| 1..K | `<requested-factor>_attribution` | coefficient times raw factor mean |
| K+1 | `specific_return` | sum of the preceding factor-contribution rows |
| K+2 | `common_return` | raw return mean minus `risk_free_rate` |
| K+3 | `unexplained` | `common_return - alpha - specific_return` |

The requested factor rows preserve caller order.  In particular, no standard
`3factor`/`5factor` model selection is exposed and no factor names are inferred
from table metadata.

## Boundary and calculation contract

The explicit factor-name span is nonempty.  Names must be nonempty, contain no
embedded NUL, be distinct as full `std::string_view` values, and not equal the
reserved Phase 12 `alpha` label.  The return selector must also contain no
embedded NUL.  The leaf owns all validated selector strings before it crosses a
DataFrame C-string boundary or delegates to the older public factor-regression
leaf; a bounded non-NUL-terminated `std::string_view` is therefore valid.

`risk_free_rate` must be finite.  The public Phase 12 fit is called once with
the owned selectors and supplies the established strict boundary: selected
columns exist, are genuine `double`, match their owning index length, are
finite; raw timestamp indexes are unique and exactly equal in supplied order;
and the input has at least `K + 1` rows with a full-rank design.  This leaf does
not align, sort, drop, resample, deduplicate, or weaken any of those contracts.
Unselected malformed metadata remains irrelevant.

After a successful fit, the implementation reads the selected spans in raw row
order and calculates ordinary arithmetic means.  For each requested factor it
uses the corresponding public coefficient row, so its contribution is
`beta[factor] * mean(factor)`.  It forms the factor sum, return mean,
`common_return`, and `unexplained` in that order.  A non-finite aggregation
intermediate or output is an `fincore::input_error`; the API never publishes an
accidental infinity or NaN from an otherwise finite request.

All DataFrame output vectors and component strings are value-owned.  Later
source-table or name-storage mutation/destruction cannot change the result.
There is no valid typed-empty output: with a required nonempty factor span, an
empty frame necessarily fails the accepted `K + 1` regression precondition.

## Source oracle

The local Python source was run through the required Anaconda environment on
the existing eight-row Phase 12 MKT/SMB/HML fixture.  With
`FamaFrenchModel("3factor", risk_free_rate=0.0002)`, it produced:

```text
alpha             = 0.0005694444444444665
MKT_attribution   = 0.010526060999251869
SMB_attribution   = -0.0031142789575150653
HML_attribution   = 0.0010187735138187518
specific_return   = 0.008430555555555556
common_return     = 0.0088
unexplained       = -0.00020000000000002134
```

With the same values and zero risk-free rate, all rows except
`common_return` and `unexplained` are unchanged; those rows are respectively
`0.009000000000000001` and `-2.0816681711721685e-17`.  Native finite results
use a `1e-12` tolerance against those source values, while the test suite also
proves the exact native algebra by independently multiplying the accepted
public coefficients by selected raw means.

## Scope and verification

The leaf begins with a public-header-only RED test and the complete source
oracle command.  GREEN tests prove exact string/double schema and ordinal
index, source goldens, selected-factor order, one-fit public-delegation parity,
risk-free locality, strict selector/type/index/finiteness/rank failures,
ignored metadata, and output ownership.  A later attribution-umbrella-only
C++23 consumer proves export without including a leaf or private header.

This phase excludes a model receiver, standard Fama-French factor sets,
provider/fetch/cache behavior, prediction, rolling exposures, Newey-West
statistics, p-values, residual/idiosyncratic-risk helpers, Brinson or position
attribution changes, Python dict compatibility, external numerical
dependencies, reports/releases, full Python parity, and GUI runtime.  Fresh
AppleClang, Homebrew LLVM ASan+UBSan, ImPlot compile/link-only, and
invalid-ImGui configuration evidence will be retained at acceptance; no GUI is
started.
