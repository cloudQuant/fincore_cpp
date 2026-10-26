#!/usr/bin/env python3
"""Paired C4-vs-current native performance no-regression check.

Uses fresh processes, alternating AB/BA order, predeclared 5 pairs, and the
Iteration 7 harness. macOS has no CPU-affinity control, so dispersion >5% is
reported as INCONCLUSIVE rather than PASS.
"""

from __future__ import annotations

import argparse
import json
import random
import statistics
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CASES = ["target_validation", "analysis_prepare", "research_assembly", "positions", "fingerprint"]


def run(binary: Path, case: str, label: str) -> dict:
    output = subprocess.check_output(
        [str(binary), "--case", case, "--assets", "200", "--dates", "120",
         "--warmup", "2", "--repeat", "1", "--label", label],
        text=True,
    )
    line = [item for item in output.splitlines() if item.strip().startswith("{")][-1]
    return json.loads(line)


def median(values: list[float]) -> float:
    return statistics.median(values)


def dispersion(ratios: list[float]) -> float:
    center = median(ratios)
    mad = median([abs(value - center) for value in ratios])
    return 1.4826 * mad / center if center else float("inf")


def bootstrap_interval(ratios: list[float], seed: int = 20261024) -> tuple[float, float]:
    rng = random.Random(seed)
    samples = []
    for _ in range(10_000):
        draw = [ratios[rng.randrange(len(ratios))] for _ in ratios]
        samples.append(median(draw))
    samples.sort()
    return samples[250], samples[9750]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--c4", type=Path, required=True)
    parser.add_argument("--current", type=Path, required=True)
    parser.add_argument("--pairs", type=int, default=5)
    parser.add_argument("--out", type=Path, default=ROOT / "docs/evidence/iteration-8/p109/performance.json")
    args = parser.parse_args()

    reports = []
    for case in CASES:
        ratios = []
        raw = []
        digests_equal = True
        for pair in range(args.pairs):
            order = [("c4", args.c4), ("current", args.current)]
            if pair % 2 == 1:
                order.reverse()
            measurements = {}
            for label, binary in order:
                result = run(binary, case, label)
                measurements[label] = result
            c4 = measurements["c4"]
            current = measurements["current"]
            digests_equal &= c4["digest"] == current["digest"]
            ratio = current["ms"] / c4["ms"]
            ratios.append(ratio)
            raw.append({"pair": pair, "order": [item[0] for item in order],
                        "c4_ms": c4["ms"], "current_ms": current["ms"],
                        "ratio": ratio, "c4_rss": c4["maxrss_bytes"],
                        "current_rss": current["maxrss_bytes"]})
        center = median(ratios)
        spread = dispersion(ratios)
        low, high = bootstrap_interval(ratios)
        if not digests_equal:
            status = "FAIL"
            reason = "digest mismatch between arms"
        elif spread > 0.05:
            status = "INCONCLUSIVE"
            reason = "ratio dispersion >5%"
        elif high <= 1.10:
            status = "PASS"
            reason = "95% median-ratio upper bound <=1.10"
        elif low > 1.10:
            status = "FAIL"
            reason = "95% median-ratio lower bound >1.10"
        else:
            status = "INCONCLUSIVE"
            reason = "95% interval straddles 1.10"
        reports.append({"case": case, "status": status, "reason": reason,
                        "median_ratio": center, "dispersion": spread,
                        "ci95": [low, high], "digests_equal": digests_equal,
                        "pairs": raw})
        print(f"{case}: {status} median={center:.4f} dispersion={spread:.4f} ci=[{low:.4f},{high:.4f}]")

    payload = {
        "schema_version": "1",
        "phase": "P109",
        "reference": "Iteration 7 C4 source archive",
        "current": "Iteration 8 candidate",
        "protocol": {"fresh_processes": True, "alternating_ab_ba": True,
                     "pairs": args.pairs, "bootstrap": 10000,
                     "dispersion_gate": 0.05, "ratio_ceiling": 1.10,
                     "cpu_affinity": "not available on macOS; dispersion gate applied"},
        "cases": reports,
    }
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n")
    statuses = [case["status"] for case in reports]
    print(json.dumps({"statuses": statuses, "all_pass": all(s == "PASS" for s in statuses)}))
    return 0 if all(s == "PASS" for s in statuses) else 1


if __name__ == "__main__":
    raise SystemExit(main())
