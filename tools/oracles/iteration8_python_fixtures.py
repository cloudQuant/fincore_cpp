#!/usr/bin/env python3
"""Generate the Iteration 8 NumPy RandomState oracle fixture.

Run with the pinned oracle environment (NumPy 1.26.4). The C++ test reads the
CSV, so the RNG sequence is not merely self-asserted in C++.
"""

from __future__ import annotations

import json
import hashlib
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "tests/fixtures/iteration8"


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    seed = 42
    population = 5
    num_samples = 3
    num_days = 4
    rng = np.random.RandomState(seed)
    indices = rng.randint(0, population, size=(num_samples, num_days))
    path = OUT / "simulate_paths_seed42.csv"
    lines = ["seed,num_samples,num_days,population", f"{seed},{num_samples},{num_days},{population}"]
    lines.extend(",".join(str(int(value)) for value in row) for row in indices)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    metadata = {
        "oracle": "numpy.random.RandomState",
        "numpy_version": np.__version__,
        "seed": seed,
        "population": population,
        "num_samples": num_samples,
        "num_days": num_days,
        "fixture": str(path.relative_to(ROOT)),
        "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "indices": indices.tolist(),
    }
    (OUT / "simulate_paths_seed42.json").write_text(
        json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(json.dumps(metadata, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
