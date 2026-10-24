#!/usr/bin/env python3
"""Replay pinned Python MWR/XIRR fixtures and measure returned-rate residuals.

Run with the user's Anaconda environment and a clean checkout of the pinned
oracle, for example:
  /Users/yunjinqi/opt/anaconda3/bin/conda run -n base python -B \
    tools/oracles/irr_extremes.py --oracle-root /path/to/fincore --output /tmp/irr-oracle.json

This records the Python reference behavior. Native-specific NaN expectations
for roots that cannot meet the declared residual budget are stated in the
fixture file and checked by the C++ unit tests.
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import math
import subprocess
import sys
from datetime import date
from decimal import Decimal, localcontext
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
FIXTURES = ROOT / "docs/evidence/revised-plan-20261023/irr-hardening/fixtures.json"
PINNED_COMMIT = "4b8f31b1b59707480d5db9603704d83bbf7e6946"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def normalized_residual(
    cashflows: list[float], periods: list[float], rate: float
) -> float | None:
    if not math.isfinite(rate) or rate <= -1.0:
        return None
    with localcontext() as context:
        context.prec = 100
        log_gross = (Decimal(1) + Decimal.from_float(rate)).ln()
        magnitudes = [
            abs(Decimal.from_float(flow)).ln()
            - Decimal.from_float(period) * log_gross
            for flow, period in zip(cashflows, periods, strict=True)
            if flow != 0.0
        ]
        if not magnitudes:
            return None
        anchor = max(magnitudes)
        terms = [
            (Decimal(1) if flow > 0.0 else Decimal(-1))
            * (abs(Decimal.from_float(flow)).ln()
               - Decimal.from_float(period) * log_gross - anchor).exp()
            for flow, period in zip(cashflows, periods, strict=True)
            if flow != 0.0
        ]
        denominator = sum((abs(term) for term in terms), Decimal(0))
        if denominator == 0:
            return None
        return float(abs(sum(terms, Decimal(0))) / denominator)


def load_oracle(oracle_root: Path) -> tuple[Any, Path, str]:
    source = oracle_root / "fincore/performance/returns.py"
    if not source.is_file():
        raise RuntimeError(f"pinned returns.py was not found: {source}")
    revision = subprocess.run(
        ["git", "-C", str(oracle_root), "rev-parse", "HEAD"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()
    if revision != PINNED_COMMIT:
        raise RuntimeError(
            f"oracle checkout must be {PINNED_COMMIT}, found {revision}"
        )
    spec = importlib.util.spec_from_file_location("pinned_fincore_returns", source)
    if spec is None or spec.loader is None:
        raise RuntimeError("could not load the pinned returns.py module")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module, source, revision


def case_periods(case: dict[str, Any]) -> list[float]:
    if case["route"] == "xirr":
        dates = [date.fromisoformat(value) for value in case["dates"]]
        first = min(dates)
        return [(value - first).days / 365.0 for value in dates]
    interval = case["interval"]
    step = 1.0 if interval is None else float(interval)
    return [float(index) * step for index in range(len(case["cashflows"]))]


def replay_case(
    module: Any, case: dict[str, Any], budgets: dict[str, Any]
) -> dict[str, Any]:
    cashflows = [float(value) for value in case["cashflows"]]
    periods = case_periods(case)
    if case["route"] == "xirr":
        dates = [date.fromisoformat(value) for value in case["dates"]]
        rate = float(module.xirr(cashflows, dates))
    else:
        rate = float(module.mwr(cashflows, case["interval"]))
    finite_rate = math.isfinite(rate)
    expected = case["native_result"]
    residual = normalized_residual(cashflows, periods, rate)
    rate_error = None
    rate_budget_pass = None
    if finite_rate and "analytic_rate" in case:
        analytic_rate = float(case["analytic_rate"])
        rate_error = abs(rate - analytic_rate)
        if case["rate_budget"] == "extreme_two_flow_rate_relative":
            rate_error /= abs(analytic_rate)
        rate_budget = float(budgets[case["rate_budget"]])
        rate_budget_pass = rate_error <= rate_budget
    if expected == "finite" and not finite_rate:
        outcome = "REFERENCE_UNEXPECTED_NAN"
    elif expected == "nan" and finite_rate:
        outcome = "REFERENCE_FINITE_NATIVE_EXPECTS_NAN"
    elif expected == "finite" and residual is not None and residual > 1e-10:
        outcome = "REFERENCE_RESIDUAL_OUT_OF_BUDGET"
    elif expected == "finite" and rate_budget_pass is False:
        outcome = "REFERENCE_RATE_OUT_OF_BUDGET"
    else:
        outcome = "REFERENCE_CLASS_MATCH"
    return {
        "name": case["name"],
        "route": case["route"],
        "cashflows": cashflows,
        "periods_years": periods,
        "python_rate": rate if finite_rate else None,
        "python_normalized_residual": residual,
        "python_rate_error": rate_error,
        "python_rate_budget_pass": rate_budget_pass,
        "native_expected_class": expected,
        "reference_outcome": outcome,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--oracle-root", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    fixtures = json.loads(FIXTURES.read_text(encoding="utf-8"))
    if fixtures["oracle_commit"] != PINNED_COMMIT:
        raise RuntimeError("fixture oracle commit and runner pin disagree")
    module, source, revision = load_oracle(args.oracle_root.resolve())
    records = [
        replay_case(module, case, fixtures["budgets"])
        for case in fixtures["cases"]
    ]
    payload = {
        "schema_version": 1,
        "oracle_commit": revision,
        "oracle_source_sha256": sha256(source),
        "runner_sha256": sha256(Path(__file__).resolve()),
        "fixtures_sha256": sha256(FIXTURES),
        "python_version": sys.version,
        "records": records,
        "passed": all(
            record["reference_outcome"] in {
                "REFERENCE_CLASS_MATCH",
                "REFERENCE_FINITE_NATIVE_EXPECTS_NAN",
            }
            and (
                record["native_expected_class"] != "finite"
                or record["python_normalized_residual"] is not None
                and record["python_normalized_residual"] <= 1e-10
                and record["python_rate_budget_pass"] is True
            )
            for record in records
        ),
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(payload, indent=2, sort_keys=True, allow_nan=False) + "\n",
        encoding="utf-8",
    )
    print(f"records={len(records)} passed={payload['passed']} output={args.output}")
    return 0 if payload["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
