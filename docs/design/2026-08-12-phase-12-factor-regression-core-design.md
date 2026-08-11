# Phase 12 Factor Regression Core Design

**Date:** 2026-08-12

## Decision

Phase 12 adds a deterministic, DataFrame-native ordinary-least-squares factor
regression leaf based on the fitting core of Python
`fincore.attribution.fama_french.FamaFrenchModel`.  It deliberately names the
native surface *factor regression*, rather than claiming a full Fama-French
model: callers explicitly supply the factors, while standard factor sets,
providers, statistical inference, stateful prediction, and rolling analysis
remain separate work.

The native boundary takes one selected return column, a factor-return
DataFrame, and an explicit ordered factor-name span.  It uses a private
Householder-QR least-squares solve (no new math dependency) for

```text
return[t] = alpha + beta[0] * factor[0][t] + ... + beta[K-1] * factor[K-1][t]
```

and owns three tabular outputs: an ordinal-indexed coefficient table and two
return-indexed fitted/residual tables.  A direct Anaconda oracle using Python's
actual 3-factor `FamaFrenchModel(...).fit(..., newey_west_lags=0)` establishes
the finite alpha/beta/R-squared/residual goldens.  The public implementation
does not reuse Python's pandas positional behavior: it applies the established
native strict same-index contract before fitting.

## Public shape

The leaf header will be
`include/fincore/attribution/factor_regression.hpp`; the existing public
`include/fincore/attribution.hpp` umbrella will later include this leaf next to
Brinson.

```cpp
inline constexpr std::string_view factor_regression_factor_column{"factor"};
inline constexpr std::string_view factor_regression_coefficient_column{
    "coefficient"};
inline constexpr std::string_view factor_regression_fitted_column{
    "fitted_returns"};
inline constexpr std::string_view factor_regression_residual_column{
    "residuals"};
inline constexpr std::string_view factor_regression_alpha_name{"alpha"};

struct factor_regression_result {
    double alpha;
    double r_squared;
    dataframe coefficients;
    dataframe fitted_returns;
    dataframe residuals;
};

factor_regression_result fit_factor_regression(
    const dataframe& returns,
    const dataframe& factor_returns,
    std::span<const std::string_view> factor_column_names,
    std::string_view returns_column_name = returns_column);
```

`coefficients` has synthetic ordinal index `0..K`, one genuine `std::string`
`factor` column and one genuine `double` `coefficient` column.  Its rows are
exactly `alpha` first, then caller-supplied factors in caller-supplied order.
`fitted_returns` and `residuals` retain the raw returns timestamp index and
each have exactly one genuine-`double` named `fitted_returns` or `residuals`.
The result's scalar `alpha` duplicates coefficient row zero for convenient C++
consumption; `r_squared` is the population coefficient of determination.

## Boundary, solve, and numerical behavior

The requested factor span must be nonempty, contain no empty/duplicate names,
and must not contain the reserved `alpha` label.  The selected return and all
selected factor columns must exist, be genuine `double`, and have exact owning
index length; unselected metadata, including malformed numeric metadata, is
ignored.  Both raw indexes must be duplicate-free and exactly equal in length,
value, and order.  The native surface neither positional-zips mismatched
frames nor drops/reorders timestamps.

All selected values must be finite; NaN and signed infinity are rejected as an
input error rather than filtered or silently propagated into a numerical solve.
At least `K + 1` observations are required.  A rank-deficient design,
detected by the Householder-QR pivot tolerance relative to the input matrix
scale, throws `fincore::input_error`; the implementation does not return an
arbitrary minimum-norm solution.  It returns an exact-fit result when
observations equal coefficients, as no standard-error degrees of freedom are
being claimed.

The solver adds an intercept column of ones, computes coefficients through
Householder QR, then generates fitted values and residuals in original row
order.  It calculates `r_squared = 1 - SSE/SST` when `SST > 0`; a constant
return vector has `r_squared == 0.0`, matching the current Python fit path.
No Newey-West adjustment, standard errors, p-values, weighting, GLS, risk-free
adjustment, factor fetching/caching, model state, prediction, rolling
exposures, attribution decomposition, or idiosyncratic-risk convenience
function is included.

## Verification and exclusions

The leaf must first have a public-header-only RED test, then use the mandatory
Anaconda Fama-French 3-factor oracle.  Tests must prove exact output table
schemas and raw index retention; standard factor ordering and an explicitly
permuted factor selection; finite goldens; coefficient/fitted/residual identity;
constant-return R-squared; exact fit; input type/index/length/name failures;
nonfinite rejection; rank deficiency; unrelated metadata isolation; and output
ownership.  An umbrella-only C++23 consumer proves that the attribution entry
exports both Brinson and factor regression without depending on private source
headers.

Phase acceptance uses retained terminal logs for fresh AppleClang headless,
Homebrew LLVM ASan+UBSan headless, ImPlot/demo compile-link-only, and invalid
ImGui configuration gates.  The phase does not prove full Fama-French parity,
provider integration, statistical inference, release readiness, or GUI runtime
behavior.
