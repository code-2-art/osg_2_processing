#!/usr/bin/env bash
# ============================================================
#  osgverse_lib 一键构建 + 安装到 Processing sketchbook（Linux / macOS）
#
#  用法：
#     bash osgverse_lib/build.sh
#
#  可选开关：
#     --skip-native     跳过 C++/CMake 构建（仅打包 Java + 组装）
#     --skip-install    不安装到 Processing sketchbook
#     --java-home <p>   指定 JDK（默认使用 Processing 自带 JDK）
#     --osgverse-sdk <p> 指定 osgVerse SDK 路径（默认探测 ~/Public/osgverse/build/sdk_core）
#
#  产物：
#     osgverse_lib/build/osgverselib.jar
#     osgverse_lib/native/linux-amd64/libosgverselib.so
#     osgverse_lib/dist/osgverselib/            完整可发布目录
#     <sketchbook>/libraries/osgverselib        安装结果
# ============================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
LIBNAME="osgverselib"

SKIP_NATIVE=0
SKIP_INSTALL=0
JAVA_HOME_OVERRIDE=""
SDK_OVERRIDE=""

while [ $# -gt 0 ]; do
  case "$1" in
    --skip-native) SKIP_NATIVE=1 ;;
    --skip-install) SKIP_INSTALL=1 ;;
    --java-home) JAVA_HOME_OVERRIDE="$2"; shift ;;
    --osgverse-sdk) SDK_OVERRIDE="$2"; shift ;;
    *) echo "unknown option: $1"; exit 1 ;;
  esac
  shift
done

step() { echo ""; echo "=== $1 ==="; }

# ---------- 1. 定位 Processing / JDK ----------
PROC_HOME="${PROCESSING_HOME:-}"
if [ -z "$PROC_HOME" ]; then
  for cand in \
    "$HOME/Public/Misc/Processing/lib" \
    "$HOME/Processing/lib" \
    "/opt/processing/lib" \
    "/usr/lib/processing/lib" ; do
    if [ -d "$cand" ]; then PROC_HOME="$cand"; break; fi
  done
fi
if [ -z "$PROC_HOME" ]; then
  echo "Cannot find Processing. Set PROCESSING_HOME (to the dir containing lib/)." >&2
  exit 1
fi
echo "Processing home: $PROC_HOME"

if [ -n "$JAVA_HOME_OVERRIDE" ]; then
  JAVA_HOME="$JAVA_HOME_OVERRIDE"
elif [ -d "$PROC_HOME/app/resources/jdk" ]; then
  JAVA_HOME="$PROC_HOME/app/resources/jdk"
fi
if [ -z "${JAVA_HOME:-}" ] || [ ! -x "$JAVA_HOME/bin/javac" ]; then
  echo "JDK not found. Pass --java-home <path>." >&2
  exit 1
fi
JAVAC="$JAVA_HOME/bin/javac"
JAR="$JAVA_HOME/bin/jar"
step "Using JDK: $JAVA_HOME"

CORE_JAR=$(ls "$PROC_HOME"/app/resources/core/library/core-*.jar 2>/dev/null | sort | tail -n1 || true)
if [ -z "$CORE_JAR" ]; then
  echo "Processing core jar not found." >&2
  exit 1
fi
step "Processing core: $CORE_JAR"

# ---------- osgVerse SDK ----------
OSGVERSE_SDK="$SDK_OVERRIDE"
if [ -z "$OSGVERSE_SDK" ] && [ -n "${OSGVERSE_SDK_ROOT:-}" ]; then
  OSGVERSE_SDK="$OSGVERSE_SDK_ROOT"
fi
if [ -z "$OSGVERSE_SDK" ]; then
  for cand in "$HOME/Public/osgverse/build/sdk_core"; do
    if [ -d "$cand/include/osgVerse/pipeline" ]; then OSGVERSE_SDK="$cand"; break; fi
  done
fi
if [ -z "$OSGVERSE_SDK" ] || [ ! -f "$OSGVERSE_SDK/include/osgVerse/pipeline/Pipeline.h" ]; then
  echo "osgVerse SDK not found. Pass --osgverse-sdk <path> (must contain include/osgVerse/pipeline/Pipeline.h)." >&2
  exit 1
fi
step "osgVerse SDK: $OSGVERSE_SDK"

# ---------- 2. 编译 Java 并生成 JNI 头 ----------
SRC_ROOT="$SCRIPT_DIR/java"
CLASSES_DIR="$SCRIPT_DIR/build/classes"
HDR_DIR="$SCRIPT_DIR/native/include"
JAR_FILE="$SCRIPT_DIR/build/$LIBNAME.jar"
mkdir -p "$CLASSES_DIR" "$HDR_DIR"

step "javac (regenerates native/include/org_example_osgverselib_NativeOsgVerseCore.h)"
SOURCES=$(find "$SRC_ROOT" -name '*.java')
"$JAVAC" -encoding UTF-8 -h "$HDR_DIR" -d "$CLASSES_DIR" -classpath "$CORE_JAR" $SOURCES
"$JAR" cf "$JAR_FILE" -C "$CLASSES_DIR" .
echo "jar -> $JAR_FILE"

GEN_HEADER="$HDR_DIR/org_example_osgverselib_NativeOsgVerseCore.h"
if [ ! -f "$GEN_HEADER" ]; then
  echo "Expected header not generated: $GEN_HEADER" >&2
  exit 1
fi

# ---------- 3. 构建 C++ / CMake ----------
if [ "$SKIP_NATIVE" = "0" ]; then
  step "CMake native build"
  BUILD_DIR="$SCRIPT_DIR/build/linux-amd64"
  OUT_DIR="$SCRIPT_DIR/native/linux-amd64"
  mkdir -p "$OUT_DIR"
  cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCPPLIB_OUTPUT_DIR="$OUT_DIR" \
    -DCPPLIB_JAVA_HOME="$JAVA_HOME" \
    -DOSGVERSE_SDK_ROOT="$OSGVERSE_SDK"
  cmake --build "$BUILD_DIR" -- -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
  echo "artifacts:"
  ls -la "$OUT_DIR"
else
  echo "(native build skipped)"
fi

# ---------- 4. 组装 dist/osgverselib ----------
step "Assemble dist/$LIBNAME"
DIST="$SCRIPT_DIR/dist/$LIBNAME"
DIST_LIB="$DIST/library"
DIST_NATIVE="$DIST_LIB/native"
rm -rf "$DIST"
mkdir -p "$DIST_LIB" "$DIST_NATIVE"

cp "$SCRIPT_DIR/library.properties" "$DIST/library.properties"
cp "$JAR_FILE" "$DIST_LIB/$LIBNAME.jar"

if [ -d "$SCRIPT_DIR/native" ]; then
  for sub in "$SCRIPT_DIR"/native/*/; do
    [ -d "$sub" ] || continue
    name=$(basename "$sub")
    cp -r "$sub" "$DIST_NATIVE/$name"
  done
fi

# 只打包属于 osgverselib 的示例
OSGVERSE_EXAMPLES="CppOsgVerseViewer CppOsgVerseEarth"
if [ -d "$ROOT/examples" ]; then
  mkdir -p "$DIST/examples"
  for ex in $OSGVERSE_EXAMPLES; do
    if [ -d "$ROOT/examples/$ex" ]; then
      cp -r "$ROOT/examples/$ex" "$DIST/examples/$ex"
    fi
  done
fi

echo "dist/$LIBNAME contents:"
find "$DIST" -type f | sort | sed "s|$SCRIPT_DIR/||" | sed 's/^/  /'

# ---------- 5. 安装到 sketchbook ----------
if [ "$SKIP_INSTALL" = "0" ]; then
  SKETCHBOOK=""
  for cand in "$HOME/sketchbook" "$HOME/Documents/Processing"; do
    if [ -d "$cand" ]; then SKETCHBOOK="$cand"; break; fi
  done
  if [ -z "$SKETCHBOOK" ]; then
    echo "Cannot find Processing sketchbook. Use --skip-install." >&2
    exit 1
  fi
  DEST="$SKETCHBOOK/libraries/$LIBNAME"
  step "Install to sketchbook: $DEST"
  mkdir -p "$SKETCHBOOK/libraries"
  rm -rf "$DEST"
  cp -r "$DIST" "$DEST"
  echo "Installed. 请重启 Processing，然后 File > Examples > Libraries 查看示例。"
else
  echo "Not installed. 发布目录在: $DIST"
fi

echo ""
echo "All done."
