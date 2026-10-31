#!/usr/bin/env python3
"""Write build/python-clangd/compile_commands.json, where .clangd looks for the flags of bindings/python/.

    python scripts/python_compile_commands.py

pip builds the module with the Visual Studio generator on Windows, which writes no such file, so this
configures bindings/python with Ninja and builds nothing. Run it with a Python that has nanobind, and
again after adding a source file or changing Python or nanobind.
"""

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
import build  # noqa: E402  the toolchain checks and the MSVC setup of the release build

BUILD_DIR = ROOT / "build" / "python-clangd"


def main():
    try:
        import nanobind
    except ImportError:
        build.fail(f"nanobind is not installed in {sys.executable}", "python -m pip install -r requirements-dev.txt")

    cmake = build.check_cmake()
    build.check_vendor()
    if build.pick_generator() != "Ninja":
        build.fail("ninja not found on PATH", "python -m pip install -r requirements-dev.txt")
    env = build.build_env()

    build.run(
        [
            cmake,
            "-S",
            ROOT / "bindings" / "python",
            "-B",
            BUILD_DIR,
            "-G",
            "Ninja",
            "-DCMAKE_BUILD_TYPE=Release",
            "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
            f"-DPython_EXECUTABLE={sys.executable}",
            f"-Dnanobind_DIR={nanobind.cmake_dir()}",
        ],
        env,
    )
    print(f"\nwrote {BUILD_DIR / 'compile_commands.json'}; restart clangd to pick it up")


if __name__ == "__main__":
    main()
