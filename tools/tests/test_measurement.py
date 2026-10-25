from __future__ import annotations

import math
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
MEASUREMENT_ROOT = REPOSITORY_ROOT / "benchmarks" / "alpha003_a_stocks"
sys.path.insert(0, str(MEASUREMENT_ROOT))

import measurement  # noqa: E402


class _FakeWindowsApi:
    current_handle = 100

    def __init__(self, *, child_mask: int | None = None) -> None:
        self.parent_mask = 0b1100
        self.system_mask = 0b1111
        self.child_mask_override = child_mask
        self.child_masks: dict[int, int] = {}
        self.events: list[tuple[str, int]] = []

    def current_process(self) -> int:
        return self.current_handle

    def get_process_affinity_mask(self, handle: int) -> tuple[int, int]:
        if handle == self.current_handle:
            return self.parent_mask, self.system_mask
        return self.child_masks[handle], self.system_mask

    def set_process_affinity_mask(self, handle: int, mask: int) -> None:
        self.events.append(("set_affinity", mask))
        if handle != self.current_handle:
            raise AssertionError("only the current process should be pinned/restored")
        self.parent_mask = mask

    def process_cpu_seconds(self, handle: int) -> float:
        self.events.append(("read_cpu", handle))
        return 0.25

    def peak_working_set_bytes(self, handle: int) -> int:
        self.events.append(("read_peak", handle))
        return 4096


class _FakeProcess:
    def __init__(self, api: _FakeWindowsApi, *, timeout_first_wait: bool = False) -> None:
        self.api = api
        self._handle = 200
        inherited_mask = api.parent_mask
        api.child_masks[self._handle] = (
            api.child_mask_override if api.child_mask_override is not None else inherited_mask
        )
        api.events.append(("spawn", inherited_mask))
        self.returncode: int | None = None
        self.killed = False
        self.timeout_first_wait = timeout_first_wait
        self.wait_count = 0

    def wait(self, timeout: float | None = None) -> int:
        self.wait_count += 1
        self.api.events.append(("wait", self.api.parent_mask))
        if self.timeout_first_wait and self.wait_count == 1:
            raise subprocess.TimeoutExpired("fake", timeout)
        self.returncode = -9 if self.killed else 0
        return self.returncode

    def poll(self) -> int | None:
        return self.returncode

    def kill(self) -> None:
        self.killed = True


class MeasurementTests(unittest.TestCase):
    def test_thread_environment_is_explicit_and_does_not_mutate_caller(self) -> None:
        original = {"PATH": "example", "OMP_NUM_THREADS": "16"}

        result = measurement.thread_limited_environment(original)

        self.assertEqual(original, {"PATH": "example", "OMP_NUM_THREADS": "16"})
        self.assertEqual(result["PATH"], "example")
        for name, value in measurement.THREAD_ENVIRONMENT.items():
            self.assertEqual(result[name], value)

    def test_pair_schedule_is_abba_balanced_for_formal_and_development_counts(self) -> None:
        formal = measurement.pair_order_schedule(10)
        development = measurement.pair_order_schedule(5)

        self.assertEqual(formal[:4], (("A", "B"), ("B", "A"), ("A", "B"), ("B", "A")))
        self.assertEqual(formal.count(("A", "B")), 5)
        self.assertEqual(formal.count(("B", "A")), 5)
        self.assertEqual(development.count(("A", "B")), 3)
        self.assertEqual(development.count(("B", "A")), 2)
        self.assertEqual(
            measurement.warmup_order_schedule(), (("A", "B"), ("B", "A"))
        )

    def test_pair_schedule_rejects_invalid_counts_and_labels(self) -> None:
        for count in (0, -1, True, 1.5):
            with self.subTest(count=count), self.assertRaises(ValueError):
                measurement.pair_order_schedule(count)  # type: ignore[arg-type]
        with self.assertRaises(ValueError):
            measurement.pair_order_schedule(2, first="same", second="same")

    def test_summary_retains_samples_and_marks_noise_above_ten_percent(self) -> None:
        values = [98, 100, 100, 102]
        summary = measurement.summarize_samples(values)

        self.assertEqual(summary["samples"], values)
        self.assertEqual(summary["median"], 100)
        self.assertEqual(summary["q1"], 99.5)
        self.assertEqual(summary["q3"], 100.5)
        self.assertEqual(summary["iqr"], 1.0)
        self.assertEqual(summary["min"], 98.0)
        self.assertEqual(summary["max"], 102.0)
        self.assertEqual(summary["status"], "VALID")

        noisy = measurement.summarize_samples([1, 2, 3, 4])
        self.assertEqual(noisy["status"], "INCONCLUSIVE")
        self.assertGreater(noisy["iqr_over_median"], 0.10)  # type: ignore[operator]
        self.assertEqual(measurement.summarize_samples([0, 0])["status"], "INCONCLUSIVE")

    def test_summary_rejects_non_finite_negative_and_boolean_observations(self) -> None:
        for values in ([math.nan], [math.inf], [-1.0], [True]):
            with self.subTest(values=values), self.assertRaises(ValueError):
                measurement.summarize_samples(values)  # type: ignore[arg-type]

    def test_paired_bootstrap_uses_fixed_seed_and_median_ratio(self) -> None:
        reference = list(range(100, 110))
        candidate = [value * 1.1 for value in reference]

        first = measurement.paired_bootstrap_regression(reference, candidate)
        second = measurement.paired_bootstrap_regression(reference, candidate)

        self.assertEqual(first, second)
        self.assertAlmostEqual(first["ratio"], 1.1)  # type: ignore[arg-type]
        # Because each candidate observation is paired at a fixed 10% increase,
        # every paired-index resample has the same candidate/reference ratio.
        self.assertAlmostEqual(first["ci_lower"], 1.1)  # type: ignore[arg-type]
        self.assertAlmostEqual(first["ci_upper"], 1.1)  # type: ignore[arg-type]
        self.assertEqual(first["status"], "FAIL")
        self.assertEqual(first["seed"], 92_026)
        self.assertEqual(first["resamples"], 10_000)

    def test_paired_bootstrap_oracle_uses_ratio_of_resampled_medians(self) -> None:
        reference = [1.0] * 5 + [10.0] * 5
        candidate = [1.0] * 10

        result = measurement.paired_bootstrap_regression(reference, candidate)

        self.assertAlmostEqual(result["ratio"], 1.0 / 5.5)  # type: ignore[arg-type]
        self.assertNotAlmostEqual(result["ratio"], 0.55)  # type: ignore[arg-type]
        self.assertAlmostEqual(result["ci_lower"], 0.1)  # type: ignore[arg-type]
        self.assertAlmostEqual(result["ci_upper"], 1.0)  # type: ignore[arg-type]

    def test_paired_bootstrap_classifies_pass_and_inconclusive(self) -> None:
        reference = [1.0] * 10
        passed = measurement.paired_bootstrap_regression(reference, [1.04] * 10)
        inconclusive = measurement.paired_bootstrap_regression(
            reference, [0.90] * 5 + [1.10] * 5
        )

        self.assertEqual(passed["status"], "PASS")
        self.assertLessEqual(passed["ci_upper"], 1.05)  # type: ignore[operator]
        self.assertEqual(inconclusive["status"], "INCONCLUSIVE")

    def test_paired_bootstrap_rejects_unpaired_or_insufficient_samples(self) -> None:
        with self.assertRaises(ValueError):
            measurement.paired_bootstrap_regression([1.0] * 10, [1.0] * 9)
        with self.assertRaises(ValueError):
            measurement.paired_bootstrap_regression([1.0] * 9, [1.0] * 9)
        with self.assertRaises(ValueError):
            measurement.paired_bootstrap_regression([0.0] * 10, [1.0] * 10)

    def test_run_command_pins_before_spawn_restores_parent_and_records_child(self) -> None:
        api = _FakeWindowsApi()
        process: _FakeProcess | None = None

        def popen(*args: object, **kwargs: object) -> _FakeProcess:
            nonlocal process
            self.assertEqual(api.parent_mask, 0b1000)
            self.assertEqual(kwargs["env"]["MKL_NUM_THREADS"], "1")  # type: ignore[index]
            self.assertIs(kwargs["stderr"], subprocess.STDOUT)
            process = _FakeProcess(api)
            return process

        with tempfile.TemporaryDirectory() as temporary_directory:
            with (
                mock.patch.object(measurement, "_WIN32", api),
                mock.patch.object(measurement.subprocess, "Popen", side_effect=popen),
                mock.patch.object(measurement.time, "perf_counter", side_effect=(10.0, 11.5)),
            ):
                result = measurement.run_command(
                    ["worker.exe", "--fixed", ""],
                    cpu_index=3,
                    stdout_path=Path(temporary_directory) / "run.log",
                )

        self.assertIsNotNone(process)
        self.assertEqual(api.parent_mask, 0b1100)
        self.assertEqual(api.events[0], ("set_affinity", 0b1000))
        self.assertEqual(api.events[1], ("spawn", 0b1000))
        self.assertEqual(api.events[2], ("set_affinity", 0b1100))
        self.assertEqual(api.events[3], ("wait", 0b1100))
        self.assertEqual(result.affinity_mask, 0b1000)
        self.assertEqual(result.command, ("worker.exe", "--fixed", ""))
        self.assertEqual(result.cpu_index, 3)
        self.assertTrue(result.affinity_verified)
        self.assertEqual(result.wall_seconds, 1.5)
        self.assertEqual(result.cpu_seconds, 0.25)
        self.assertEqual(result.peak_working_set_bytes, 4096)
        self.assertEqual(result.thread_environment["OPENBLAS_NUM_THREADS"], "1")

    def test_run_command_restores_parent_when_spawn_fails(self) -> None:
        api = _FakeWindowsApi()
        with (
            mock.patch.object(measurement, "_WIN32", api),
            mock.patch.object(measurement.subprocess, "Popen", side_effect=OSError("spawn failed")),
            mock.patch.object(measurement.time, "perf_counter", return_value=10.0),
        ):
            with self.assertRaisesRegex(OSError, "spawn failed"):
                measurement.run_command(["worker.exe"], cpu_index=2)
        self.assertEqual(api.parent_mask, 0b1100)

    def test_run_command_kills_and_waits_if_child_affinity_is_not_inherited(self) -> None:
        api = _FakeWindowsApi(child_mask=0b1111)
        process: _FakeProcess | None = None

        def popen(*args: object, **kwargs: object) -> _FakeProcess:
            nonlocal process
            process = _FakeProcess(api)
            return process

        with (
            mock.patch.object(measurement, "_WIN32", api),
            mock.patch.object(measurement.subprocess, "Popen", side_effect=popen),
            mock.patch.object(measurement.time, "perf_counter", return_value=10.0),
        ):
            with self.assertRaisesRegex(RuntimeError, "child affinity did not inherit"):
                measurement.run_command(["worker.exe"], cpu_index=2)

        self.assertTrue(process.killed)  # type: ignore[union-attr]
        self.assertEqual(process.wait_count, 1)  # type: ignore[union-attr]
        self.assertEqual(api.parent_mask, 0b1100)

    def test_run_command_timeout_kills_and_waits_before_raising(self) -> None:
        api = _FakeWindowsApi()
        process: _FakeProcess | None = None

        def popen(*args: object, **kwargs: object) -> _FakeProcess:
            nonlocal process
            process = _FakeProcess(api, timeout_first_wait=True)
            return process

        with (
            mock.patch.object(measurement, "_WIN32", api),
            mock.patch.object(measurement.subprocess, "Popen", side_effect=popen),
            mock.patch.object(measurement.time, "perf_counter", return_value=10.0),
        ):
            with self.assertRaises(subprocess.TimeoutExpired):
                measurement.run_command(["worker.exe"], cpu_index=2, timeout=0.01)

        self.assertTrue(process.killed)  # type: ignore[union-attr]
        self.assertEqual(process.wait_count, 2)  # type: ignore[union-attr]
        self.assertEqual(api.parent_mask, 0b1100)

    def test_run_command_fails_closed_without_windows_affinity(self) -> None:
        with mock.patch.object(measurement, "_WIN32", None):
            with self.assertRaises(NotImplementedError):
                measurement.run_command(["worker.exe"])


if __name__ == "__main__":
    unittest.main()
