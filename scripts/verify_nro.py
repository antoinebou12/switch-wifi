#!/usr/bin/env python3
from pathlib import Path
import struct
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

    nro_size = struct.unpack_from("<I", data, 0x18)[0]
    asset_header_size = 0x38
    if nro_size + asset_header_size > len(data) or data[nro_size:nro_size + 4] != b"ASET":
        print("missing NRO asset header", file=sys.stderr)
        return 1

    sections = {
        "icon": struct.unpack_from("<QQ", data, nro_size + 0x08),
        "nacp": struct.unpack_from("<QQ", data, nro_size + 0x18),
        "romfs": struct.unpack_from("<QQ", data, nro_size + 0x28),
    }
    for name, (offset, size) in sections.items():
        if size == 0:
            print(f"missing {name} asset", file=sys.stderr)
            return 1
        if offset < asset_header_size or nro_size + offset + size > len(data):
            print(f"invalid {name} asset bounds", file=sys.stderr)
            return 1

    print(
        f"verified {path}: {len(data)} bytes, NRO0/ASET headers and "
        f"{sections['romfs'][1]}-byte ROMFS present"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
