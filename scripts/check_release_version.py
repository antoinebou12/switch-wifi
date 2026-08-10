#!/usr/bin/env python3
from pathlib import Path
import re
import sys


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: check_release_version.py <vX.Y.Z> <CMakeLists.txt>", file=sys.stderr)
        return 2
    tag = sys.argv[1]
    text = Path(sys.argv[2]).read_text(encoding="utf-8")
    match = re.search(r"project\(switch-wifi\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)", text)
    if not match:
        print("project version not found", file=sys.stderr)
        return 1
    expected = f"v{match.group(1)}"
    if tag != expected:
        print(f"tag {tag!r} does not match CMake version {expected!r}", file=sys.stderr)
        return 1
    print(f"release version verified: {tag}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
