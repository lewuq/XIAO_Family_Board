"""Static syntax check of the xf_* components against the real compile flags.

This script writes no build artefacts: it reuses the compiler and include paths
of the last configured build (`build/compile_commands.json` produced by the
project itself) and runs the compiler with `-fsyntax-only`, which performs the
full parse, warning and error pass without emitting objects, libraries or
firmware images.

Usage:
    python tools/check_syntax.py [relative/source.c ...]

With no arguments every C source of the xf_* components plus the application
entry is checked. Exit code is 0 only when all files compile cleanly.
"""

from __future__ import annotations

import json
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
COMPILE_DB = ROOT / "build" / "compile_commands.json"

# Public/private include directories of the components added by this task; the
# build database predates them, so they are appended explicitly.
EXTRA_INCLUDES = [
    "components/xf_bsp/include",
    "components/xf_core/include",
    "components/xf_data/include",
    "components/xf_data/generated",
    "components/xf_assets/include",
    "components/xf_assets/generated",
    "components/xf_ui/include",
    "components/xf_ui",
    "components/xf_ui/generated",
    "main",
]

DEFAULT_SOURCES = sorted(
    str(p.relative_to(ROOT)).replace("\\", "/")
    for p in list((ROOT / "components").glob("xf_*/**/*.c"))
) + ["main/blink_example_main.c"]


ENTRY_SUFFIXES = ("main/blink_example_main.c", "main/app_main.c")


def base_flags() -> list[str]:
    if not COMPILE_DB.is_file():
        raise SystemExit(f"missing {COMPILE_DB}; configure the project once first")
    entries = json.loads(COMPILE_DB.read_text(encoding="utf-8"))
    entry = None
    for suffix in ENTRY_SUFFIXES:
        for candidate in entries:
            if candidate["file"].replace("\\", "/").endswith(suffix):
                entry = candidate
                break
        if entry is not None:
            break
    if entry is None:
        raise SystemExit("no compile command found for the application entry")

    # shlex in POSIX mode would eat the backslashes of the compiler path;
    # non-POSIX mode keeps them and leaves quotes to strip by hand.
    argv = [tok[1:-1] if len(tok) > 1 and tok[0] == tok[-1] == '"' else tok
            for tok in shlex.split(entry["command"], posix=False)]
    # The database escapes the quotes of string defines for the shell it was
    # written by; the compiler wants the bare literal.
    argv = [tok.replace('\\"', '"') for tok in argv]
    # Drop the output/compile tail so -fsyntax-only can be substituted.
    if "-o" in argv:
        argv = argv[: argv.index("-o")]
    keep: list[str] = []
    skip_next = False
    for arg in argv:
        if skip_next:
            skip_next = False
            continue
        if arg == "-c":
            skip_next = True
            continue
        keep.append(arg)
    return keep


def check(sources: list[str], flags: list[str]) -> int:
    failures = 0
    for source in sources:
        path = ROOT / source
        if not path.is_file():
            print(f"[skip] {source} (missing)")
            continue
        argv = list(flags)
        for include in EXTRA_INCLUDES:
            argv.insert(1, f"-I{(ROOT / include).as_posix()}")
        argv += ["-fsyntax-only", path.as_posix()]
        proc = subprocess.run(argv, cwd=str(ROOT), capture_output=True, text=True)
        if proc.returncode == 0:
            print(f"[ ok ] {source}")
        else:
            failures += 1
            print(f"[FAIL] {source}")
            sys.stdout.write(proc.stderr)
            sys.stdout.write(proc.stdout)
    return failures


def main(argv: list[str]) -> int:
    sources = argv[1:] or DEFAULT_SOURCES
    failures = check(sources, base_flags())
    print(f"{len(sources) - failures}/{len(sources)} files clean")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))