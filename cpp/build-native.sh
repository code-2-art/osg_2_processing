#!/usr/bin/env bash
# ============================================================
#  cpplib native 构建脚本（Linux / macOS）
#  用法:  cpp/build-native.sh [linux-amd64 | linux-aarch64 | macosx-aarch64] [-cuda] [-java HOME]
#  产物:  <root>/native/<platform>/libcpplib.so (或 .dylib)
# ============================================================
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PLAT="${1:-}"
shift || true
CUDA=OFF
JAVA=""

while [ $# -gt 0 ]; do
  case "$1" in
    -cuda) CUDA=ON ;;
    -java) JAVA="$2"; shift ;;
  esac
  shift
done

case "$(uname -s)" in
  Linux*)  [ -z "$PLAT" ] && PLAT=linux-amd64 ;;
  Darwin*) [ -z "$PLAT" ] && PLAT=macosx-aarch64 ;;
  *) echo "unsupported host"; exit 1 ;;
esac

OUT_DIR="$ROOT/native/$PLAT"
BUILD_DIR="$ROOT/cpp/build/$PLAT"
mkdir -p "$OUT_DIR"

EXTRA=()
[ -n "$JAVA" ] && EXTRA+=("-DCPPLIB_JAVA_HOME=$JAVA")

cmake -S "$ROOT/cpp" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCPPLIB_OUTPUT_DIR="$OUT_DIR" \
  -DCPPLIB_USE_CUDA="$CUDA" "${EXTRA[@]}"

cmake --build "$BUILD_DIR" -- -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"

echo "artifacts in $OUT_DIR:"
ls -la "$OUT_DIR"
