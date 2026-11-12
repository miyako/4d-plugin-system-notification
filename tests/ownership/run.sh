#!/bin/sh
# Ownership harness: builds the plugin's real sources and the unchanged SDK
# 4DPluginAPI.c against a synthetic Call4D, with ASan/UBSan, and runs the tests.
#
# usage: tests/ownership/run.sh [plugin-tree]   (default: this checkout)
set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=${1:-$(cd "$HERE/../.." && pwd)}
ROOT=$(cd "$ROOT" && pwd)
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

SDK="$ROOT/4D Plugin API"
FLAGS="-g -O0 -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -Wno-everything"
cc() { clang++ $FLAGS -std=c++17 -I"$HERE" -I"$ROOT" -I"$SDK" -I"$SDK/Classes" "$@"; }

# 4DPluginAPI.c and the plugin are built as Objective-C++ on macOS (see the Xcode project).
cc -x objective-c++ -fno-objc-arc -c "$SDK/4DPluginAPI.c" -o "$OUT/sdk.o"
cc -x objective-c++ -fno-objc-arc -c "$ROOT/4DPlugin-System-Notification.cpp" -o "$OUT/plugin.o"
cc -x objective-c++ -fno-objc-arc -c "$SDK/Classes/C_TEXT.cpp" -o "$OUT/c_text.o"
cc -x objective-c++ -fno-objc-arc -c "$SDK/Classes/C_LONGINT.cpp" -o "$OUT/c_longint.o"
cc -x objective-c++ -fno-objc-arc -c "$HERE/call4d.cpp" -o "$OUT/call4d.o"
cc -x objective-c++ -fno-objc-arc -c "$HERE/tests.cpp" -o "$OUT/tests.o"
clang++ $FLAGS "$OUT"/*.o -framework Cocoa -framework IOKit -o "$OUT/ownership"

ASAN_OPTIONS=abort_on_error=0:halt_on_error=1 UBSAN_OPTIONS=print_stacktrace=1 "$OUT/ownership"
