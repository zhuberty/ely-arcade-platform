#!/bin/bash
# Build and run the menu's unit tests (parsers + background command runner). They need no raylib
# and no cabinet hardware: only g++.
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/ely-arcade-tests"
mkdir -p "$OUT"
g++ -std=c++17 -Wall -Wextra -O1 -o "$OUT/netinfo_test" \
    "$ROOT/tests/netinfo_test.cpp" "$ROOT/src/netinfo.cpp" "$ROOT/src/process.cpp" -pthread
"$OUT/netinfo_test"
"$ROOT/tests/wifi_test.sh"
