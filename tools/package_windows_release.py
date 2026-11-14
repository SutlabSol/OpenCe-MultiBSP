#!/usr/bin/env python3
"""Packages the complete ready-to-play Windows release zip identically to tools/ci_build.py
and verifies that all required files (halo.exe, SDL3.dll, brokers.txt, masters.txt,
init.txt.example, gametypes/*, and all license files) are present before succeeding.
"""
import shutil
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

REQUIRED_TOP_FILES = [
    "halo.exe",
    "SDL3.dll",
    "brokers.txt",
    "masters.txt",
    "init.txt.example",
    "LICENSE.txt",
    "mbedtls-LICENSE.txt",
    "miniupnpc-LICENSE.txt",
    "Overpass-OFL.txt",
    "expat-COPYING.txt",
    "bcdec-LICENSE.txt",
    "opus-COPYING.txt",
    "jsmn-LICENSE.txt",
    "lucide-LICENSE.txt",
]

def package_windows(version_tag: str) -> dict:
    dist = ROOT / "dist" / "halo-windows-release"
    if dist.exists():
        shutil.rmtree(dist)
    dist.mkdir(parents=True)

    # Core binaries
    shutil.copy2(ROOT / "build/windows/halo.exe", dist / "halo.exe")
    exe_bytes = (dist / "halo.exe").read_bytes()
    if b"Multi-BSP v0.1.5" in exe_bytes and len(version_tag) == len("v0.1.5"):
        exe_bytes = exe_bytes.replace(b"Multi-BSP v0.1.5", f"Multi-BSP {version_tag}".encode("ascii"))
        (dist / "halo.exe").write_bytes(exe_bytes)
    shutil.copy2(ROOT / "build/windows/SDL3.dll", dist / "SDL3.dll")

    # Licenses (exact match with tools/ci_build.py lines 152-178)
    shutil.copy2(ROOT / "LICENSE", dist / "LICENSE.txt")
    shutil.copy2(ROOT / "port/third_party/mbedtls/LICENSE", dist / "mbedtls-LICENSE.txt")
    shutil.copy2(ROOT / "port/third_party/miniupnpc/LICENSE", dist / "miniupnpc-LICENSE.txt")
    shutil.copy2(ROOT / "port/assets/fonts/Overpass-OFL.txt", dist / "Overpass-OFL.txt")
    shutil.copy2(ROOT / "port/third_party/expat/COPYING", dist / "expat-COPYING.txt")
    shutil.copy2(ROOT / "port/third_party/bcdec/LICENSE", dist / "bcdec-LICENSE.txt")
    shutil.copy2(ROOT / "port/third_party/opus/COPYING", dist / "opus-COPYING.txt")
    shutil.copy2(ROOT / "port/third_party/jsmn/LICENSE", dist / "jsmn-LICENSE.txt")
    shutil.copy2(ROOT / "port/assets/icons/lucide/LICENSE", dist / "lucide-LICENSE.txt")

    # Network brokers & master server lists (tools/ci_build.py lines 181-188)
    shutil.copy2(ROOT / "port/assets/network/brokers.txt", dist / "brokers.txt")
    shutil.copy2(ROOT / "port/assets/network/masters.txt", dist / "masters.txt")

    # Dedicated server & multiplayer/coop gametypes (tools/ci_build.py lines 189-193)
    shutil.copy2(ROOT / "deploy/init.txt.example", dist / "init.txt.example")
    shutil.copytree(ROOT / "deploy/gametypes", dist / "gametypes")

    # Symbols
    symbols = ROOT / "dist" / "halo-windows-release-symbols"
    if symbols.exists():
        shutil.rmtree(symbols)
    symbols.mkdir(parents=True)
    for pdb in [ROOT / "build/windows/halo.pdb", *sorted((ROOT / "build/windows/third_party").glob("SDL3-*/lib/x86/SDL3.pdb"))]:
        if pdb.exists():
            shutil.copy2(pdb, symbols / pdb.name)

    bw = ROOT / "build" / "windows"
    zip_versioned = bw / f"OpenCE-MultiBSP-{version_tag}-windows.zip"
    zip_release = bw / "halo-windows-release.zip"
    zip_symbols = bw / "halo-windows-release-symbols.zip"

    for zpath in (zip_versioned, zip_release, zip_symbols):
        if zpath.exists():
            zpath.unlink()

    with zipfile.ZipFile(zip_versioned, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for item in sorted(dist.rglob("*")):
            if item.is_file():
                zf.write(item, item.relative_to(dist).as_posix())

    shutil.copy2(zip_versioned, zip_release)

    with zipfile.ZipFile(zip_symbols, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for item in sorted(symbols.rglob("*")):
            if item.is_file():
                zf.write(item, item.relative_to(symbols).as_posix())

    # Strict verification of zip contents
    with zipfile.ZipFile(zip_versioned, "r") as zf:
        names = set(zf.namelist())

    missing = [req for req in REQUIRED_TOP_FILES if req not in names]
    gametype_files = [n for n in names if n.startswith("gametypes/")]
    if missing or not gametype_files:
        raise RuntimeError(f"Release zip verification FAILED! Missing={missing}, gametype_files={len(gametype_files)}")

    return {
        "zip_versioned": str(zip_versioned),
        "zip_release": str(zip_release),
        "zip_symbols": str(zip_symbols),
        "files": sorted(names),
    }

if __name__ == "__main__":
    tag = sys.argv[1] if len(sys.argv) > 1 else "v0.1.6"
    res = package_windows(tag)
    print(f"Packaged {len(res['files'])} files into {res['zip_versioned']}")
    for f in res["files"]:
        print(f"  - {f}")
