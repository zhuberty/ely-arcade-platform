#!/bin/bash
# Build the menu and every game submodule (release_x64).
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
build() { ( cd "$1" && (cd build && ../sdk/tools/premake/premake5 gmake) && make config=release_x64 ); }
build "$ROOT"
for g in "$ROOT"/games/*/; do [ -d "$g/build" ] && build "$g"; done
