#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$ROOT/bin/spice-home-cpp-linux-x86_64"
if [[ ! -x "$BIN" ]]; then
  BIN="$ROOT/build/desktop/spice-home-cpp"
fi
[[ -x "$BIN" ]] || { echo 'No desktop binary. Run scripts/build-desktop.sh.' >&2; exit 2; }
exec "$BIN" --assets "$ROOT/assets" "$@"
