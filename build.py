#!/usr/bin/env python3
"""One-command release build for ffpsd.

    python build.py             # shared library, installed into ffpsd-out/
    python build.py --static    # static library instead
    python build.py --clean     # throw the build tree away first

Always builds Release. Works on Windows (MSVC + Ninja), macOS and Linux;
on Windows the MSVC environment is set up automatically, so the script can
be started from any shell.
"""

import argparse
import locale
import os
import platform
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import NoReturn

ROOT = Path(__file__).resolve().parent
OUT_DIR = ROOT / "ffpsd-out"
MIN_CMAKE = (3, 23)
PNG_VENDORED = ("libpng", "zlib-ng")
JPEG_VENDORED = ("libjpeg-turbo",)

IS_WINDOWS = platform.system() == "Windows"
IS_MACOS = platform.system() == "Darwin"

# Host architecture -> vcvarsall.bat argument.
MSVC_ARCH = {"AMD64": "x64", "ARM64": "arm64", "x86": "x86"}


def fail(message, hint=None) -> NoReturn:
    print(f"error: {message}", file=sys.stderr)
    if hint:
        print(f"hint:  {hint}", file=sys.stderr)
    sys.exit(1)


def run(cmd, env=None):
    cmd = [str(part) for part in cmd]
    print("+ " + " ".join(cmd), flush=True)
    if subprocess.run(cmd, cwd=ROOT, env=env).returncode != 0:
        fail(f"command failed: {cmd[0]}")


def capture(cmd, shell=False):
    """Run a command and return its stdout, tolerating odd console encodings."""
    if not shell:
        cmd = [str(part) for part in cmd]
    result = subprocess.run(cmd, shell=shell, capture_output=True)
    encoding = locale.getpreferredencoding(False)
    return result.stdout.decode(encoding, errors="replace")


def check_cmake():
    cmake = shutil.which("cmake")
    if not cmake:
        fail("cmake not found on PATH", cmake_install_hint())

    match = re.search(r"(\d+)\.(\d+)\.(\d+)", capture([cmake, "--version"]))
    if not match:
        fail("could not parse the output of `cmake --version`")
    version = tuple(int(part) for part in match.groups())
    if version[:2] < MIN_CMAKE:
        wanted = ".".join(str(part) for part in MIN_CMAKE)
        found = ".".join(str(part) for part in version)
        fail(f"cmake {wanted} or newer is required, found {found}", cmake_install_hint())

    print(f"cmake      {'.'.join(str(part) for part in version)}")
    return cmake


def cmake_install_hint():
    if IS_WINDOWS:
        return "winget install Kitware.CMake"
    if IS_MACOS:
        return "brew install cmake"
    return "sudo apt install cmake   (or the equivalent for your distribution)"


def pick_generator():
    if shutil.which("ninja"):
        print("generator  Ninja")
        return "Ninja"
    if IS_WINDOWS:
        fail("ninja not found on PATH", "winget install Ninja-build.Ninja")
    print("generator  Unix Makefiles (ninja not found)")
    return "Unix Makefiles"


def build_env():
    """Environment for the cmake calls: MSVC toolchain on Windows, plain elsewhere."""
    if not IS_WINDOWS:
        compiler = next((name for name in ("c++", "g++", "clang++") if shutil.which(name)), None)
        if not compiler:
            hint = "xcode-select --install" if IS_MACOS else "sudo apt install build-essential"
            fail("no C++ compiler found on PATH", hint)
        print(f"compiler   {shutil.which(compiler)}")
        return None

    if shutil.which("cl"):
        print("compiler   cl (already on PATH)")
        return None

    return msvc_env()


def msvc_env():
    """Locate Visual Studio and return the environment vcvarsall.bat produces."""
    program_files = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
    vswhere = Path(program_files) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    vs_hint = "install Visual Studio with the 'Desktop development with C++' workload"
    if not vswhere.is_file():
        fail("vswhere.exe not found, cannot locate Visual Studio", vs_hint)

    installs = capture([
        vswhere, "-latest", "-products", "*",
        "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
        "-property", "installationPath",
    ]).splitlines()
    installs = [line.strip() for line in installs if line.strip()]
    if not installs:
        fail("no Visual Studio installation with the C++ tools found", vs_hint)

    vcvarsall = Path(installs[0]) / "VC" / "Auxiliary" / "Build" / "vcvarsall.bat"
    if not vcvarsall.is_file():
        fail(f"{vcvarsall} is missing", vs_hint)

    arch = MSVC_ARCH.get(platform.machine(), "x64")
    # A single shell string: cmd needs the batch file and `set` in one invocation.
    dump = capture(f'"{vcvarsall}" {arch} >nul && set', shell=True)

    env = dict(line.split("=", 1) for line in dump.splitlines() if "=" in line)
    if "VCINSTALLDIR" not in env:
        fail(f"{vcvarsall.name} {arch} did not produce a usable environment")

    print(f"compiler   MSVC from {installs[0]} ({arch})")
    return env


def wipe(directory):
    if not directory.exists():
        return
    if directory == ROOT or directory in ROOT.parents:
        fail(f"refusing to delete {directory}: it contains the source tree")
    shutil.rmtree(directory)


def show(directory):
    print(f"\ninstalled into {directory}")
    for path in sorted(p for p in directory.rglob("*") if p.is_file()):
        size = path.stat().st_size
        print(f"  {path.relative_to(directory).as_posix():<44}{size / 1024:>8.1f} KiB")


def smoke_test(directory, env):
    """Run the installed img2ffpsd once: it links the library, so this proves it works."""
    tool = directory / "bin" / ("img2ffpsd.exe" if IS_WINDOWS else "img2ffpsd")
    if not tool.is_file():
        return
    print()
    run([tool, "--help"], env)


def check_vendor(names=PNG_VENDORED + JPEG_VENDORED):
    """PNG and JPEG support build libpng, zlib-ng and libjpeg-turbo from the submodules under vendor/."""
    missing = [name for name in names if not (ROOT / "vendor" / name / "CMakeLists.txt").is_file()]
    if not missing:
        return
    if not (ROOT / ".git").exists():
        fail("vendor/ is missing " + ", ".join(missing),
             "download them, or build without them: --no-png, --no-jpeg")
    run(["git", "submodule", "update", "--init", "--recursive"])


def main():
    parser = argparse.ArgumentParser(
        description="Build ffpsd in Release and install it into ffpsd-out/.")
    parser.add_argument("--static", action="store_true",
                        help="build a static library instead of a shared one")
    parser.add_argument("--no-apps", action="store_true",
                        help="skip the apps in apps/, such as img2ffpsd")
    parser.add_argument("--no-png", action="store_true",
                        help="build without PNG loading and saving, so without libpng, zlib-ng and img2ffpsd")
    parser.add_argument("--no-jpeg", action="store_true",
                        help="build without JPEG loading and saving, so without libjpeg-turbo and img2ffpsd")
    parser.add_argument("--crt", choices=("static", "dynamic"),
                        help="MSVC: the C runtime, /MT or /MD (default: static for "
                             "--static, dynamic for a shared library)")
    parser.add_argument("--clean", action="store_true",
                        help="delete the build directory before configuring")
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 1,
                        help="parallel compile jobs (default: %(default)s)")
    parser.add_argument("--out", type=Path, default=OUT_DIR,
                        help="install directory (default: %(default)s)")
    args = parser.parse_args()

    kind = "static" if args.static else "shared"
    crt = args.crt or ("static" if args.static else "dynamic")
    crt_note = f", {crt} CRT" if IS_WINDOWS else ""
    print(f"== ffpsd release build: {kind}{crt_note}, {platform.system()} {platform.machine()}")

    cmake = check_cmake()
    check_vendor((() if args.no_png else PNG_VENDORED) + (() if args.no_jpeg else JPEG_VENDORED))
    generator = pick_generator()
    env = build_env()

    build_dir = ROOT / "build" / f"{kind}-release-out"
    out_dir = args.out.resolve()

    if args.clean:
        wipe(build_dir)
    wipe(out_dir)  # start from an empty tree so no stale files survive

    run([cmake, "-S", ROOT, "-B", build_dir, "-G", generator,
         "-DCMAKE_BUILD_TYPE=Release",
         f"-DBUILD_SHARED_LIBS={'OFF' if args.static else 'ON'}",
         f"-DFFPSD_BUILD_APPS={'OFF' if args.no_apps else 'ON'}",
         f"-DFFPSD_WITH_PNG={'OFF' if args.no_png else 'ON'}",
         f"-DFFPSD_WITH_JPEG={'OFF' if args.no_jpeg else 'ON'}",
         f"-DFFPSD_MSVC_STATIC_RUNTIME={'ON' if crt == 'static' else 'OFF'}",
         f"-DCMAKE_INSTALL_PREFIX={out_dir}"], env)
    run([cmake, "--build", build_dir, "--config", "Release", "--parallel", args.jobs], env)
    run([cmake, "--install", build_dir, "--config", "Release"], env)

    show(out_dir)
    smoke_test(out_dir, env)
    print("\ndone")


if __name__ == "__main__":
    main()
