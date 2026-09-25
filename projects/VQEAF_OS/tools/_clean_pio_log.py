#!/usr/bin/env python3
"""Strip the WorkBuddy sandbox atexit noise out of a captured PlatformIO log.

The sandbox's `sitecustomize.py` refuses PlatformIO's own `os.remove()` calls
on its temp/lock files, so SCons prints an "Exception ignored in atexit
callback" traceback for every TempFileMunge cleanup. That noise is not part of
the build and must not end up in the guide.

Removal is stateful on purpose: blindly dropping indented lines would also eat
legitimate compiler warnings (TFT_eSPI's `#warning` echo lines are indented).
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

START = (
    "[safe-delete]",
    "Exception ignored in atexit callback:",
    "Traceback (most recent call last):",
)
END = re.compile(r"^(SystemExit|[A-Za-z_]*Error)\b")


def clean(text: str) -> str:
    out, dropping = [], False
    for line in text.splitlines():
        if line.startswith(START):
            dropping = True
            continue
        if dropping:
            if END.match(line):
                # the terminating `SystemExit: 1` belongs to the traceback too
                dropping = False
                continue
            # inside a traceback: frame lines, continuations, blanks
            if line.startswith(("  File \"", "    ")) or not line.strip():
                continue
            dropping = False
        out.append(line)
    return "\n".join(out).rstrip("\n") + "\n"


def main() -> int:
    src, dst = Path(sys.argv[1]), Path(sys.argv[2])
    before = src.read_text(encoding="utf-8", errors="replace")
    after = clean(before)
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_text(after, encoding="utf-8")
    dropped = len(before.splitlines()) - len(after.splitlines())
    print(f"{src.name} -> {dst.name}: {len(before.splitlines())} lines, "
          f"dropped {dropped}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
