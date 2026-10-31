#!/usr/bin/env python3
"""Build the benchmarks in Release, run them and show the results.

    python scripts/run_benchmarks.py                    # everything, 10 repetitions
    python scripts/run_benchmarks.py --filter Layer/    # a subset, by regular expression
    python scripts/run_benchmarks.py --save before      # keep this run as "before"
    python scripts/run_benchmarks.py --compare before   # run again and compare with "before"
    python scripts/run_benchmarks.py --against v0.4.1   # build that version too, run both, compare

Each run is saved as JSON under build/bench-release/results/, "last" unless
--save names it. --against builds the library of a tag, branch or commit with
the benchmarks of this checkout, in build/bench-<ref>/, so both run the same
code against two libraries; that version needs every call the benchmarks make.
Works from any shell: on Windows the MSVC environment is set up the same way
build.py does it.
"""

import argparse
import importlib.util
import io
import re
import shutil
import subprocess
import sys
import tarfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
import build  # noqa: E402  the toolchain checks and the MSVC setup of the release build

PRESET = "bench-release"
BUILD_DIR = ROOT / "build" / PRESET
RESULTS_DIR = BUILD_DIR / "results"
EXECUTABLE_NAME = "ffpsd_benchmarks.exe" if build.IS_WINDOWS else "ffpsd_benchmarks"
COMPARE = ROOT / "vendor" / "benchmark" / "tools" / "compare.py"

# What a build of the library and the benchmarks takes from vendor/; the rest is for the apps and the tests.
BASELINE_VENDORED = ("benchmark",) + build.PNG_VENDORED + build.JPEG_VENDORED + build.WEBP_VENDORED


def result_path(name):
    return RESULTS_DIR / f"{name}.json"


def check_benchmark_submodule():
    if not (ROOT / "vendor" / "benchmark" / "CMakeLists.txt").is_file():
        build.run(["git", "submodule", "update", "--init", "--recursive"])


def check_compare_requirements():
    missing = [name for name in ("numpy", "scipy") if importlib.util.find_spec(name) is None]
    if missing:
        build.fail("comparing needs " + " and ".join(missing), "pip install -r requirements-dev.txt")


def git(*args, cwd=ROOT):
    return subprocess.run(["git", *args], cwd=cwd, check=True, capture_output=True).stdout


def extract(tree, directory, cwd=ROOT):
    """The files of a commit, as git archive gives them, into directory."""
    with tarfile.open(fileobj=io.BytesIO(git("archive", "--format=tar", tree, cwd=cwd))) as archive:
        if hasattr(tarfile, "data_filter"):
            archive.extractall(directory, filter="data")
        else:
            archive.extractall(directory)


def baseline_sources(ref):
    """The library of ref with the benchmarks of this checkout, in build/bench-<ref>/src."""
    try:
        commit = git("rev-parse", "--verify", f"{ref}^{{commit}}").decode().strip()
    except subprocess.CalledProcessError:
        build.fail(f"{ref} is no tag, branch or commit here", "git fetch --tags")

    directory = ROOT / "build" / ("bench-" + re.sub(r"[^\w.-]", "_", ref))
    source = directory / "src"
    stamp = directory / "commit"
    if not stamp.is_file() or stamp.read_text().strip() != commit:
        build.wipe(source)
        print(f"extracting {ref} ({commit[:10]}) into {source.relative_to(ROOT)}")
        extract(commit, source)

        # The submodules at the commits ref pins; their objects are already in the checkout's submodules.
        for name in BASELINE_VENDORED:
            entry = git("ls-tree", commit, f"vendor/{name}").decode().split()
            if len(entry) < 3 or entry[1] != "commit":
                continue
            try:
                extract(entry[2], source / "vendor" / name, cwd=ROOT / "vendor" / name)
            except subprocess.CalledProcessError:
                build.fail(f"vendor/{name} lacks {entry[2][:10]}, which {ref} pins", f"git -C vendor/{name} fetch origin {entry[2]}")
        directory.mkdir(parents=True, exist_ok=True)
        stamp.write_text(commit + "\n")

    benchmarks = source / "benchmarks"
    build.wipe(benchmarks)
    shutil.copytree(ROOT / "benchmarks", benchmarks)
    return directory


def build_baseline(ref, cmake, env):
    directory = baseline_sources(ref)
    binary = directory / "build"
    build.run(
        [
            cmake,
            "-S",
            directory / "src",
            "-B",
            binary,
            "-G",
            "Ninja",
            "-DCMAKE_BUILD_TYPE=Release",
            "-DBUILD_SHARED_LIBS=OFF",
            "-DFFPSD_MSVC_STATIC_RUNTIME=OFF",
            "-DFFPSD_BUILD_APPS=OFF",
            "-DFFPSD_BUILD_TESTS=OFF",
            "-DFFPSD_BUILD_EXAMPLES=OFF",
            "-DFFPSD_BUILD_BENCHMARKS=ON",
            f"-DFFPSD_BENCHMARK_DATA_DIR={(ROOT / 'tests' / 'data').as_posix()}",
        ],
        env,
    )
    build.run([cmake, "--build", binary, "--target", "ffpsd_benchmarks"], env)
    return binary / "bin" / EXECUTABLE_NAME


def run_benchmarks(executable, args, result, env):
    command = [
        executable,
        f"--benchmark_repetitions={args.repetitions}",
        "--benchmark_display_aggregates_only=true",  # the file keeps every repetition
        f"--benchmark_out={result}",
        "--benchmark_out_format=json",
    ]
    if args.filter:
        command.append(f"--benchmark_filter={args.filter}")
    print()
    build.run(command, env)
    print(f"\nsaved {result.relative_to(ROOT)}")


def main():
    parser = argparse.ArgumentParser(description="Build and run the ffpsd benchmarks in Release.")
    parser.add_argument("--filter", help="run only the benchmarks whose name matches this regex, such as Formats/")
    parser.add_argument(
        "--repetitions",
        type=int,
        default=10,
        help="runs of each benchmark; the report shows their mean, median and spread (default: %(default)s)",
    )
    parser.add_argument("--save", metavar="NAME", default="last", help="name to keep this run under (default: %(default)s)")
    which = parser.add_mutually_exclusive_group()
    which.add_argument("--compare", metavar="NAME", help="compare this run with a saved one, such as 'before'")
    which.add_argument(
        "--against",
        metavar="REF",
        help="build the library of a tag, branch or commit with these benchmarks, run it before this one and compare",
    )
    parser.add_argument("--no-build", action="store_true", help="run what is already built, without configuring and building")
    args = parser.parse_args()

    baseline = result_path(args.compare) if args.compare else None
    if baseline is not None and not baseline.is_file():
        build.fail(f"no saved run {baseline.relative_to(ROOT)}", f"make one first: python scripts/run_benchmarks.py --save {args.compare}")
    if args.compare or args.against:
        check_compare_requirements()

    cmake = build.check_cmake()
    build.check_vendor()
    check_benchmark_submodule()
    env = build.build_env()

    executable = BUILD_DIR / "bin" / EXECUTABLE_NAME
    if not args.no_build:
        build.run([cmake, "--preset", PRESET], env)
        build.run([cmake, "--build", "--preset", PRESET], env)
    if not executable.is_file():
        build.fail(f"{executable.relative_to(ROOT)} is not built", "run without --no-build")

    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    if args.against:
        other = build_baseline(args.against, cmake, env)
        baseline = result_path(re.sub(r"[^\w.-]", "_", args.against))
        run_benchmarks(other, args, baseline, env)

    result = result_path(args.save)
    run_benchmarks(executable, args, result, env)

    if baseline is not None:
        print(f"\n== {baseline.stem} -> {args.save}")
        build.run([sys.executable, COMPARE, "benchmarks", baseline, result], env)


if __name__ == "__main__":
    main()
