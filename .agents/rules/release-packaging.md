---
trigger: always_on
---

# Mandatory Ready-to-Play Windows Release Packaging Rule

Whenever creating or updating a Windows build or GitHub release for OpenCE:
1. **ALWAYS configure with `--portable`** (`python configure.py --portable`) so `halo.exe` runs on all Windows 10/11 CPUs without illegal instruction crashes.
2. **ALWAYS package using `python tools/package_windows_release.py <vX.Y.Z>`** — NEVER manually zip only `halo.exe` and `SDL3.dll` or guess asset paths.
3. Every Windows release zip (`OpenCE-MultiBSP-vX.Y.Z-windows.zip` and `halo-windows-release.zip`) MUST contain the complete 29-file ready-to-play distribution from `tools/ci_build.py`:
   - `halo.exe`
   - `SDL3.dll`
   - `brokers.txt` (from `port/assets/network/brokers.txt`)
   - `masters.txt` (from `port/assets/network/masters.txt`)
   - `init.txt.example` (from `deploy/init.txt.example`)
   - `gametypes/*` (all 15 built-in gametype files from `deploy/gametypes/`)
   - All 9 license files (`LICENSE.txt`, `mbedtls-LICENSE.txt`, `miniupnpc-LICENSE.txt`, `Overpass-OFL.txt`, `expat-COPYING.txt`, `bcdec-LICENSE.txt`, `opus-COPYING.txt`, `jsmn-LICENSE.txt`, `lucide-LICENSE.txt`)
4. Verify the zip file list before running `gh release create` or `gh release upload`.
