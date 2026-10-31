#!/usr/bin/env python3
"""Write the Rust declarations of ffpsd/c_api.h into bindings/rust/ffpsd-sys/src/bindings.rs.

    python scripts/generate_rust_bindings.py               # write the file
    python scripts/generate_rust_bindings.py --check       # fail when the file differs from what bindgen writes
    python scripts/generate_rust_bindings.py --no-docker   # with the bindgen, libclang and rustfmt on the PATH

By default it runs bindgen of Ubuntu 24.04 in Docker, as CI checks with it: other versions can write the file
differently.
"""

import argparse
import difflib
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HEADER = ROOT / "ffpsd" / "include" / "ffpsd" / "c_api.h"
OUTPUT = ROOT / "bindings" / "rust" / "ffpsd-sys" / "src" / "bindings.rs"
FIRST_LINE = "// Generated from ffpsd/c_api.h by scripts/generate_rust_bindings.py; do not edit.\n"
IMAGE = "ubuntu:24.04"


def generate_in_docker():
    if not shutil.which("docker"):
        sys.exit("error: docker is not on the PATH; install it, or run with --no-docker")
    script = (
        "apt-get update -qq && apt-get install -y -qq bindgen clang rustfmt python3 >/dev/null"
        f" && python3 scripts/{Path(__file__).name} --no-docker --print"
    )
    # Read-only: the container only prints, and this side writes, so the file keeps its owner.
    command = ["docker", "run", "--rm", "-v", f"{ROOT}:/src:ro", "-w", "/src", IMAGE, "sh", "-c", script]
    result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8")
    if result.returncode != 0:
        sys.exit(f"error: bindgen in Docker failed; is Docker running? --no-docker skips it\n{result.stderr}")
    return result.stdout


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
    # Outside the checkout, so rustfmt keeps its defaults instead of the repository's rustfmt.toml.
    with tempfile.TemporaryDirectory() as outside:
        result = subprocess.run(command, capture_output=True, text=True, cwd=outside)
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
    parser.add_argument("--no-docker", action="store_true", help="use the bindgen on the PATH instead of Docker")
    # What the run in Docker asks of itself: the text on stdout, for the outer run to write or compare.
    parser.add_argument("--print", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()

    text = generate() if args.no_docker else generate_in_docker()
    if args.print:
        sys.stdout.write(text)
    elif args.check:
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
