#!/usr/bin/env python3
"""Build the tests with clang's source-based coverage, run them and report how much of ffpsd they cover.

    python scripts/run_coverage.py                        # a summary per file of ffpsd/src and ffpsd/include
    python scripts/run_coverage.py --html build/cov       # and a report to browse, line by line
    python scripts/run_coverage.py --save before.json     # the covered lines and branches, for --compare
    python scripts/run_coverage.py --compare before.json  # every line and branch the tests no longer reach
    python scripts/run_coverage.py --no-build             # run what build/coverage already holds

Needs clang with llvm-profdata and llvm-cov. On Windows that is clang-cl from Visual Studio's
"C++ Clang tools" component, with the MSVC environment set up as build.py does it.
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
import build  # noqa: E402  the toolchain checks and the MSVC setup of the release build

BUILD_DIR = ROOT / "build" / "coverage"
OURS = ("ffpsd/src/", "ffpsd/include/")
IGNORED = r"(vendor|tests|build)[\\/]"


def llvm_tools(env):
    """clang, llvm-profdata and llvm-cov: next to Visual Studio's clang-cl, or on the PATH."""
    if build.IS_WINDOWS:
        vc = (env or os.environ).get("VCINSTALLDIR")
        folder = Path(vc) / "Tools" / "Llvm" / "x64" / "bin" if vc else None
        if folder and (folder / "clang-cl.exe").is_file():
            return folder / "clang-cl.exe", folder / "llvm-profdata.exe", folder / "llvm-cov.exe"
        names = ("clang-cl", "llvm-profdata", "llvm-cov")
        hint = "add 'C++ Clang tools for Windows' to Visual Studio"
    else:
        names = ("clang++", "llvm-profdata", "llvm-cov")
        hint = "xcode-select --install" if build.IS_MACOS else "sudo apt install clang llvm"

    tools = []
    for name in names:
        path = shutil.which(name)
        if not path and build.IS_MACOS:
            path = build.capture(["xcrun", "--find", name]).strip() or None
        if not path:
            build.fail(f"{name} not found", hint)
        tools.append(Path(path))
    return tuple(tools)


def profile_runtime(clang):
    """Windows links through the linker itself, which needs clang's profile runtime by name, at a path without spaces."""
    found = sorted(clang.parent.parent.glob("lib/clang/*/lib/windows/clang_rt.profile-x86_64.lib"))
    if not found:
        build.fail(f"no clang_rt.profile-x86_64.lib next to {clang}")
    target = BUILD_DIR.parent / "coverage-runtime" / found[-1].name
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(found[-1], target)
    return target


def configure_and_build(cmake, clang, env):
    flags = "-fprofile-instr-generate -fcoverage-mapping"
    command = [cmake, "-S", ROOT, "-B", BUILD_DIR, "-G", build.pick_generator(),
               "-DCMAKE_BUILD_TYPE=Debug",
               "-DFFPSD_BUILD_TESTS=ON", "-DFFPSD_BUILD_APPS=OFF",
               "-DFFPSD_BUILD_BENCHMARKS=OFF", "-DFFPSD_BUILD_EXAMPLES=OFF",
               f"-DCMAKE_CXX_FLAGS_INIT={flags}"]
    if build.IS_WINDOWS:
        linker = clang.with_name("lld-link.exe")
        command += [f"-DCMAKE_C_COMPILER={clang.as_posix()}", f"-DCMAKE_CXX_COMPILER={clang.as_posix()}",
                    # clang-cl ignores MSVC's #pragma optimize in zlib-ng, loudly.
                    "-DCMAKE_C_FLAGS_INIT=-Wno-ignored-pragma-optimize",
                    f"-DCMAKE_EXE_LINKER_FLAGS_INIT={profile_runtime(clang).as_posix()}"]
        if linker.is_file():
            command.append(f"-DCMAKE_LINKER={linker.as_posix()}")
    else:
        command += [f"-DCMAKE_C_COMPILER={clang.with_name('clang')}", f"-DCMAKE_CXX_COMPILER={clang}",
                    "-DCMAKE_EXE_LINKER_FLAGS_INIT=-fprofile-instr-generate"]
    build.run(command, env)
    # The build runs the tests once to list them; their profile goes next to the build, not into the source tree.
    build.run([cmake, "--build", BUILD_DIR], dict(env or os.environ, LLVM_PROFILE_FILE=str(BUILD_DIR / "discovery.profraw")))


def relative(path):
    try:
        return Path(path).resolve().relative_to(ROOT).as_posix()
    except ValueError:
        return Path(path).as_posix()


def covered(llvm_cov, tests, profile):
    """The covered state of every line and branch of ffpsd's own code, and the totals llvm-cov counts."""
    base = [llvm_cov, "export", tests, f"-instr-profile={profile}", f"-ignore-filename-regex={IGNORED}"]
    lcov = build.capture(base + ["-format=lcov"])
    summary = json.loads(build.capture(base + ["-summary-only"]))

    lines, branches, file = {}, {}, ""
    for row in lcov.splitlines():
        if row.startswith("SF:"):
            file = relative(row[3:])
        elif not file.startswith(OURS):
            continue
        elif row.startswith("DA:"):
            line, count = row[3:].split(",")[:2]
            lines.setdefault(file, {})[line] = int(count) > 0
        elif row.startswith("BRDA:"):
            line, block, branch, taken = row[5:].split(",")
            branches.setdefault(file, {})[f"{line}:{block}:{branch}"] = taken not in ("-", "0")

    totals = {kind: [0, 0] for kind in ("lines", "regions", "functions", "branches")}
    for entry in summary["data"][0]["files"]:
        if relative(entry["filename"]).startswith(OURS):
            for kind, pair in totals.items():
                part = entry["summary"].get(kind, {"covered": 0, "count": 0})
                pair[0] += part["covered"]
                pair[1] += part["count"]
    return {"lines": lines, "branches": branches, "totals": totals}


def compare(old, new):
    """Prints what the old run covered and the new one does not; the number of such lines and branches."""
    lost = 0
    for kind in ("lines", "branches"):
        for file, marks in sorted(old[kind].items()):
            for mark, was_covered in marks.items():
                if was_covered and not new[kind].get(file, {}).get(mark, False):
                    print(f"  lost {kind[:-1]:<7} {file}:{mark}")
                    lost += 1
    for kind, (covered_count, count) in new["totals"].items():
        before = old["totals"].get(kind, [0, 0])
        print(f"  {kind:<10} {before[0]}/{before[1]} -> {covered_count}/{count}")
    return lost


def main():
    parser = argparse.ArgumentParser(description="Measure how much of ffpsd the tests cover, with clang.")
    parser.add_argument("--html", type=Path, help="also write a report to browse into this directory")
    parser.add_argument("--save", type=Path, help="save the covered lines and branches into this JSON file")
    parser.add_argument("--compare", type=Path, help="fail when a line or branch this saved run covered is no longer reached")
    parser.add_argument("--no-build", action="store_true", help="run what build/coverage already holds")
    args = parser.parse_args()

    cmake = build.check_cmake()
    build.check_vendor()
    env = build.build_env()
    clang, llvm_profdata, llvm_cov = llvm_tools(env)
    print(f"coverage   {clang}")

    if not args.no_build:
        configure_and_build(cmake, clang, env)
    tests = BUILD_DIR / "bin" / ("ffpsd_tests.exe" if build.IS_WINDOWS else "ffpsd_tests")
    if not tests.is_file():
        build.fail(f"{relative(tests)} is not built", "run without --no-build")

    raw = BUILD_DIR / "ffpsd_tests.profraw"
    profile = BUILD_DIR / "ffpsd_tests.profdata"
    raw.unlink(missing_ok=True)
    build.run([tests, "--gtest_brief=1"], dict(env or os.environ, LLVM_PROFILE_FILE=str(raw)))
    build.run([llvm_profdata, "merge", "-sparse", raw, "-o", profile])

    print()
    build.run([llvm_cov, "report", tests, f"-instr-profile={profile}", f"-ignore-filename-regex={IGNORED}"])
    if args.html:
        build.run([llvm_cov, "show", tests, f"-instr-profile={profile}", f"-ignore-filename-regex={IGNORED}",
                   "-format=html", f"-output-dir={args.html}", "-show-branches=count"])
        print(f"\nreport in {args.html / 'index.html'}")

    result = covered(llvm_cov, tests, profile)
    if args.save:
        args.save.write_text(json.dumps(result))
        print(f"\nsaved into {args.save}")
    if args.compare:
        print(f"\nagainst {args.compare}:")
        lost = compare(json.loads(args.compare.read_text()), result)
        if lost:
            build.fail(f"{lost} lines and branches are no longer covered")
        print("  nothing is lost")


if __name__ == "__main__":
    main()
