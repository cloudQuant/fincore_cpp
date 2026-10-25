"""Small, reusable helpers for controlled benchmark measurements.

Windows child processes inherit their creator's process affinity.  The runner
temporarily narrows its own affinity *before* ``CreateProcess`` (performed by
``subprocess.Popen``), verifies the child's one-bit mask, then restores the
runner before waiting.  The measured wall interval starts immediately before
process creation and ends when that child exits, so interpreter/loader startup
is included.  CPU time and peak working set are read from the completed child
through Windows process APIs.

This module deliberately does not implement a benchmark campaign or inspect
benchmark outputs.  Callers own warmup execution, order scheduling, output
comparison, and result persistence.
"""

from __future__ import annotations

import ctypes
import math
import os
import random
import statistics
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path
from typing import IO, Mapping, Sequence


THREAD_ENVIRONMENT = {
    "OMP_NUM_THREADS": "1",
    "OMP_THREAD_LIMIT": "1",
    "OMP_DYNAMIC": "FALSE",
    "OPENBLAS_NUM_THREADS": "1",
    "MKL_NUM_THREADS": "1",
    "MKL_DYNAMIC": "FALSE",
    "BLIS_NUM_THREADS": "1",
    "VECLIB_MAXIMUM_THREADS": "1",
    "NUMEXPR_NUM_THREADS": "1",
    "NUMEXPR_MAX_THREADS": "1",
    "TBB_NUM_THREADS": "1",
}

DEFAULT_NOISE_LIMIT = 0.10
DEFAULT_BOOTSTRAP_RESAMPLES = 10_000
DEFAULT_BOOTSTRAP_SEED = 92_026
DEFAULT_REGRESSION_LIMIT = 1.05
MEASUREMENT_PROTOCOL = "iteration9-r1"


@dataclass(frozen=True)
class CommandMeasurement:
    """Timing and resource facts for one completed child process."""

    command: tuple[str, ...]
    returncode: int
    wall_seconds: float
    cpu_seconds: float
    peak_working_set_bytes: int
    affinity_mask: int
    cpu_index: int
    affinity_method: str
    affinity_verified: bool
    thread_environment: dict[str, str]


class _WindowsApi:
    """Narrow Win32 wrapper kept injectable so process control is unit-testable."""

    PROCESSOR_MASK = ctypes.c_size_t

    def __init__(self) -> None:
        if os.name != "nt":
            raise NotImplementedError("single-CPU process measurement currently requires Windows")
        self._kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        self._psapi = ctypes.WinDLL("psapi", use_last_error=True)

        self._kernel32.GetCurrentProcess.restype = ctypes.c_void_p
        self._kernel32.GetProcessAffinityMask.argtypes = [
            ctypes.c_void_p,
            ctypes.POINTER(self.PROCESSOR_MASK),
            ctypes.POINTER(self.PROCESSOR_MASK),
        ]
        self._kernel32.GetProcessAffinityMask.restype = ctypes.c_int
        self._kernel32.SetProcessAffinityMask.argtypes = [
            ctypes.c_void_p,
            self.PROCESSOR_MASK,
        ]
        self._kernel32.SetProcessAffinityMask.restype = ctypes.c_int
        self._kernel32.GetProcessTimes.argtypes = [
            ctypes.c_void_p,
            ctypes.c_void_p,
            ctypes.c_void_p,
            ctypes.c_void_p,
            ctypes.c_void_p,
        ]
        self._kernel32.GetProcessTimes.restype = ctypes.c_int
        self._psapi.GetProcessMemoryInfo.argtypes = [
            ctypes.c_void_p,
            ctypes.c_void_p,
            ctypes.c_uint,
        ]
        self._psapi.GetProcessMemoryInfo.restype = ctypes.c_int

    @staticmethod
    def _raise_last_error(operation: str) -> None:
        error = ctypes.get_last_error()
        raise OSError(error, f"{operation} failed: {ctypes.FormatError(error).strip()}")

    def current_process(self) -> int:
        return int(self._kernel32.GetCurrentProcess())

    def get_process_affinity_mask(self, handle: int) -> tuple[int, int]:
        process_mask = self.PROCESSOR_MASK()
        system_mask = self.PROCESSOR_MASK()
        if not self._kernel32.GetProcessAffinityMask(
            ctypes.c_void_p(handle), ctypes.byref(process_mask), ctypes.byref(system_mask)
        ):
            self._raise_last_error("GetProcessAffinityMask")
        return int(process_mask.value), int(system_mask.value)

    def set_process_affinity_mask(self, handle: int, mask: int) -> None:
        if not self._kernel32.SetProcessAffinityMask(ctypes.c_void_p(handle), mask):
            self._raise_last_error("SetProcessAffinityMask")

    @staticmethod
    def _filetime_seconds(value: ctypes.Structure) -> float:
        ticks = (int(value.dwHighDateTime) << 32) | int(value.dwLowDateTime)
        return ticks / 10_000_000.0

    def process_cpu_seconds(self, handle: int) -> float:
        class FILETIME(ctypes.Structure):
            _fields_ = [("dwLowDateTime", ctypes.c_uint32), ("dwHighDateTime", ctypes.c_uint32)]

        creation = FILETIME()
        exit_time = FILETIME()
        kernel = FILETIME()
        user = FILETIME()
        if not self._kernel32.GetProcessTimes(
            ctypes.c_void_p(handle),
            ctypes.byref(creation),
            ctypes.byref(exit_time),
            ctypes.byref(kernel),
            ctypes.byref(user),
        ):
            self._raise_last_error("GetProcessTimes")
        return self._filetime_seconds(kernel) + self._filetime_seconds(user)

    def peak_working_set_bytes(self, handle: int) -> int:
        class PROCESS_MEMORY_COUNTERS_EX(ctypes.Structure):
            _fields_ = [
                ("cb", ctypes.c_uint32),
                ("PageFaultCount", ctypes.c_uint32),
                ("PeakWorkingSetSize", ctypes.c_size_t),
                ("WorkingSetSize", ctypes.c_size_t),
                ("QuotaPeakPagedPoolUsage", ctypes.c_size_t),
                ("QuotaPagedPoolUsage", ctypes.c_size_t),
                ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
                ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
                ("PagefileUsage", ctypes.c_size_t),
                ("PeakPagefileUsage", ctypes.c_size_t),
                ("PrivateUsage", ctypes.c_size_t),
            ]

        counters = PROCESS_MEMORY_COUNTERS_EX()
        counters.cb = ctypes.sizeof(counters)
        if not self._psapi.GetProcessMemoryInfo(
            ctypes.c_void_p(handle), ctypes.byref(counters), counters.cb
        ):
            self._raise_last_error("GetProcessMemoryInfo")
        return int(counters.PeakWorkingSetSize)


_WIN32: _WindowsApi | None
try:
    _WIN32 = _WindowsApi() if os.name == "nt" else None
except (AttributeError, OSError):
    _WIN32 = None


def thread_limited_environment(base: Mapping[str, str] | None = None) -> dict[str, str]:
    """Return a copied environment with common BLAS/OpenMP pools fixed at one."""

    environment = dict(os.environ if base is None else base)
    environment.update(THREAD_ENVIRONMENT)
    return environment


def _current_process_affinity(api: _WindowsApi) -> tuple[int, int, int]:
    handle = api.current_process()
    process_mask, system_mask = api.get_process_affinity_mask(handle)
    allowed_mask = process_mask & system_mask
    if allowed_mask == 0:
        raise RuntimeError("the current process has no available logical CPU in its affinity mask")
    return handle, process_mask, allowed_mask


def _select_cpu_mask(allowed_mask: int, cpu_index: int | None) -> tuple[int, int]:
    if cpu_index is None:
        mask = allowed_mask & -allowed_mask
        return mask.bit_length() - 1, mask
    if isinstance(cpu_index, bool) or not isinstance(cpu_index, int) or cpu_index < 0:
        raise ValueError("cpu_index must be a non-negative logical CPU index")
    mask = 1 << cpu_index
    if not allowed_mask & mask:
        raise ValueError(f"logical CPU index {cpu_index} is not in the current allowed affinity mask")
    return cpu_index, mask


def run_command(
    command: Sequence[str | os.PathLike[str]],
    *,
    cpu_index: int | None = None,
    cwd: str | os.PathLike[str] | None = None,
    env: Mapping[str, str] | None = None,
    stdout_path: str | os.PathLike[str] | None = None,
    stderr_path: str | os.PathLike[str] | None = None,
    timeout: float | None = None,
) -> CommandMeasurement:
    """Run one Windows child on a single logical CPU from process creation.

    The parent process is pinned before ``Popen`` creates the child.  Windows
    affinity inheritance applies the one-bit mask when the child is created;
    the child mask is verified before its exit is awaited.  The parent is
    restored immediately after creation so unrelated parent work does not run
    on the selected CPU for the duration of the benchmark.

    The wall clock starts immediately before ``Popen`` and stops when the
    child exits.  CPU time and peak working set are then read from the child's
    process handle, outside that wall interval.  A timeout kills and waits for
    the child before raising ``TimeoutExpired``.
    """

    if _WIN32 is None:
        raise NotImplementedError("single-CPU process measurement currently requires Windows")
    if isinstance(command, (str, bytes)) or not command:
        raise ValueError("command must be a non-empty sequence of executable and arguments")
    normalized_command = tuple(os.fsdecode(os.fspath(part)) for part in command)
    if not normalized_command[0]:
        raise ValueError("the executable path must not be empty")
    if timeout is not None and (not math.isfinite(timeout) or timeout <= 0):
        raise ValueError("timeout must be a finite positive number or None")

    environment = thread_limited_environment(env)
    recorded_thread_environment = dict(THREAD_ENVIRONMENT)
    output_context: list[IO[bytes]] = []
    output_paths = (stdout_path, stderr_path)
    output_targets: list[IO[bytes] | int | None] = [None, None]
    try:
        for index, path in enumerate(output_paths):
            if path is not None:
                stream = Path(path).open("wb")
                output_context.append(stream)
                output_targets[index] = stream
        if stdout_path is not None and stderr_path is None:
            output_targets[1] = subprocess.STDOUT

        parent_handle, previous_mask, allowed_mask = _current_process_affinity(_WIN32)
        selected_index, selected_mask = _select_cpu_mask(allowed_mask, cpu_index)
        _WIN32.set_process_affinity_mask(parent_handle, selected_mask)
        parent_is_pinned = True
        process: subprocess.Popen[bytes] | None = None
        try:
            started = time.perf_counter()
            try:
                process = subprocess.Popen(
                    normalized_command,
                    cwd=cwd,
                    env=environment,
                    stdout=output_targets[0],
                    stderr=output_targets[1],
                    shell=False,
                )
            finally:
                # Restore as soon as CreateProcess has copied the parent mask.
                try:
                    _WIN32.set_process_affinity_mask(parent_handle, previous_mask)
                except BaseException:
                    if process is not None and process.poll() is None:
                        process.kill()
                        process.wait()
                    raise
                else:
                    parent_is_pinned = False

            assert process is not None
        finally:
            if parent_is_pinned:
                _WIN32.set_process_affinity_mask(parent_handle, previous_mask)

        child_handle = int(process._handle)  # Popen retains this handle after wait().
        try:
            child_mask, _ = _WIN32.get_process_affinity_mask(child_handle)
            if child_mask != selected_mask:
                raise RuntimeError(
                    "child affinity did not inherit the requested one-bit mask "
                    f"(expected 0x{selected_mask:x}, observed 0x{child_mask:x})"
                )
            try:
                returncode = process.wait(timeout=timeout)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
                raise
        except BaseException:
            if process.poll() is None:
                process.kill()
                process.wait()
            raise
        wall_seconds = time.perf_counter() - started
        cpu_seconds = _WIN32.process_cpu_seconds(child_handle)
        peak_working_set_bytes = _WIN32.peak_working_set_bytes(child_handle)
        return CommandMeasurement(
            command=normalized_command,
            returncode=int(returncode),
            wall_seconds=wall_seconds,
            cpu_seconds=cpu_seconds,
            peak_working_set_bytes=peak_working_set_bytes,
            affinity_mask=child_mask,
            cpu_index=selected_index,
            affinity_method="inherited_parent_affinity_before_create_process",
            affinity_verified=True,
            thread_environment=recorded_thread_environment,
        )
    finally:
        for stream in output_context:
            stream.close()


def pair_order_schedule(
    pair_count: int,
    *,
    first: str = "A",
    second: str = "B",
) -> tuple[tuple[str, str], ...]:
    """Return an ABBA-balanced sequence of first/second run orders.

    Pair order alternates AB, BA, AB, BA, ... (the resulting run stream is
    ABBAABBA...).  Even counts therefore have exactly equal orientations,
    while odd counts differ by one in favor of AB.  The order is deterministic
    and performs no benchmark work.
    """

    if isinstance(pair_count, bool) or not isinstance(pair_count, int) or pair_count < 1:
        raise ValueError("pair_count must be a positive integer")
    if not first or not second or first == second:
        raise ValueError("first and second must be distinct non-empty labels")
    return tuple(
        (first, second) if index % 2 == 0 else (second, first)
        for index in range(pair_count)
    )


def warmup_order_schedule(*, first: str = "A", second: str = "B") -> tuple[tuple[str, str], ...]:
    """Return one AB and one BA warmup pair (two complete runs per engine)."""

    return pair_order_schedule(2, first=first, second=second)


def _quantile(sorted_values: Sequence[float], probability: float) -> float:
    position = (len(sorted_values) - 1) * probability
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return sorted_values[lower]
    fraction = position - lower
    return sorted_values[lower] + (sorted_values[upper] - sorted_values[lower]) * fraction


def _validated_samples(values: Sequence[float], name: str, *, allow_zero: bool = True) -> list[float]:
    if isinstance(values, (str, bytes)) or len(values) == 0:
        raise ValueError(f"{name} must be a non-empty numeric sequence")
    result: list[float] = []
    for value in values:
        if isinstance(value, bool):
            raise ValueError(f"{name} values must be finite numbers")
        try:
            numeric = float(value)
        except (TypeError, ValueError, OverflowError) as error:
            raise ValueError(f"{name} values must be finite numbers") from error
        if not math.isfinite(numeric) or numeric < 0 or (not allow_zero and numeric <= 0):
            requirement = "positive" if not allow_zero else "non-negative"
            raise ValueError(f"{name} values must be finite and {requirement}")
        result.append(numeric)
    return result


def summarize_samples(samples: Sequence[float]) -> dict[str, object]:
    """Summarize timings and mark IQR/median above 10% inconclusive.

    Quartiles use linear interpolation between adjacent sorted observations.
    The original sample values are retained in the result for audit/reporting.
    """

    numeric = _validated_samples(samples, "samples")
    ordered = sorted(numeric)
    median = float(statistics.median(ordered))
    q1 = _quantile(ordered, 0.25)
    q3 = _quantile(ordered, 0.75)
    iqr = q3 - q1
    iqr_over_median = iqr / median if median > 0 else None
    status = (
        "INCONCLUSIVE"
        if iqr_over_median is None or iqr_over_median > DEFAULT_NOISE_LIMIT
        else "VALID"
    )
    return {
        "samples": list(samples),
        "count": len(numeric),
        "median": median,
        "q1": q1,
        "q3": q3,
        "iqr": iqr,
        "min": min(numeric),
        "max": max(numeric),
        "iqr_over_median": iqr_over_median,
        "noise_limit": DEFAULT_NOISE_LIMIT,
        "protocol": MEASUREMENT_PROTOCOL,
        "status": status,
    }


def _percentile(sorted_values: Sequence[float], probability: float) -> float:
    return _quantile(sorted_values, probability)


def paired_bootstrap_regression(
    reference: Sequence[float],
    candidate: Sequence[float],
) -> dict[str, object]:
    """Classify the candidate/reference median ratio with paired-index bootstrap.

    Each bootstrap draw resamples pair indices with replacement and recomputes
    ``median(candidate_draw) / median(reference_draw)``.  The percentile
    interval is fixed at 95%, using seed 92026 and 10,000 resamples.  A 5%
    regression limit passes only when the upper bound is at most 1.05, fails
    only when the lower bound is above 1.05, and is otherwise inconclusive.
    """

    reference_values = _validated_samples(reference, "reference", allow_zero=False)
    candidate_values = _validated_samples(candidate, "candidate", allow_zero=False)
    if len(reference_values) != len(candidate_values):
        raise ValueError("reference and candidate must have the same number of paired samples")
    if len(reference_values) < 10:
        raise ValueError("at least 10 paired samples are required for regression classification")
    ratio = float(statistics.median(candidate_values) / statistics.median(reference_values))
    rng = random.Random(DEFAULT_BOOTSTRAP_SEED)
    pair_count = len(reference_values)
    bootstrap_ratios: list[float] = []
    for _ in range(DEFAULT_BOOTSTRAP_RESAMPLES):
        indices = [rng.randrange(pair_count) for _ in range(pair_count)]
        reference_median = statistics.median(reference_values[index] for index in indices)
        candidate_median = statistics.median(candidate_values[index] for index in indices)
        bootstrap_ratios.append(float(candidate_median / reference_median))
    bootstrap_ratios.sort()
    lower = _percentile(bootstrap_ratios, 0.025)
    upper = _percentile(bootstrap_ratios, 0.975)
    if upper <= DEFAULT_REGRESSION_LIMIT:
        status = "PASS"
    elif lower > DEFAULT_REGRESSION_LIMIT:
        status = "FAIL"
    else:
        status = "INCONCLUSIVE"
    return {
        "status": status,
        "ratio": ratio,
        "ci_lower": lower,
        "ci_upper": upper,
        "confidence_level": 0.95,
        "regression_limit": DEFAULT_REGRESSION_LIMIT,
        "resamples": DEFAULT_BOOTSTRAP_RESAMPLES,
        "seed": DEFAULT_BOOTSTRAP_SEED,
        "protocol": MEASUREMENT_PROTOCOL,
        "method": "paired_index_percentile_bootstrap_median_ratio",
    }


__all__ = [
    "CommandMeasurement",
    "DEFAULT_BOOTSTRAP_RESAMPLES",
    "DEFAULT_BOOTSTRAP_SEED",
    "DEFAULT_NOISE_LIMIT",
    "DEFAULT_REGRESSION_LIMIT",
    "MEASUREMENT_PROTOCOL",
    "THREAD_ENVIRONMENT",
    "pair_order_schedule",
    "paired_bootstrap_regression",
    "run_command",
    "summarize_samples",
    "thread_limited_environment",
    "warmup_order_schedule",
]
