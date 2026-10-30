#!/usr/bin/env bash
# Fetch the SDL2 development headers without root. The runtime library is
# already installed on this host; only headers, the linker symlink, and
# pkg-config/CMake files are missing.
set -euo pipefail

ROOT="${PP_SDL2_ROOT:-$HOME/.local/opt/ppdeps}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

if [[ -f "$ROOT/usr/include/SDL2/SDL.h" ]]; then
    echo "SDL2 headers already at $ROOT"
    exit 0
fi

cd "$TMP"
apt-get download libsdl2-dev
mkdir -p "$ROOT"
dpkg-deb -x libsdl2-dev_*.deb "$ROOT"
echo "SDL2 headers installed to $ROOT"
