#!/usr/bin/env python3
"""Write the Rust declarations of ffpsd/c_api.h into bindings/rust/ffpsd-sys/src/bindings.rs.

    python scripts/generate_rust_bindings.py           # write the file
    python scripts/generate_rust_bindings.py --check   # fail when the file differs from what bindgen writes

Needs bindgen, libclang and rustfmt. CI checks with those of Ubuntu 24.04, whose output can differ from
other versions, so generate the same way:

    docker run --rm -v "$PWD:/src" -w /src ubuntu:24.04 sh -c \
        "apt-get update -qq && apt-get install -y -qq bindgen clang rustfmt python3 >/dev/null && python3 scripts/generate_rust_bindings.py"
"""

import argparse
import difflib
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HEADER = ROOT / "ffpsd" / "include" / "ffpsd" / "c_api.h"
OUTPUT = ROOT / "bindings" / "rust" / "ffpsd-sys" / "src" / "bindings.rs"
FIRST_LINE = "// Generated from ffpsd/c_api.h by scripts/generate_rust_bindings.py; do not edit.\n"


def generate():
    for tool in ("bindgen", "rustfmt"):
        if not shutil.which(tool):
            sys.exit(f"error: {tool} is not on the PATH; see the top of {Path(__file__).name}")
    command = [
        "bindgen",
        str(HEADER),
        "--allowlist-function",
        "ffpsd_.*",
        "--allowlist-type",
        "ffpsd_.*",
        "--allowlist-var",
        "FFPSD_.*",
        "--no-prepend-enum-name",
        "--no-layout-tests",
        "--no-doc-comments",
        "--merge-extern-blocks",
        "--sort-semantically",
        "--formatter",
        "rustfmt",
        "--",
        f"-I{HEADER.parent.parent}",
        "-DFFPSD_STATIC",
    ]
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        sys.exit("error: bindgen failed\n" + result.stderr)
    text = result.stdout
    # The first line names the bindgen version, which would make every version a difference.
    lines = text.splitlines(keepends=True)
    if lines and lines[0].startswith("/* automatically generated"):
        lines = lines[1:]
    return FIRST_LINE + "".join(lines)


def main():
    parser = argparse.ArgumentParser(description="Generate the Rust bindings of ffpsd's C API.")
    parser.add_argument("--check", action="store_true", help="compare instead of writing")
    args = parser.parse_args()

    text = generate()
    if args.check:
        current = OUTPUT.read_text(encoding="utf-8") if OUTPUT.exists() else ""
        if current != text:
            sys.stdout.writelines(
                difflib.unified_diff(current.splitlines(True), text.splitlines(True), str(OUTPUT.relative_to(ROOT)), "bindgen")
            )
            sys.exit(f"error: {OUTPUT.relative_to(ROOT)} is out of date; run {Path(__file__).name}")
        print(f"{OUTPUT.relative_to(ROOT)} is up to date")
    else:
        OUTPUT.write_text(text, encoding="utf-8", newline="\n")
        print(f"wrote {OUTPUT.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
