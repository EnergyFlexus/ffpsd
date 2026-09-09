#!/usr/bin/env python3
"""Build the benchmarks in Release, run them and show the results.

    python scripts/run_benchmarks.py                    # everything, 10 repetitions
    python scripts/run_benchmarks.py --filter rgb       # a subset, by regular expression
    python scripts/run_benchmarks.py --save before      # keep this run as "before"
    python scripts/run_benchmarks.py --compare before   # run again and compare with "before"

Each run is saved as JSON under build/bench-release/results/, "last" unless
--save names it. Works from any shell: on Windows the MSVC environment is set
up the same way build.py does it.
"""

import argparse
import importlib.util
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
import build  # noqa: E402  the toolchain checks and the MSVC setup of the release build

PRESET = "bench-release"
BUILD_DIR = ROOT / "build" / PRESET
RESULTS_DIR = BUILD_DIR / "results"
EXECUTABLE = BUILD_DIR / "bin" / ("ffpsd_benchmarks.exe" if build.IS_WINDOWS else "ffpsd_benchmarks")
COMPARE = ROOT / "vendor" / "benchmark" / "tools" / "compare.py"
COMPARE_REQUIREMENTS = ROOT / "vendor" / "benchmark" / "tools" / "requirements.txt"


def result_path(name):
    return RESULTS_DIR / f"{name}.json"


def check_benchmark_submodule():
    if not (ROOT / "vendor" / "benchmark" / "CMakeLists.txt").is_file():
        build.run(["git", "submodule", "update", "--init", "--recursive"])


def check_compare_requirements():
    missing = [name for name in ("numpy", "scipy") if importlib.util.find_spec(name) is None]
    if missing:
        build.fail("comparing needs " + " and ".join(missing),
                   f"pip install -r {COMPARE_REQUIREMENTS.relative_to(ROOT)}")


def main():
    parser = argparse.ArgumentParser(description="Build and run the ffpsd benchmarks in Release.")
    parser.add_argument("--filter", help="run only the benchmarks whose name matches this regex")
    parser.add_argument("--repetitions", type=int, default=10,
                        help="runs of each benchmark; the report shows their mean, median "
                             "and spread (default: %(default)s)")
    parser.add_argument("--save", metavar="NAME", default="last",
                        help="name to keep this run under (default: %(default)s)")
    parser.add_argument("--compare", metavar="NAME",
                        help="compare this run with a saved one, such as 'before'")
    parser.add_argument("--no-build", action="store_true",
                        help="run what is already built, without configuring and building")
    args = parser.parse_args()

    baseline = result_path(args.compare) if args.compare else None
    if baseline is not None:
        if not baseline.is_file():
            build.fail(f"no saved run {baseline.relative_to(ROOT)}",
                       f"make one first: python scripts/run_benchmarks.py --save {args.compare}")
        check_compare_requirements()

    cmake = build.check_cmake()
    build.check_vendor()
    check_benchmark_submodule()
    env = build.build_env()

    if not args.no_build:
        build.run([cmake, "--preset", PRESET], env)
        build.run([cmake, "--build", "--preset", PRESET], env)
    if not EXECUTABLE.is_file():
        build.fail(f"{EXECUTABLE.relative_to(ROOT)} is not built", "run without --no-build")

    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    result = result_path(args.save)
    command = [EXECUTABLE,
               f"--benchmark_repetitions={args.repetitions}",
               "--benchmark_display_aggregates_only=true",  # the file keeps every repetition
               f"--benchmark_out={result}",
               "--benchmark_out_format=json"]
    if args.filter:
        command.append(f"--benchmark_filter={args.filter}")

    print()
    build.run(command, env)
    print(f"\nsaved {result.relative_to(ROOT)}")

    if baseline is not None:
        print(f"\n== {args.compare} -> {args.save}")
        build.run([sys.executable, COMPARE, "benchmarks", baseline, result], env)


if __name__ == "__main__":
    main()
