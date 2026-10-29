#!/usr/bin/env python3
"""Add the symbols of the vendored libraries that lack the ffpsd_ prefix to the headers in cmake/prefix/.

    python scripts/update_prefixes.py             # build static-debug, update, check
    python scripts/update_prefixes.py --no-check  # without the vendored_symbols test

Each round builds the libraries with the current renames and adds what they still define unprefixed;
the next round builds them with the new renames, so it ends when a round adds nothing. Run it after
updating a library in vendor/, and on each system, since some symbols exist only on one of them.

Works from any shell: on Windows the MSVC environment is set up the same way build.py does it.
"""

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
import build  # noqa: E402  the toolchain checks and the MSVC setup of the release build

PRESET = "static-debug"
PREFIX_DIR = ROOT / "cmake" / "prefix"
RENAME = re.compile(r"^#define (\w+) ffpsd_\1$", re.MULTILINE)
MAX_ROUNDS = 4


def renames():
    """The renamed names of each header in cmake/prefix/."""
    return {path.name: set(RENAME.findall(path.read_text(encoding="utf-8")))
            for path in sorted(PREFIX_DIR.glob("*.h"))}


def main():
    parser = argparse.ArgumentParser(description="Add the unprefixed symbols of the vendored libraries "
                                                 "to cmake/prefix/ and check the static library.")
    parser.add_argument("--no-check", action="store_true", help="skip the vendored_symbols test")
    args = parser.parse_args()

    cmake = build.check_cmake()
    ctest = Path(cmake).with_name("ctest.exe" if build.IS_WINDOWS else "ctest")  # installed beside cmake
    build.check_vendor()
    env = build.build_env()
    build.run([cmake, "--preset", PRESET], env)

    start = renames()
    for round_number in range(1, MAX_ROUNDS + 1):
        before = renames()
        print(f"\n== round {round_number}")
        build.run([cmake, "--build", "--preset", PRESET, "--target", "update_vendor_prefixes"], env)
        if renames() == before:
            break
    else:
        build.fail(f"the headers still change after {MAX_ROUNDS} rounds",
                   "a library undoes some renames with macros of its own: see cmake/prefix/")

    end = renames()
    print()
    for header, names in end.items():
        added = sorted(names - start.get(header, set()))
        print(f"{header}: {len(names)} renames" + (f", {len(added)} new: {' '.join(added)}" if added else ""))

    if not args.no_check:
        build.run([cmake, "--build", "--preset", PRESET], env)
        build.run([ctest, "--preset", PRESET, "-R", "^vendored_symbols$"], env)
    print("\ncmake/prefix/ is up to date")


if __name__ == "__main__":
    main()
