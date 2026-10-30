#!/usr/bin/env sh
# Configure and build pp. Extra arguments go to the CMake configure step.
set -eu
cd "$(dirname "$0")"

BUILD_DIR="${BUILD_DIR:-build/release}"
JOBS="${JOBS:-2}"

configure() {
    cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release "$@"
}

if ! configure "$@"; then
    # Debian/Ubuntu without SDL2 headers: fetch them into the user prefix.
    if [ ! -f "$HOME/.local/opt/ppdeps/usr/include/SDL2/SDL.h" ] &&
       [ ! -f /usr/include/SDL2/SDL.h ] &&
       command -v apt-get >/dev/null 2>&1 && command -v dpkg-deb >/dev/null 2>&1; then
        echo "SDL2 development files missing; running scripts/setup_deps.sh" >&2
        ./scripts/setup_deps.sh
        configure "$@"
    else
        echo "Configure failed. Install SDL2 development files and retry." >&2
        exit 1
    fi
fi

cmake --build "$BUILD_DIR" -j "$JOBS"
