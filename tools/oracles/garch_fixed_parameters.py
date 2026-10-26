#!/usr/bin/env python
"""Record fixed-parameter GARCH-family references from the pinned Python source.

The pinned Python methods supply the implementation-side reference; Decimal
recurrences independently check their initialization, index-1 likelihood
window, and arithmetic. This does not compare optimizers or claim fit parity.
"""

from __future__ import annotations

import argparse
from decimal import Decimal, localcontext
import hashlib
import importlib.util
import json
import locale
import os
from pathlib import Path
import platform
import subprocess
import sys
import time
import warnings

import numpy as np
import scipy


PINNED_SOURCE_SHA256 = "65a39aaf6e39c8fe06366a38060a2257dc586cadd1ab421d0fa60443fc4e4c23"
PINNED_SOURCE_COMMIT = "4b8f31b1b59707480d5db9603704d83bbf7e6946"
PINNED_SOURCE_BLOB = "74581d71e9d45e632b504ff887a345ca0ce1679e"
ABS_TOL = 1e-12
REL_TOL = 1e-10
OBSERVATIONS = (0.03, -0.02, 0.015, -0.04, 0.01, 0.025, -0.035, 0.005)
PI = Decimal("3.14159265358979323846264338327950288419716939937510")

PARAMETERS = {
    "garch11": {"omega": 0.0002, "alpha": 0.12, "beta": 0.76},
    "egarch": {"omega": -0.1, "alpha": 0.2, "gamma": -0.15, "beta": 0.85},
    "gjr_garch": {"omega": 0.0002, "alpha": 0.08, "gamma": 0.12, "beta": 0.74},
}


def decimal(value: float) -> Decimal:
    return Decimal(str(value))


def decimal_variance(values: tuple[float, ...]) -> Decimal:
    values_decimal = tuple(decimal(value) for value in values)
    mean = sum(values_decimal, Decimal(0)) / Decimal(len(values_decimal))
    return sum(((value - mean) ** 2 for value in values_decimal), Decimal(0)) / Decimal(
        len(values_decimal)
    )


def decimal_reference(model: str) -> tuple[list[float], float]:
    with localcontext() as context:
        context.prec = 60
        values = tuple(decimal(value) for value in OBSERVATIONS)
        params = {key: decimal(value) for key, value in PARAMETERS[model].items()}
        variances = [decimal_variance(OBSERVATIONS)]

        for index in range(1, len(values)):
            previous = values[index - 1]
            if model == "garch11":
                current = (
                    params["omega"]
                    + params["alpha"] * previous**2
                    + params["beta"] * variances[index - 1]
                )
            elif model == "gjr_garch":
                indicator = Decimal(1) if previous < 0 else Decimal(0)
                current = (
                    params["omega"]
                    + params["alpha"] * previous**2
                    + params["gamma"] * indicator * previous**2
                    + params["beta"] * variances[index - 1]
                )
            else:
                z_previous = previous / variances[index - 1].sqrt()
                log_variance = (
                    params["omega"]
                    + params["alpha"] * abs(z_previous)
                    + params["gamma"] * z_previous
                    + params["beta"] * variances[index - 1].ln()
                )
                current = log_variance.exp()
            variances.append(current)

        total = Decimal(0)
        for index in range(1, len(values)):
            total += (Decimal(2) * PI * variances[index]).ln()
            if model == "egarch":
                total += (values[index] / variances[index].sqrt()) ** 2
            else:
                total += values[index] ** 2 / variances[index]
        log_likelihood = -Decimal("0.5") * total
        return [float(value) for value in variances], float(log_likelihood)


def locate_source(source: Path) -> Path:
    resolved = source.resolve()
    if not resolved.is_file():
        raise FileNotFoundError(f"Pinned Python source does not exist: {resolved}")
    return resolved


def git_output(source: Path, *args: str) -> str:
    return subprocess.check_output(
        ["git", "-C", str(source.parent), *args], text=True
    ).strip()


def verify_source_git_identity(source: Path) -> dict[str, str]:
    root = git_output(source, "rev-parse", "--show-toplevel")
    head = git_output(source, "rev-parse", "HEAD")
    status_before = git_output(source, "status", "--porcelain")
    blob = git_output(
        source,
        "rev-parse",
        f"{PINNED_SOURCE_COMMIT}:fincore/risk/garch.py",
    )
    working_blob = git_output(source, "hash-object", str(source))
    if head != PINNED_SOURCE_COMMIT:
        raise ValueError(
            f"Pinned source checkout HEAD mismatch: expected {PINNED_SOURCE_COMMIT}, got {head}"
        )
    if status_before:
        raise ValueError("Pinned source checkout is dirty before oracle execution")
    if blob != PINNED_SOURCE_BLOB or working_blob != PINNED_SOURCE_BLOB:
        raise ValueError(
            "Pinned source Git blob mismatch: "
            f"expected {PINNED_SOURCE_BLOB}, commit has {blob}, file has {working_blob}"
        )
    return {
        "root": root,
        "head_before": head,
        "status_before": status_before or "clean",
        "pinned_commit_blob": blob,
        "working_file_blob": working_blob,
    }


def load_pinned_module(source: Path):
    digest = hashlib.sha256(source.read_bytes()).hexdigest()
    if digest != PINNED_SOURCE_SHA256:
        raise ValueError(
            f"Pinned source hash mismatch: expected {PINNED_SOURCE_SHA256}, got {digest}"
        )
    spec = importlib.util.spec_from_file_location("pinned_fincore_garch", source)
    if spec is None or spec.loader is None:
        raise ImportError(f"Cannot load pinned GARCH source at {source}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return digest, module


def pinned_fit_behavior(module) -> dict[str, object]:
    """Capture actual pinned fit behavior for constant and infinite data."""
    models = {
        "garch11": lambda: module.GARCH(mean_model="zero"),
        "egarch": module.EGARCH,
        "gjr_garch": module.GJRGARCH,
    }
    observations = {
        "constant_0_1": np.full(11, 0.1, dtype=float),
        "positive_infinity": np.asarray(
            [0.01, -0.02, 0.03, np.inf, -0.01, 0.02, -0.03, 0.04, 0.01, -0.02, 0.03],
            dtype=float,
        ),
    }
    recorded: dict[str, object] = {}
    for model_name, constructor in models.items():
        recorded[model_name] = {}
        for input_name, values in observations.items():
            try:
                with warnings.catch_warnings(record=True) as caught, np.errstate(
                    all="ignore"
                ):
                    warnings.simplefilter("always")
                    result = constructor().fit(values)
                conditional = np.asarray(result.conditional_var, dtype=float)
                residuals = np.asarray(result.residuals, dtype=float)
                recorded[model_name][input_name] = {
                    "outcome": "returned",
                    "converged": bool(result.converged),
                    "log_likelihood_finite": bool(
                        np.isfinite(result.log_likelihood)
                    ),
                    "conditional_variances_finite": bool(np.all(np.isfinite(conditional))),
                    "conditional_variances_positive": bool(
                        np.all(conditional > 0.0)
                    ),
                    "residuals_finite": bool(np.all(np.isfinite(residuals))),
                    "warnings": [
                        {
                            "category": warning.category.__name__,
                            "message": str(warning.message),
                        }
                        for warning in caught
                    ],
                }
            except Exception as error:  # record the source's observed outcome
                recorded[model_name][input_name] = {
                    "outcome": "raised",
                    "exception_type": type(error).__name__,
                    "message": str(error),
                }
    return recorded


def pinned_reference(module, model: str) -> tuple[list[float], float]:
    values = np.asarray(OBSERVATIONS, dtype=float)
    params = PARAMETERS[model]
    if model == "garch11":
        instance = module.GARCH(mean_model="zero")
        path = instance._compute_conditional_var(
            values, params["omega"], params["alpha"], params["beta"]
        )
        nll = instance._neg_log_likelihood(
            np.asarray([params["omega"], params["alpha"], params["beta"]]), values
        )
    elif model == "egarch":
        instance = module.EGARCH()
        path = instance._compute_conditional_var(
            values,
            params["omega"],
            params["alpha"],
            params["gamma"],
            params["beta"],
        )
        nll = instance._neg_log_likelihood(
            np.asarray(
                [params["omega"], params["alpha"], params["gamma"], params["beta"]]
            ),
            values,
        )
    else:
        instance = module.GJRGARCH()
        path = instance._compute_conditional_var(
            values,
            params["omega"],
            params["alpha"],
            params["gamma"],
            params["beta"],
        )
        nll = instance._neg_log_likelihood(
            np.asarray(
                [params["omega"], params["alpha"], params["gamma"], params["beta"]]
            ),
            values,
        )
    return np.asarray(path, dtype=float).tolist(), -float(nll)


def close(actual: float, expected: float) -> bool:
    return abs(actual - expected) <= ABS_TOL + REL_TOL * abs(expected)


def build_manifest(
    source: Path,
    digest: str,
    module,
    source_git: dict[str, str],
) -> dict[str, object]:
    fixtures = []
    for model in ("garch11", "egarch", "gjr_garch"):
        python_path, python_log_likelihood = pinned_reference(module, model)
        decimal_path, decimal_log_likelihood = decimal_reference(model)
        pairs = zip(
            python_path + [python_log_likelihood],
            decimal_path + [decimal_log_likelihood],
            strict=True,
        )
        if not all(close(actual, expected) for actual, expected in pairs):
            raise AssertionError(f"Pinned Python and Decimal references differ for {model}")
        fixtures.append(
            {
                "model": model,
                "observations": list(OBSERVATIONS),
                "parameters": PARAMETERS[model],
                "pinned_python": {
                    "conditional_variances": python_path,
                    "log_likelihood": python_log_likelihood,
                },
                "decimal": {
                    "conditional_variances": decimal_path,
                    "log_likelihood": decimal_log_likelihood,
                },
            }
        )
    return {
        "schema": "fincore.garch-fixed-parameter-reference.v2",
        "oracle_execution": {
            "python_argv": sys.argv,
            "bytecode_disabled": sys.dont_write_bytecode,
            "script_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            "python_version": sys.version,
            "numpy_version": np.__version__,
            "scipy_version": scipy.__version__,
            "platform": platform.platform(),
            "timezone_environment": os.environ.get("TZ"),
            "timezone_runtime": {
                "tzname": list(time.tzname),
                "timezone_seconds_west": time.timezone,
                "local_zone": time.localtime().tm_zone,
            },
            "locale": locale.setlocale(locale.LC_ALL, None),
        },
        "tolerance": {"absolute": ABS_TOL, "relative": REL_TOL},
        "conventions": {
            "initial_variance": "population variance of the supplied residual/return vector",
            "likelihood_window": "indices 1 through n-1; index 0 initializes the path only",
            "egarch_standardization": "z[t-1] = observation[t-1] / sqrt(variance[t-1])",
        },
        "pinned_python_source": {
            "commit": PINNED_SOURCE_COMMIT,
            "git_blob": PINNED_SOURCE_BLOB,
            "sha256": digest,
            "path": str(source),
            "module_file": str(Path(module.__file__).resolve()),
            "git_checkout": source_git,
            "kernels": "GARCH._compute_conditional_var/_neg_log_likelihood; EGARCH and GJRGARCH equivalents",
        },
        "pinned_input_fit_behavior": pinned_fit_behavior(module),
        "fit_characterization": {
            "optimizer": "Pinned source uses SciPy L-BFGS-B; native uses transformed Nelder-Mead.",
            "garch11_zero_mean_fixture": {
                "pinned_python_log_likelihood": 747.2805538792478,
                "native_recorded_log_likelihood": 747.72860692539825,
                "note": "The recorded native local fit has a higher likelihood for this fixture; this does not establish global optimality or fitted-parameter parity. Fixed-parameter paths and likelihoods are compared separately."
            },
        },
        "fixtures": fixtures,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--python-source",
        type=Path,
        required=True,
        help="path to fincore/risk/garch.py from the pinned clean Git checkout",
    )
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    source = locate_source(args.python_source)
    source_git = verify_source_git_identity(source)
    digest, module = load_pinned_module(source)
    manifest = build_manifest(source, digest, module, source_git)
    status_after = git_output(source, "status", "--porcelain")
    head_after = git_output(source, "rev-parse", "HEAD")
    if head_after != PINNED_SOURCE_COMMIT or status_after:
        raise RuntimeError("Pinned source checkout changed during oracle execution")
    manifest["pinned_python_source"]["git_checkout"]["head_after"] = head_after
    manifest["pinned_python_source"]["git_checkout"]["status_after"] = (
        status_after or "clean"
    )
    rendered = json.dumps(manifest, indent=2, sort_keys=True, allow_nan=False) + "\n"
    if args.output is None:
        sys.stdout.write(rendered)
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered, encoding="utf-8")
        print(f"wrote {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
