import math
import os
import sys
import warnings
from pathlib import Path

import numpy as np
import pandas as pd

import fincore

src = Path(os.environ["FINCORE_PHASE38_PYTHON_SOURCE"]).resolve()
module_file = Path(fincore.__file__).resolve()
print(f"fincore_file={module_file}")
print(f"fincore_source_root={src}")
print("sys_path_head=" + "|".join(sys.path[:2]))
assert str(module_file).startswith(str(src) + os.sep), (module_file, src)

from fincore.metrics.ratios import adjusted_sharpe_ratio, sharpe_ratio
from fincore.metrics.stats import kurtosis, skewness

failures = []


def fmt(value):
    value = float(value)
    if math.isnan(value):
        return "nan", "nan"
    if math.isinf(value):
        text = "inf" if value > 0 else "-inf"
        return text, text
    return repr(value), value.hex()


def run(name, fn, expected_hex=None, expected_repr=None,
        expected_class=None, controls=None):
    with warnings.catch_warnings(record=True) as caught:
        warnings.simplefilter("always")
        value = fn()
    text, hexed = fmt(value)
    messages = [f"{w.category.__name__}:{w.message}" for w in caught]
    print(f"{name}={text}; hex={hexed}; warnings={len(caught)}; "
          f"messages={' | '.join(messages) if messages else '-'}")
    if expected_hex is not None and hexed != expected_hex:
        failures.append(f"{name}: hex {hexed} != {expected_hex}")
    if expected_repr is not None and text != expected_repr:
        failures.append(f"{name}: repr {text} != {expected_repr}")
    if expected_class is not None and text != expected_class:
        failures.append(f"{name}: class {text} != {expected_class}")
    for key, (got, want) in (controls or {}).items():
        got_text, got_hex = fmt(got)
        want_text, want_hex = fmt(want)
        if got_hex != want_hex:
            failures.append(f"{name}.{key}: {got_hex} != {want_hex}")
    return value


ordinary = [0.01, 0.02, -0.05, 0.03, 0.01, -0.02, 0.04, 0.01, 0.02, -0.03]
raw4 = [0.01, -0.02, 0.03, -0.01]

import inspect
print(f"signature={inspect.signature(adjusted_sharpe_ratio)}")
try:
    adjusted_sharpe_ratio(pd.Series(ordinary), period=252)
    print("period_kw=NOERROR")
    failures.append("period_kw accepted")
except TypeError as exc:
    print(f"period_kw=EXC:TypeError:{exc}")

run("ordinary", lambda: adjusted_sharpe_ratio(pd.Series(ordinary)),
    expected_hex="0x1.16f6abe1eec25p+1")
run("ordinary_risk_free_0.001",
    lambda: adjusted_sharpe_ratio(pd.Series(ordinary), 0.001),
    expected_hex="0x1.a33002580e213p+0")

for n in range(5):
    run(f"raw_n{n}", lambda n=n: adjusted_sharpe_ratio(np.array(raw4[:n])),
        expected_class="nan" if n < 4 else None,
        expected_hex="0x1.d4357e1b8748ap+0" if n == 4 else None)

nan = float("nan")
valid_cases = {
    "raw4_valid0": [nan, nan, nan, nan],
    "raw4_valid1": [nan, nan, nan, 0.01],
    "raw4_valid2": [nan, nan, 0.01, -0.02],
    "raw4_valid3": [nan, 0.01, -0.02, 0.03],
}
run("raw4_valid0", lambda: adjusted_sharpe_ratio(np.array(valid_cases["raw4_valid0"])),
    expected_class="nan")
run("raw4_valid1", lambda: adjusted_sharpe_ratio(np.array(valid_cases["raw4_valid1"])),
    expected_class="nan")
run("raw4_valid2", lambda: adjusted_sharpe_ratio(np.array(valid_cases["raw4_valid2"])),
    expected_hex="-0x1.05d0f5132296cp+2")
run("raw4_valid3", lambda: adjusted_sharpe_ratio(np.array(valid_cases["raw4_valid3"])),
    expected_hex="0x1.21538e16a68a4p+2")
run("raw10_all_nan", lambda: adjusted_sharpe_ratio(np.array([nan] * 10)),
    expected_class="nan")

def resize(pattern, n):
    out = []
    while len(out) < n:
        out.append(pattern[len(out) % len(pattern)])
    return out

clamp_cases = {
    19: (resize([0.01, 0.03], 19), [-0.001] * 18 + [0.04]),
    20: (resize([0.01, 0.03], 20), [-0.001] * 19 + [0.04]),
    49: (resize([0.01, 0.03], 49), [-0.001] * 48 + [0.04]),
    50: (resize([0.01, 0.03], 50), [-0.001] * 49 + [0.04]),
}
clamp_expect = {
    19: ("upper", "0x1.092697e36c102p+5", "0x1.e2179fcc0a4bdp+4",
         "0x1.afc19d8606149p-4", "-0x1.fd27d27d27d28p+0",
         "29.79043391264038", 1.1),
    20: ("upper", "0x1.41d4449f0b066p+5", "0x1.ef1f2e7e871d8p+4",
         "-0x1.6e35fffffffffp-52", "-0x1.0000000000001p+1",
         "32.92000000000004", 1.3),
    49: ("upper", "0x1.4389282866086p+5", "0x1.f1bf51793a831p+4",
         "0x1.4e6fdf33cf068p-5", "-0x1.ff92c5f92c5f9p+0",
         "80.17717492645842", 1.3),
    50: ("upper", "0x1.46df064334f0ep+5", "0x1.f6e0f5f13dc16p+4",
         "-0x1.9546000000000p-52", "-0x1.0000000000000p+1",
         "83.32000000000016", 1.2999999999999998),
}
for n, (upper_values, lower_values) in clamp_cases.items():
    _, want_hex, base_hex, skew_hex, kurt_hex, raw_repr, multiplier = clamp_expect[n]
    base = sharpe_ratio(np.array(upper_values))
    skew = skewness(np.array(upper_values))
    kurt = kurtosis(np.array(upper_values))
    dampening = min(1.0, n / 50.0)
    raw_adjustment = 1 + (skew / 6) * base * dampening - \
        (kurt / 24) * (base ** 2) * dampening
    lo, hi = (0.9, 1.1) if n < 20 else (0.8, 1.3)
    adjustment = max(lo, min(hi, raw_adjustment))
    print(f"upper_n{n}_control=n={n}; dampening={dampening}; "
          f"base={fmt(base)[0]}; hex={fmt(base)[1]}; "
          f"skew={fmt(skew)[0]}; hex={fmt(skew)[1]}; "
          f"kurt={fmt(kurt)[0]}; hex={fmt(kurt)[1]}; "
          f"raw_adjustment={repr(float(raw_adjustment))}; "
          f"clamp=[{lo},{hi}]; multiplier={repr(float(adjustment))}")
    if fmt(base)[1] != base_hex:
        failures.append(f"upper_n{n} base {fmt(base)[1]} != {base_hex}")
    if fmt(skew)[1] != skew_hex:
        failures.append(f"upper_n{n} skew {fmt(skew)[1]} != {skew_hex}")
    if fmt(kurt)[1] != kurt_hex:
        failures.append(f"upper_n{n} kurt {fmt(kurt)[1]} != {kurt_hex}")
    if repr(float(raw_adjustment)) != raw_repr:
        failures.append(f"upper_n{n} raw_adjustment drift")
    if abs(float(adjustment) - multiplier) > 1e-15:
        failures.append(f"upper_n{n} multiplier drift")
    run(f"upper_n{n}", lambda v=upper_values: adjusted_sharpe_ratio(np.array(v)),
        expected_hex=want_hex)

lower_expect = {
    19: ("lower_n19", "0x1.c23d9fb992facp+0", "0x1.f44478954dfa3p+0",
         "0x1.0071addef7506p+2", "0x1.c1c71c71c71c6p+3",
         "0.6460598286778145", 0.8999999999999999),
    20: ("lower_n20", "0x1.745972883524fp+0", "0x1.d16fcf2a426e2p+0",
         "0x1.084973f1ba82dp+2", "0x1.e1af286bca1aep+3",
         "0.6712415924321031", 0.8),
    49: ("lower_n49", "-0x1.6a7ded089562fp-2", "-0x1.c51d684ababbap-2",
         "0x1.b22adae2bcb10p+2", "0x1.602aaaaaaaaa9p+5",
         "0.15774448900051596", 0.8),
    50: ("lower_n50", "-0x1.93b46ad04b455p-2", "-0x1.f8a185845e16ap-2",
         "0x1.b6db6db6db6dep+2", "0x1.6829cbc14e5e2p+5",
         "-0.018764643788057922", 0.8),
}
for n, (upper_values, lower_values) in clamp_cases.items():
    _, want_hex, base_hex, skew_hex, kurt_hex, raw_repr, multiplier = lower_expect[n]
    base = sharpe_ratio(np.array(lower_values))
    skew = skewness(np.array(lower_values))
    kurt = kurtosis(np.array(lower_values))
    dampening = min(1.0, n / 50.0)
    raw_adjustment = 1 + (skew / 6) * base * dampening - \
        (kurt / 24) * (base ** 2) * dampening
    lo, hi = (0.9, 1.1) if n < 20 else (0.8, 1.3)
    adjustment = max(lo, min(hi, raw_adjustment))
    print(f"lower_n{n}_control=n={n}; dampening={dampening}; "
          f"base={fmt(base)[0]}; hex={fmt(base)[1]}; "
          f"skew={fmt(skew)[0]}; hex={fmt(skew)[1]}; "
          f"kurt={fmt(kurt)[0]}; hex={fmt(kurt)[1]}; "
          f"raw_adjustment={repr(float(raw_adjustment))}; "
          f"clamp=[{lo},{hi}]; multiplier={repr(float(adjustment))}")
    if fmt(base)[1] != base_hex:
        failures.append(f"lower_n{n} base {fmt(base)[1]} != {base_hex}")
    if fmt(skew)[1] != skew_hex:
        failures.append(f"lower_n{n} skew {fmt(skew)[1]} != {skew_hex}")
    if fmt(kurt)[1] != kurt_hex:
        failures.append(f"lower_n{n} kurt {fmt(kurt)[1]} != {kurt_hex}")
    if repr(float(raw_adjustment)) != raw_repr:
        failures.append(f"lower_n{n} raw_adjustment drift")
    if abs(float(adjustment) - multiplier) > 1e-15:
        failures.append(f"lower_n{n} multiplier drift")
    run(f"lower_n{n}", lambda v=lower_values: adjusted_sharpe_ratio(np.array(v)),
        expected_hex=want_hex)

run("zero_std_zero", lambda: adjusted_sharpe_ratio(np.array([0.0] * 4)),
    expected_class="nan")
run("zero_std_positive", lambda: adjusted_sharpe_ratio(np.array([0.01] * 4)),
    expected_class="inf")
run("zero_std_negative", lambda: adjusted_sharpe_ratio(np.array([-0.01] * 4)),
    expected_class="-inf")
with warnings.catch_warnings(record=True) as caught:
    warnings.simplefilter("always")
    pos_skew = skewness(np.array([0.01] * 4))
    pos_kurt = kurtosis(np.array([0.01] * 4))
print(f"zero_std_positive_moments=skew:{fmt(pos_skew)[0]}; "
      f"kurt:{fmt(pos_kurt)[0]}; warnings={len(caught)}")

inf = float("inf")
run("positive_inf", lambda: adjusted_sharpe_ratio(np.array([inf, 0.01, -0.02, 0.03])),
    expected_class="nan")
run("negative_inf", lambda: adjusted_sharpe_ratio(np.array([-inf, 0.01, -0.02, 0.03])),
    expected_class="nan")
run("both_inf", lambda: adjusted_sharpe_ratio(np.array([inf, -inf, 0.01, -0.02])),
    expected_class="nan")
run("risk_free_nan", lambda: adjusted_sharpe_ratio(np.array(raw4), nan),
    expected_class="nan")
run("risk_free_pos_inf", lambda: adjusted_sharpe_ratio(np.array(raw4), inf),
    expected_class="nan")
run("risk_free_neg_inf", lambda: adjusted_sharpe_ratio(np.array(raw4), -inf),
    expected_class="nan")

near_zero = [1.0, np.nextafter(1.0, 2.0), 1.0, np.nextafter(1.0, 2.0)]
run("near_zero_variance", lambda: adjusted_sharpe_ratio(np.array(near_zero)),
    expected_hex="0x1.37135779c8dcbp+56")

raw20_clean2 = [nan] * 18 + [0.01, -0.02]
run("raw20_clean2", lambda: adjusted_sharpe_ratio(np.array(raw20_clean2)),
    expected_hex="-0x1.374e4f1b6f5f0p+2")
base20 = sharpe_ratio(np.array(raw20_clean2))
print(f"raw20_clean2_control=n=20; clean=2; base={fmt(base20)[0]}; "
      f"hex={fmt(base20)[1]}; multiplier=1.3")

ordered = [-1e16, 1.0, 1e16, 1.0]
reordered = [1.0, 1e16, 1.0, -1e16]
ordered_value = run("order_ordered", lambda: adjusted_sharpe_ratio(np.array(ordered)))
reordered_value = run("order_reordered",
                      lambda: adjusted_sharpe_ratio(np.array(reordered)))
print(f"order_distinct={ordered_value != reordered_value}")
if ordered_value == reordered_value:
    failures.append("order fixtures are not distinct")
with warnings.catch_warnings(record=True) as caught:
    warnings.simplefilter("always")
    ordered_base = sharpe_ratio(np.array(ordered))
print(f"order_ordered_base={fmt(ordered_base)[0]}; hex={fmt(ordered_base)[1]}; "
      f"warnings={len(caught)}")

print(f"FAILURES={len(failures)}")
for failure in failures:
    print(f"FAIL: {failure}")
sys.exit(1 if failures else 0)
