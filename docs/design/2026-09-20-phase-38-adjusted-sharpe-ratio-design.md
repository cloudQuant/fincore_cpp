# Phase 38 Native Adjusted Sharpe Ratio Design

## Scope decision

Phase 37 accepted native `kappa_three_ratio` at C++ commit
`722528acdabd1102594a729a2e6478ef3fad39a5`. A current-source parity pass
selects `adjusted_sharpe_ratio` as the next **valuable, public, non-alias,
dependency-closed one-column scalar** route.

The selection is deliberately narrow. At Python source HEAD
`2354bb560788f372502151a8b1b53accf536a6c0`, it is both exported directly by
`fincore.metrics.ratios.__all__` and registered as
`metrics.ratios.adjusted_sharpe_ratio` in `metrics/operations.py`. It is not a
thin forwarding alias: it combines the existing annualized Sharpe calculation
with distribution-moment correction, sample-size dampening, and a bounded
adjustment.

The Python checkout has 29 unrelated tracked/untracked governance, packaging,
and test changes. The runtime source files needed here --
`fincore/metrics/ratios.py`, `fincore/metrics/stats.py`, and
`fincore/metrics/operations.py` -- are unmodified. This is adequate evidence
for route selection, but it is not a clean-reference claim. Before native
implementation and acceptance, a fresh detached clean Python source worktree
at the pinned revision must reproduce the oracle.

The remaining direct non-alias ratio candidates are intentionally deferred:

1. **Selected -- `adjusted_sharpe_ratio`.** One returns column, one scalar
   `risk_free`, and no binary alignment, report graph, data provider, solver,
   package, target, or GUI dependency. Its moment nuance remains contained in
   `ratios.cpp`.
2. **`mar_ratio`.** It couples drawdown-before-cleaning, annualization,
   no-drawdown treatment, raw-length checks, and nonfinite result ordering.
3. **`conditional_sharpe_ratio`.** It opens percentile interpolation, tail
   membership, tail sample-size, and nonfinite cutoff behavior.
4. **`burke_ratio`, `cal_treynor_ratio`, `m_squared`, `sterling_ratio`, and
   capture-return metrics.** Each requires a wider drawdown, beta, benchmark,
   or annualization contract.

`treynor_ratio` is a Python forwarding wrapper around `cal_treynor_ratio`.
Market-correlation names are wrappers over correlation already represented by
native `safe_correlation`; `value_at_risk` maps to native historical VaR.
Report builders, legacy facade names, calendar resampling, SciPy optimization
or regression, EVT, providers, plotting, packaging, adapters, and GUI runtime
remain outside this phase. The current Python source has no
`fincore/empyrical.py`, `_registry.py`, or `report/compute.py`; Phase 38 must
not invent those obsolete routes.

## Python contract and public route

The current Python definition is:

```python
def adjusted_sharpe_ratio(returns, risk_free=0.0):
    if len(returns) < 4:
        return np.nan

    sharpe = sharpe_ratio(returns, risk_free)
    if np.isnan(sharpe):
        return np.nan

    skew = skewness(returns)
    if np.isnan(skew):
        skew = 0

    kurt = kurtosis(returns)
    if np.isnan(kurt):
        kurt = 0

    n = len(returns)
    dampening = min(1.0, n / 50.0)
    skew_adj = (skew / 6) * sharpe * dampening
    kurt_adj = (kurt / 24) * (sharpe**2) * dampening
    adjustment = 1 + skew_adj - kurt_adj
    adjustment = (
        max(0.9, min(1.1, adjustment))
        if n < 20 else max(0.8, min(1.3, adjustment))
    )
    return sharpe * adjustment
```

The exact public Python signature is
`(returns: pd.Series | np.ndarray, risk_free: float = 0.0) -> float`.
`period=` is rejected with `TypeError`; no period, annualization, output, or
compatibility parameter exists on this function. Its internal `sharpe_ratio`
uses daily annualization (`252`) and its own NaN-aware sample-standard-
deviation path. The native leaf represents that effective default behavior;
it does not introduce an unnecessary annualization parameter.

The source `skewness` and `kurtosis` wrappers call SciPy with
`nan_policy="omit"`, default `bias=True`, and Fisher excess kurtosis. Their
pre-checks use the raw Python length. Once Phase 38 has passed its raw-length
gate (`n >= 4`), SciPy computes moments using the NaN-omitted values without
the native public stats functions' stricter finite-only filtering or clean
sample gates.

The only current operation-level consumer discovered is the direct operation
table entry. There is no current flat `fincore` export, legacy facade, report
adapter, or class/instance method to extend. A future adapter, if separately
authorized, must preserve the two-argument Python signature and explicitly
bind to this native leaf; it is not part of Phase 38.

## Native API and boundaries

Add exactly one declaration to `<fincore/metrics/ratios.hpp>`:

```cpp
double adjusted_sharpe_ratio(
    const dataframe& returns,
    double risk_free = 0.0,
    std::string_view column = returns_column);
```

This is a native DataFrame scalar API, not a Python binding. It takes neither
`period` nor annualization, has no overload, automatic-input method, ndarray
or Series interface, alias, result object, facade include, CMake target,
dependency, provider, network path, report source, plot source, package, or
GUI path. `<fincore/metrics.hpp>` already reaches `ratios.hpp`; it must not
change.

The source control flow is deliberately more specific than merely calling the
public native `skewness` and `kurtosis` functions. Those public functions use
`detail::finite_values`, reject fewer than three/four *finite* values, and
replace nonfinite results with NaN. That is a different contract from Python's
NaN-only SciPy moments. In particular, four raw values with only two valid
values have a valid Python adjusted-Sharpe result; public native stats would
fall back to zero for both moments and produce the wrong result.

## Exact control flow

The implementation belongs in the existing `src/metrics/ratios.cpp` target.
It must use the existing anonymous-namespace `valid_values` helper (which
removes NaN only) and add a small private biased-moment helper in that same
translation unit. The exact route is:

```text
if column contains embedded NUL:
    throw input_error("column name must not contain an embedded NUL")

raw_values = require_double_column(returns, column)
if raw_values.size() < 4:
    return NaN

sharpe = sharpe_ratio(returns, risk_free,       # default daily annualization
                      daily_annualization, column)
if isnan(sharpe):
    return NaN                                  # only NaN, not all nonfinite

moments = private NaN-only biased moments(raw_values)
skew = isnan(moments.skew) ? 0 : moments.skew
kurt = isnan(moments.kurt) ? 0 : moments.kurt

n = raw_values.size()                           # raw count, never clean count
dampening = min(1.0, n / 50.0)
skew_adjustment = (skew / 6.0) * sharpe * dampening
kurt_adjustment = (kurt / 24.0) * (sharpe * sharpe) * dampening
adjustment = 1.0 + skew_adjustment - kurt_adjustment

if n < 20:
    adjustment = max(0.9, min(1.1, adjustment))
else:
    adjustment = max(0.8, min(1.3, adjustment))

return sharpe * adjustment
```

The embedded-NUL rejection is before every DataFrame access. It must use the
exact existing message, `column name must not contain an embedded NUL`, rather
than rely on a downstream C-string boundary. After that check,
`require_double_column` owns missing-column, wrong-type, and index/column-size
diagnostics. Structural validation therefore precedes the raw-size guard and
all numerical checks. Legal bounded non-NUL-terminated `string_view` selectors
remain supported.

`risk_free` is not separately validated. It is passed to native `sharpe_ratio`
as `sharpe_ratio(returns, risk_free, daily_annualization, column)`: explicit
daily annualization preserves the source default while the selected native
column remains intact. NaN and signed infinity are therefore observed through
the Sharpe result and take the source's early-NaN path. The implementation may
not recompute Sharpe with a different standard-deviation, annualization,
cleaning, guard, selected column, or result filter.

## Private SciPy-compatible moment helper

The helper is private to `ratios.cpp`; it must not refactor or change the
public stats API. It receives the raw `std::span<const double>`, performs
NaN-only filtering through `valid_values`, then computes population central
moments in retained row order:

```text
mean = arithmetic mean(clean values)
m2 = mean((value - mean)^2)
m3 = mean((value - mean)^3)
m4 = mean((value - mean)^4)
zero_variance = m2 <= (epsilon * mean)^2

skew = NaN if zero_variance else m3 / (m2 * sqrt(m2))
kurt = NaN if zero_variance else m4 / (m2 * m2) - 3.0
```

This is the SciPy default `bias=True`/Fisher formula used by the Python
wrappers. There is deliberately no clean-count threshold of three or four:
with raw `n >= 4`, two clean values yield skew `0` and kurtosis `-2`, and three
clean values yield their ordinary biased moments. Empty or all-NaN retained
input may produce NaN moments; the caller independently substitutes zero for
each NaN moment only after Sharpe has succeeded.

Use the `std::numeric_limits<double>::epsilon()` zero-variance comparison in
the source-like form above. Do not filter finite values, pre-emptively reject
infinity, replace nonfinite moment arithmetic, add an epsilon to a denominator,
or call `detail::finite_values`, public `skewness`, or public `kurtosis`.
Those changes would break the raw-four/clean-two and nonfinite contracts.

The ordering of the C++ clamp is semantically material. It must be expressed
as `std::max(lower, std::min(upper, adjustment))`, with `upper` as the first
argument to `std::min`. When a zero-standard-deviation input makes the Sharpe
infinite and the NaN moment fallbacks yield a NaN adjustment term, that operand
order selects the upper bound just as Python's nested `min`/`max` does. Do not
replace it with `std::clamp`, reorder operands, add a finite check, or normalize
the final result.

## Raw count, cleaning, nonfinite values, and ownership

There are three deliberately distinct observation policies:

- Fewer than four **raw** DataFrame values returns NaN before Sharpe or
  moments. Raw count also controls dampening and the `< 20` clamp family.
- Native `sharpe_ratio` separately removes NaN values and requires two clean
  values. Thus raw four with zero/one clean values is NaN, while raw four with
  two clean values remains calculable.
- The local moments remove NaN only, retain signed infinity, and do not use
  public stats clean-count gates. Moment NaN falls back to zero independently
  for skew and kurtosis.

The result preserves the Python classifications:

- all-zero returns give NaN because Sharpe is NaN before moment fallback;
- positive and negative constant returns give respectively `+inf` and `-inf`:
  the source's NaN moment terms choose the appropriate upper clamp and do not
  turn a nonfinite Sharpe into NaN;
- signed-infinite returns or `risk_free` values yield NaN when native Sharpe is
  NaN; and
- no warning channel, exception translation, finite result clamp, or signed-
  zero normalization is added.

The operation reads raw values in DataFrame row order. It neither sorts nor
aligns indices, deduplicates labels, mutates its inputs, or retains a span,
selector, index, or DataFrame storage after returning. Duplicate/nonmonotonic
timestamp labels therefore matter only through their associated value order.

## Current-source selection oracle and provenance boundary

The initial 40-line route-selection capture is retained at
`/tmp/fincore-phase38-adjusted-sharpe-oracle.K7lK3N`. It ran with the mandatory
Anaconda interpreter, exited zero, and wrote zero stderr bytes. Its script and
raw-output SHA-256 values are respectively
`684349c8b5ae74c8be7390933208a7b4294c4e420aa420adaf395056d4d2b00f` and
`633743478911f4bc44bd144cdde0234f559b5924a88464be283b28cae28bf25c`.

The capture records the dirty checkout's CWD, HEAD, and clean target-file
status, but its original script did not print `fincore.__file__`; it is
therefore route-selection evidence, not a clean-source acceptance proof. The
same command was replayed under explicit
`PYTHONPATH=/Users/yunjinqi/Documents/new_projects/fincore` and byte-compared
cleanly against the retained 40 lines. The durable replay root
`/tmp/fincore-phase38-adjusted-sharpe-replay.ESIGce` retains the exact command
manifest, zero-byte stderr and diff, and the same raw-output SHA-256
`633743478911f4bc44bd144cdde0234f559b5924a88464be283b28cae28bf25c`.
Task 161 still must reproduce the full oracle from a detached clean root and
assert the imported module path.

The complete output is specification evidence for the intended source contract;
warning categories and text are Python observations only, not C++ diagnostics:

```text
signature=(returns: 'pd.Series | np.ndarray', risk_free: 'float' = 0.0) -> 'float'
period_kw=EXC:TypeError:adjusted_sharpe_ratio() got an unexpected keyword argument 'period'; warnings=0; messages=-
ordinary=2.179402814215353; hex=0x1.16f6abe1eec25p+1; warnings=0; messages=-
ordinary_risk_free_0.001=1.637451311586237; hex=0x1.a33002580e213p+0; warnings=0; messages=-
raw_n0=nan; warnings=0; messages=-
raw_n1=nan; warnings=0; messages=-
raw_n2=nan; warnings=0; messages=-
raw_n3=nan; warnings=0; messages=-
raw_n4=1.8289412324150214; hex=0x1.d4357e1b8748ap+0; warnings=0; messages=-
raw4_valid0=nan; warnings=0; messages=-
raw4_valid1=nan; warnings=0; messages=-
raw4_valid2=-4.090878742872842; hex=-0x1.05d0f5132296cp+2; warnings=0; messages=-
raw4_valid3=4.520724794501948; hex=0x1.21538e16a68a4p+2; warnings=0; messages=-
raw10_all_nan=nan; warnings=0; messages=-
upper_n19=33.1438443915704; hex=0x1.092697e36c102p+5; warnings=0; messages=-
upper_n19_control=n=19; dampening=0.38; base=30.130767628700358; hex=0x1.e2179fcc0a4bdp+4; skew=0.10540925533894553; hex=0x1.afc19d8606149p-4; kurt=-1.988888888888889; hex=-0x1.fd27d27d27d28p+0; raw_adjustment=29.79043391264038; clamp=[0.9,1.1]; multiplier=1.1
lower_n19=1.758752806478678; hex=0x1.c23d9fb992facp+0; warnings=0; messages=-
lower_n19_control=n=19; dampening=0.38; base=1.9541697849763089; hex=0x1.f44478954dfa3p+0; skew=4.006938426723769; hex=0x1.0071addef7506p+2; kurt=14.055555555555554; hex=0x1.c1c71c71c71c6p+3; raw_adjustment=0.6460598286778145; clamp=[0.9,1.1]; multiplier=0.8999999999999999
upper_n20=40.22864650967021; hex=0x1.41d4449f0b066p+5; warnings=0; messages=-
upper_n20_control=n=20; dampening=0.4; base=30.945112699746318; hex=0x1.ef1f2e7e871d8p+4; skew=-3.176373552203626e-16; hex=-0x1.6e35fffffffffp-52; kurt=-2.0000000000000004; hex=-0x1.0000000000001p+1; raw_adjustment=32.92000000000004; clamp=[0.8,1.3]; multiplier=1.3
lower_n20=1.4544898588693689; hex=0x1.745972883524fp+0; warnings=0; messages=-
lower_n20_control=n=20; dampening=0.4; base=1.818112323586711; hex=0x1.d16fcf2a426e2p+0; skew=4.12948320967011; hex=0x1.084973f1ba82dp+2; kurt=15.052631578947366; hex=0x1.e1af286bca1aep+3; raw_adjustment=0.6712415924321031; clamp=[0.8,1.3]; multiplier=0.8
upper_n49=40.44197112618808; hex=0x1.4389282866086p+5; warnings=0; messages=-
upper_n49_control=n=49; dampening=0.98; base=31.109208558606216; hex=0x1.f1bf51793a831p+4; skew=0.04082482904638668; hex=0x1.4e6fdf33cf068p-5; kurt=-1.9983333333333333; hex=-0x1.ff92c5f92c5f9p+0; raw_adjustment=80.17717492645842; clamp=[0.8,1.3]; multiplier=1.3
lower_n49=-0.3539959942330339; hex=-0x1.6a7ded089562fp-2; warnings=0; messages=-
lower_n49_control=n=49; dampening=0.98; base=-0.44249499279129234; hex=-0x1.c51d684ababbap-2; skew=6.783865662978101; hex=0x1.b22adae2bcb10p+2; kurt=44.02083333333332; hex=0x1.602aaaaaaaaa9p+5; raw_adjustment=0.15774448900051596; clamp=[0.8,1.3]; multiplier=0.8
upper_n50=40.85889866357148; hex=0x1.46df064334f0ep+5; warnings=0; messages=-
upper_n50_control=n=50; dampening=1.0; base=31.42992204890114; hex=0x1.f6e0f5f13dc16p+4; skew=-3.5151867311053464e-16; hex=-0x1.9546000000000p-52; kurt=-2.0; hex=-0x1.0000000000000p+1; raw_adjustment=83.32000000000016; clamp=[0.8,1.3]; multiplier=1.2999999999999998
lower_n50=-0.3942429246552071; hex=-0x1.93b46ad04b455p-2; warnings=0; messages=-
lower_n50_control=n=50; dampening=1.0; base=-0.49280365581900887; hex=-0x1.f8a185845e16ap-2; skew=6.857142857142859; hex=0x1.b6db6db6db6dep+2; kurt=45.020408163265316; hex=0x1.6829cbc14e5e2p+5; raw_adjustment=-0.018764643788057922; clamp=[0.8,1.3]; multiplier=0.8
zero_std_zero=nan; warnings=0; messages=-
zero_std_positive=inf; warnings=4; messages=RuntimeWarning:Precision loss occurred in moment calculation due to catastrophic cancellation. This occurs when the data are nearly identical. Results may be unreliable. | RuntimeWarning:Precision loss occurred in moment calculation due to catastrophic cancellation. This occurs when the data are nearly identical. Results may be unreliable. | RuntimeWarning:Precision loss occurred in moment calculation due to catastrophic cancellation. This occurs when the data are nearly identical. Results may be unreliable. | RuntimeWarning:Precision loss occurred in moment calculation due to catastrophic cancellation. This occurs when the data are nearly identical. Results may be unreliable.
zero_std_negative=-inf; warnings=4; messages=RuntimeWarning:Precision loss occurred in moment calculation due to catastrophic cancellation. This occurs when the data are nearly identical. Results may be unreliable. | RuntimeWarning:Precision loss occurred in moment calculation due to catastrophic cancellation. This occurs when the data are nearly identical. Results may be unreliable. | RuntimeWarning:Precision loss occurred in moment calculation due to catastrophic cancellation. This occurs when the data are nearly identical. Results may be unreliable. | RuntimeWarning:Precision loss occurred in moment calculation due to catastrophic cancellation. This occurs when the data are nearly identical. Results may be unreliable.
zero_std_positive_moments=skew:nan; kurt:nan; warnings=4; messages=RuntimeWarning:Precision loss occurred in moment calculation due to catastrophic cancellation. This occurs when the data are nearly identical. Results may be unreliable. | RuntimeWarning:Precision loss occurred in moment calculation due to catastrophic cancellation. This occurs when the data are nearly identical. Results may be unreliable. | RuntimeWarning:Precision loss occurred in moment calculation due to catastrophic cancellation. This occurs when the data are nearly identical. Results may be unreliable. | RuntimeWarning:Precision loss occurred in moment calculation due to catastrophic cancellation. This occurs when the data are nearly identical. Results may be unreliable.
positive_inf=nan; warnings=0; messages=-
negative_inf=nan; warnings=0; messages=-
both_inf=nan; warnings=0; messages=-
risk_free_nan=nan; warnings=0; messages=-
risk_free_pos_inf=nan; warnings=0; messages=-
risk_free_neg_inf=nan; warnings=0; messages=-
```

Two supplemental current-source cases were independently retained at
`/tmp/fincore-phase38-adjusted-sharpe-supplement.FRF96x` after explicitly
setting `PYTHONPATH` to the current source checkout. The script and raw output
SHA-256 values are respectively
`ddecef6a66a438a8be2646b830ce3ff6fa265085dbac21cd62f088e00c27e823` and
`05945a7b0232f4f3f3222c515f99cd722fda1ba4b88b6136d9f2ecf3c9932a8b`.
They are route-selection evidence from the same dirty-but-target-files-clean
checkout, not a substitute for the fresh clean oracle required by the plan:

```text
fincore_file=/Users/yunjinqi/Documents/new_projects/fincore/fincore/__init__.py
near_zero_variance=8.755998418330334e+16; hex=0x1.37135779c8dcbp+56; warnings=4; messages=RuntimeWarning:Precision loss occurred in moment calculation due to catastrophic cancellation. This occurs when the data are nearly identical. Results may be unreliable. | RuntimeWarning:Precision loss occurred in moment calculation due to catastrophic cancellation. This occurs when the data are nearly identical. Results may be unreliable. | RuntimeWarning:Precision loss occurred in moment calculation due to catastrophic cancellation. This occurs when the data are nearly identical. Results may be unreliable. | RuntimeWarning:Precision loss occurred in moment calculation due to catastrophic cancellation. This occurs when the data are nearly identical. Results may be unreliable.
raw20_clean2=-4.864154602806124; hex=-0x1.374e4f1b6f5f0p+2; warnings=0; messages=-
raw20_clean2_control=n=20; clean=2; base=-3.7416573867739413; hex=-0x1.deeea11683f49p+1; multiplier=1.3
```

The ordinary fixture is `[.01, .02, -.05, .03, .01, -.02, .04, .01, .02,
-.03]`; the raw-four fixture is `[.01, -.02, .03, -.01]`. The four clamp
fixtures use `resize([.01, .03], n)` and `[-.001] * (n - 1) + [.04]` for raw
counts 19, 20, 49, and 50. The supplemental fixtures are respectively
`[1.0, nextafter(1.0, 2.0), 1.0, nextafter(1.0, 2.0)]` and eighteen NaNs
followed by `[.01, -.02]`. Together they distinguish SciPy's nonconstant
near-zero `m2` rule, both clamp families, raw-versus-clean count, and the
dampening boundary.

The nonfinite tests use exactly `[+inf, .01, -.02, .03]`,
`[-inf, .01, -.02, .03]`, and `[+inf, -inf, .01, -.02]`, each with raw count
four. The order test uses duplicate/nonmonotonic labels with ordered values
`[-1e16, 1.0, 1e16, 1.0]` and a reordered equivalent multiset
`[1.0, 1e16, 1.0, -1e16]`; the source Sharpe/adjusted results are respectively
approximately `4.860555523805895e-16` and `0.0`. This proves the native path
may not sort or otherwise reorder raw values.

## Mutation-resistant tests

Both registered C++ test translation units must lock the exact function pointer
type `double (*)(const dataframe&, double, std::string_view)` with
`std::is_same_v<decltype(&fincore::adjusted_sharpe_ratio), signature>` -- not
a `static_cast` that could permit forbidden overloads -- and a default call on
at least four finite values with a finite expected result. The ratio leaf must
retain `<fincore/metrics/ratios.hpp>` as its only fincore include; the umbrella
test proves the existing façade inclusion path.

The test matrix must cover:

- ordinary and nonzero-risk-free goldens;
- raw sizes 0 through 4, all-NaN raw ten, and raw four with zero, one, two,
  and three clean values; notably `raw4_valid2` and `raw4_valid3` must assert
  their oracle values to reject accidental reuse of native public stats;
- a nonconstant near-zero-variance source fixture
  `[1.0, nextafter(1.0, 2.0), 1.0, nextafter(1.0, 2.0)]`; its moment fallback
  must leave adjusted Sharpe equal to base Sharpe rather than incorrectly
  apply a 1.1 clamp;
- all four raw-count clamp boundaries (upper and lower at 19/20/49/50), with
  the expected multipliers and a tolerance appropriate only for finite
  reduction differences;
- raw twenty with eighteen NaNs followed by `[.01, -.02]`, proving that the
  `>= 20` clamp and dampening use raw count rather than the clean count;
- zero, positive constant, and negative constant inputs, asserting exact
  NaN/+infinity/-infinity classifications;
- the oracle's four-value signed-infinite return inputs plus
  NaN/+infinity/-infinity `risk_free`, so a raw-size guard cannot cause a
  false pass;
- default, named, bounded non-NUL-terminated selectors; the named and bounded
  cases must use a valid custom column on a frame with no `returns` column, so
  a future accidental two-argument Sharpe call cannot silently pass;
  duplicate/nonmonotonic labels and a four-value cancellation-order fixture
  whose ordered and reordered inputs are observably distinct; and scalar
  ownership after local input/selector storage is destroyed;
- NUL-before-DataFrame precedence, including a valid prefix and an otherwise
  malformed frame; then empty/missing selector, wrong type, and index/value
  mismatch structural diagnostics; and
- no change to the public native stats contracts, CMake graph, dependencies,
  sources, package, provider, report, plot, or GUI behavior.

## Exact implementation and acceptance scope

Task 161 may change exactly these four already-registered paths:

- `include/fincore/metrics/ratios.hpp`;
- `src/metrics/ratios.cpp`;
- `tests/unit/ratios_test.cpp`; and
- `tests/unit/metrics_umbrella_test.cpp`.

No new target, source list, include facade, dependency, benchmark, adapter, or
test file is permitted. Acceptance requires a clean detached Python oracle, a
genuine dual-consumer RED before declaration/definition, current-tree build
and CTest gates, fresh AppleClang headless evidence, fresh LLVM ASan+UBSan
evidence, ImGui/ImPlot/demo compile/link-only evidence without demo execution
or GUI launch, and a separate invalid-ImGui expected failure. The phase does
not authorize merge, rebase, push, release, live data, provider calls, or GUI
runtime validation.
