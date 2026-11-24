
import argparse
import csv
import re
import statistics
import subprocess
from pathlib import Path

# ==========================================
# Paths
# ==========================================

ROOT = Path(__file__).resolve().parent.parent

BUILD_DIR = ROOT / "experimental_builds"
PROGRAM_DIR = ROOT / "guest_programs"
OUTPUT_DIR = ROOT / "analytic_tools" / "results"

BINARIES = {
    "goto": BUILD_DIR / "grrvm_goto",
    "switch": BUILD_DIR / "grrvm_switch",
}

# ==========================================
# Arguments
# ==========================================

parser = argparse.ArgumentParser(
    description="GRRVM Dispatch Performance Benchmark"
)

parser.add_argument("--runs", type=int, default=100)
parser.add_argument("--warmup", type=int, default=10)
parser.add_argument(
    "--program",
    default="iter_test.grr.o"
)

args = parser.parse_args()

PROGRAM = PROGRAM_DIR / args.program
RUNS = args.runs
WARMUP = args.warmup

if RUNS < 2 or WARMUP < 0:
    parser.error("Requires at least 2 runs and non-negative warmup.")

# ==========================================
# Validation
# ==========================================

for binary in BINARIES.values():
    if not binary.is_file():
        raise FileNotFoundError(binary)

if not PROGRAM.is_file():
    raise FileNotFoundError(PROGRAM)

OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

# ==========================================
# Benchmark
# ==========================================

results = {
    name: [] for name in BINARIES
}

TIME_PATTERN = re.compile(
    r"Initial time: (\d+) ns, Final time: (\d+) ns"
)


def benchmark(binary):
    output = subprocess.check_output(
        [str(binary), "-r", str(PROGRAM)],
        cwd=ROOT,
        text=True
    )

    match = TIME_PATTERN.search(output)

    if not match:
        raise RuntimeError(
            f"Execution time not found: {binary}"
        )

    start, end = map(int, match.groups())

    return end - start


def percentile(values, p):
    ordered = sorted(values)

    index = (len(ordered) - 1) * p
    lower = int(index)
    upper = min(lower + 1, len(ordered) - 1)

    return (
        ordered[lower]
        + (ordered[upper] - ordered[lower])
        * (index - lower)
    )


def to_ms(ns):
    return ns / 1_000_000


# ==========================================
# Warm-up
# ==========================================

print("=" * 60)
print("GRRVM DISPATCH BENCHMARK")
print("=" * 60)

print(f"Program : {PROGRAM.name}")
print(f"Runs    : {RUNS}")
print(f"Warm-up : {WARMUP}")

print("\nWarming up...")

for i in range(WARMUP):
    order = list(BINARIES)

    if i % 2:
        order.reverse()

    for name in order:
        benchmark(BINARIES[name])

# ==========================================
# Execution
# ==========================================

print("\nRunning benchmark...")

for i in range(RUNS):
    order = list(BINARIES)

    if i % 2:
        order.reverse()

    for name in order:
        duration = benchmark(BINARIES[name])
        results[name].append(duration)

    print(
        f"\rProgress: {i + 1}/{RUNS}",
        end="",
        flush=True
    )

print("\n")

# ==========================================
# Statistics
# ==========================================

metrics = {
    "Mean": statistics.mean,
    "Median": statistics.median,
    "Minimum": min,
    "Maximum": max,
    "StdDev": statistics.stdev,
    "P95": lambda x: percentile(x, 0.95),
    "P99": lambda x: percentile(x, 0.99),
}

print("=" * 60)
print("BENCHMARK RESULTS")
print("=" * 60)

print(f"{'Metric':<20}{'GOTO (ms)':>18}{'SWITCH (ms)':>18}")
print("-" * 56)

for name, function in metrics.items():
    goto = to_ms(function(results["goto"]))
    switch = to_ms(function(results["switch"]))

    print(f"{name:<20}{goto:>18.6f}{switch:>18.6f}")

# ==========================================
# Performance comparison
# ==========================================

goto_mean = statistics.mean(results["goto"])
switch_mean = statistics.mean(results["switch"])

goto_median = statistics.median(results["goto"])
switch_median = statistics.median(results["switch"])

speedup = switch_mean / goto_mean
reduction = (1 - goto_mean / switch_mean) * 100

median_speedup = switch_median / goto_median
median_reduction = (1 - goto_median / switch_median) * 100

print("\nPERFORMANCE COMPARISON")
print("-" * 56)

print(f"Mean speedup          : {speedup:.4f}x")
print(f"Mean time reduction   : {reduction:.2f}%")
print(f"Median speedup        : {median_speedup:.4f}x")
print(f"Median time reduction : {median_reduction:.2f}%")

for name, samples in results.items():
    cv = (
        statistics.stdev(samples)
        / statistics.mean(samples)
        * 100
    )

    print(f"{name.upper():<6} variability     : {cv:.2f}%")

# ==========================================
# CSV Export
# ==========================================

output_file = (
    OUTPUT_DIR / f"dispatch_{PROGRAM.stem}_{RUNS}.csv"
)

with output_file.open("w", newline="") as file:
    writer = csv.writer(file)

    writer.writerow([
        "run",
        "goto_ns",
        "switch_ns",
        "goto_ms",
        "switch_ms",
        "speedup",
    ])

    for i in range(RUNS):
        goto = results["goto"][i]
        switch = results["switch"][i]

        writer.writerow([
            i + 1,
            goto,
            switch,
            to_ms(goto),
            to_ms(switch),
            switch / goto,
        ])

print(f"\nResults exported to:\n{output_file}")
