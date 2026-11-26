#!/usr/bin/env bash
# Cross-build and stage only. Never uploads, installs, changes Home, or fabricates an IPK.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
: "${SPICE_WEBOS_TOOLCHAIN:?Set SPICE_WEBOS_TOOLCHAIN to your verified SDK CMake toolchainfile.cmake}"
[[ -f "$SPICE_WEBOS_TOOLCHAIN" ]] || { echo 'Toolchain file missing' >&2; exit 2; }
command -v readelf >/dev/null || { echo 'readelf is required for the ARM gate' >&2; exit 2; }
BUILD="${SPICE_WEBOS_BUILD_DIR:-$ROOT/build/webos}"
STAGE="${SPICE_WEBOS_STAGE_DIR:-$ROOT/dist/webos-stage}"
[[ ! -e "$STAGE" ]] || { echo "Stage already exists; choose a new SPICE_WEBOS_STAGE_DIR: $STAGE" >&2; exit 2; }
if [[ -n "${SPICE_PREPARED_DATA:-}" ]]; then
  command -v python3 >/dev/null || { echo 'python3 is required to verify prepared data' >&2; exit 2; }
  python3 "$ROOT/tools/prepare_tv_data.py" verify --bundle "$SPICE_PREPARED_DATA"
fi
cmake -S "$ROOT" -B "$BUILD" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$SPICE_WEBOS_TOOLCHAIN" \
  -DCMAKE_BUILD_TYPE=Release -DSPICE_WEBOS=ON -DSPICE_STRICT=ON -DBUILD_TESTING=OFF
cmake --build "$BUILD" --parallel "${SPICE_JOBS:-4}"
BIN="$BUILD/spice-home-cpp"
readelf -h "$BIN" | grep -Eq 'Class:.*ELF32' || { echo 'Refusing non-ELF32 artifact' >&2; exit 2; }
readelf -h "$BIN" | grep -Eq 'Machine:.*ARM$' || { echo 'Refusing non-ARM artifact' >&2; exit 2; }
cmake --install "$BUILD" --prefix "$STAGE"
if [[ -n "${SPICE_PREPARED_DATA:-}" ]]; then
  python3 "$ROOT/tools/prepare_tv_data.py" stage --bundle "$SPICE_PREPARED_DATA" --out "$STAGE/assets/prepared"
fi
readelf -d "$BIN" > "$BUILD/elf-dependencies.txt"
printf '\nARM executable staged at: %s\n' "$STAGE"
printf 'This is NOT an install-ready IPK. Audit the SDL-webOS and FreeType runtime and transitive dependencies against the C3 before packaging.\n'
printf 'Use docs/WEBOS-BUILD.md. No TV was contacted.\n'
