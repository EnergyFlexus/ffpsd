#!/usr/bin/env python3
"""Build the tests in Debug, run them and show the results.

    python scripts/run_tests.py                     # static-debug
    python scripts/run_tests.py --shared            # shared-debug, the DLL build
    python scripts/run_tests.py --all               # both, one after the other
    python scripts/run_tests.py --filter ReadPsd    # only the tests whose name matches
    python scripts/run_tests.py --no-build          # run what is already built

Works from any shell: on Windows the MSVC environment is set up the same way
build.py does it.
"""

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
import build  # noqa: E402  the toolchain checks and the MSVC setup of the release build

TEST_DATA = ROOT / "tests" / "data"
LFS_POINTER = b"version https://git-lfs.github.com/spec/"


def check_googletest_submodule():
    if not (ROOT / "vendor" / "googletest" / "CMakeLists.txt").is_file():
        build.run(["git", "submodule", "update", "--init", "--recursive"])


def check_test_data():
    """A clone without Git LFS has pointer files where the PSDs should be."""
    pointers = [path.name for path in sorted(TEST_DATA.glob("*.ps[db]"))
                if path.read_bytes()[:len(LFS_POINTER)] == LFS_POINTER]
    if pointers:
        build.fail("tests/data holds Git LFS pointers, not files: " + ", ".join(pointers),
                   "git lfs install && git lfs pull")


def main():
    parser = argparse.ArgumentParser(description="Build and run the ffpsd tests in Debug.")
    kind = parser.add_mutually_exclusive_group()
    kind.add_argument("--shared", action="store_true", help="test the DLL build (shared-debug)")
    kind.add_argument("--all", action="store_true", help="test the static build, then the DLL build")
    parser.add_argument("--filter", help="run only the tests whose name matches this regex")
    parser.add_argument("--no-build", action="store_true",
                        help="run what is already built, without configuring and building")
    args = parser.parse_args()

    if args.all:
        presets = ["static-debug", "shared-debug"]
    else:
        presets = ["shared-debug" if args.shared else "static-debug"]

    cmake = build.check_cmake()
    ctest = Path(cmake).with_name("ctest.exe" if build.IS_WINDOWS else "ctest")  # installed beside cmake
    build.check_vendor()
    check_googletest_submodule()
    check_test_data()
    env = build.build_env()

    for preset in presets:
        print(f"\n== {preset}")
        if not args.no_build:
            build.run([cmake, "--preset", preset], env)
            build.run([cmake, "--build", "--preset", preset], env)
        if not (ROOT / "build" / preset / "CTestTestfile.cmake").is_file():
            build.fail(f"build/{preset} is not built", "run without --no-build")

        command = [ctest, "--preset", preset]
        if args.filter:
            command += ["-R", args.filter]
        build.run(command, env)

    print("\nall tests passed")


if __name__ == "__main__":
    main()
