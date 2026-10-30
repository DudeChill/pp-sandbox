#!/usr/bin/env sh
# Build if needed, then launch the game. Arguments are passed to pp.
set -eu
cd "$(dirname "$0")"
./build.sh
exec "./build/release/pp" "$@"
