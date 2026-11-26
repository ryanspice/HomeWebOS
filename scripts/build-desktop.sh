#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${SPICE_BUILD_DIR:-$ROOT/build/desktop}"
cmake -S "$ROOT" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release -DSPICE_STRICT=ON
cmake --build "$BUILD" --parallel "${SPICE_JOBS:-4}"
ctest --test-dir "$BUILD" --output-on-failure
printf 'Native executable: %s\n' "$BUILD/spice-home-cpp"
