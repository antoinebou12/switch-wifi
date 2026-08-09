#!/usr/bin/env python3
from pathlib import Path
import sys


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: verify_nro.py <file.nro>", file=sys.stderr)
        return 2
    path = Path(sys.argv[1])
    if not path.is_file():
        print(f"missing NRO: {path}", file=sys.stderr)
        return 1
    data = path.read_bytes()
    if len(data) < 0x40:
        print(f"NRO is too small: {len(data)} bytes", file=sys.stderr)
        return 1
    if data[0x10:0x14] != b"NRO0":
        print("invalid NRO magic at offset 0x10", file=sys.stderr)
        return 1
    print(f"verified {path}: {len(data)} bytes, NRO0 header present")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
