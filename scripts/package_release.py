#!/usr/bin/env python3
from pathlib import Path
import hashlib
import shutil
import sys
import zipfile


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> int:
    if len(sys.argv) != 4:
        print("usage: package_release.py <vX.Y.Z> <switch-wifi.nro> <dist-dir>", file=sys.stderr)
        return 2
    tag, nro_arg, dist_arg = sys.argv[1:]
    nro = Path(nro_arg)
    dist = Path(dist_arg)
    dist.mkdir(parents=True, exist_ok=True)
    release_nro = dist / "switch-wifi.nro"
    shutil.copy2(nro, release_nro)

    checksum = sha256(release_nro)
    (dist / "SHA256SUMS").write_text(f"{checksum}  switch-wifi.nro\n", encoding="utf-8")

    archive = dist / f"switch-wifi-{tag}.zip"
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as bundle:
        bundle.write(release_nro, "switch-wifi.nro")
        bundle.write("README.md", "README.md")
        bundle.write("LICENSE", "LICENSE")
        bundle.write(dist / "SHA256SUMS", "SHA256SUMS")
    print(f"created {archive}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
