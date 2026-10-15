#!/usr/bin/env bash
# ============================================================
#  cpplib 一键构建 + 冒烟测试 + 安装到 Processing sketchbook（Linux / macOS）
#
#  用法：
#     bash tools/build.sh
#
#  可选开关：
#     --cuda            启用 CUDA 后端（需要本机 NVIDIA CUDA Toolkit）
#     --skip-native     跳过 C++/CMake 构建（仅打包 Java + 组装）
#     --skip-test       跳过 JNI 命令行冒烟测试
#     --skip-install    不安装到 Processing sketchbook
#     --java-home <p>   指定 JDK（默认使用 Processing 自带 JDK）
#
#  产物：
#     build/cpplib.jar                      Java 侧 jar
#     native/linux-amd64/libcpplib.so       原生产物（由 CMake 输出）
#     dist/cpplib/                          完整的可发布库目录
#     <sketchbook>/libraries/cpplib         安装结果（Processing 自动识别）
# ============================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
LIBNAME="cpplib"

CUDA=OFF
SKIP_NATIVE=0
SKIP_TEST=0
SKIP_INSTALL=0
JAVA_HOME_OVERRIDE=""

while [ $# -gt 0 ]; do
  case "$1" in
    --cuda) CUDA=ON ;;
    --skip-native) SKIP_NATIVE=1 ;;
    --skip-test) SKIP_TEST=1 ;;
    --skip-install) SKIP_INSTALL=1 ;;
    --java-home) JAVA_HOME_OVERRIDE="$2"; shift ;;
    *) echo "unknown option: $1"; exit 1 ;;
  esac
  shift
done

step() {
  echo ""
  echo "=== $1 ==="
}

# ---------- 1. 定位 Processing / JDK / core ----------
# 尝试常见的 Processing 安装位置
PROC_HOME="${PROCESSING_HOME:-}"
if [ -z "$PROC_HOME" ]; then
  for cand in \
    "$HOME/Public/Misc/Processing/lib" \
    "$HOME/Processing/lib" \
    "/opt/processing/lib" \
    "/usr/lib/processing/lib" \
    "$HOME/Downloads/processing/lib" ; do
    if [ -d "$cand" ]; then PROC_HOME="$cand"; break; fi
  done
fi
# 也允许用户把 PROCESSING_HOME 指到 Processing 根目录（含 lib/ 的上一级）
if [ -z "$PROC_HOME" ] && [ -n "${PROCESSING_HOME:-}" ]; then
  if [ -d "$PROCESSING_HOME/lib" ]; then PROC_HOME="$PROCESSING_HOME/lib"; fi
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
  echo "JDK not found. Pass --java-home <path> or install Processing with bundled JDK." >&2
  exit 1
fi
JAVAC="$JAVA_HOME/bin/javac"
JAR="$JAVA_HOME/bin/jar"
JAVA="$JAVA_HOME/bin/java"
step "Using JDK: $JAVA_HOME"

CORE_DIR="$PROC_HOME/app/resources/core/library"
CORE_JAR=$(ls "$CORE_DIR"/core-*.jar 2>/dev/null | sort | tail -n1 || true)
if [ -z "$CORE_JAR" ]; then
  echo "Processing core jar not found under $CORE_DIR" >&2
  exit 1
fi
step "Processing core: $CORE_JAR"

# ---------- 2. 编译 Java 并生成 JNI 头 ----------
SRC_ROOT="$ROOT/src/main/java"
CLASSES_DIR="$ROOT/build/classes"
HDR_DIR="$ROOT/native/include"
JAR_FILE="$ROOT/build/$LIBNAME.jar"
mkdir -p "$CLASSES_DIR" "$HDR_DIR"

step "javac (regenerates native/include/org_example_cpplib_NativeCore.h)"
SOURCES=$(find "$SRC_ROOT" -name '*.java')
"$JAVAC" -encoding UTF-8 -h "$HDR_DIR" -d "$CLASSES_DIR" -classpath "$CORE_JAR" $SOURCES
"$JAR" cf "$JAR_FILE" -C "$CLASSES_DIR" .
echo "jar -> $JAR_FILE"

GEN_HEADER="$HDR_DIR/org_example_cpplib_NativeCore.h"
if [ ! -f "$GEN_HEADER" ]; then
  echo "Expected header not generated: $GEN_HEADER" >&2
  exit 1
fi

# ---------- 3. 构建 C++ / CMake ----------
if [ "$SKIP_NATIVE" = "0" ]; then
  step "CMake native build (cuda=$CUDA)"
  NATIVE_SH="$ROOT/cpp/build-native.sh"
  # build-native.sh 的第一个参数是平台名（如 linux-amd64），留空则自动探测
  NATIVE_ARGS=("")
  [ -n "$JAVA_HOME" ] && NATIVE_ARGS+=(-java "$JAVA_HOME")
  [ "$CUDA" = "ON" ] && NATIVE_ARGS+=(-cuda)
  bash "$NATIVE_SH" "${NATIVE_ARGS[@]}"
else
  echo "(native build skipped)"
fi

# ---------- 4. 组装 dist/cpplib ----------
step "Assemble dist/$LIBNAME"
DIST="$ROOT/dist/$LIBNAME"
DIST_LIB="$DIST/library"
DIST_NATIVE="$DIST_LIB/native"
DIST_EXAMPLES="$DIST/examples"
rm -rf "$DIST"
mkdir -p "$DIST_LIB" "$DIST_NATIVE" "$DIST_EXAMPLES"

cp "$ROOT/library.properties" "$DIST/library.properties"
cp "$JAR_FILE" "$DIST_LIB/$LIBNAME.jar"

# 原生产物（native/<os>-<arch>/ 子目录）
if [ -d "$ROOT/native" ]; then
  for sub in "$ROOT"/native/*/; do
    [ -d "$sub" ] || continue
    name=$(basename "$sub")
    cp -r "$sub" "$DIST_NATIVE/$name"
  done
fi

# examples
if [ -d "$ROOT/examples" ]; then
  cp -r "$ROOT/examples/." "$DIST_EXAMPLES/"
fi

echo "dist/$LIBNAME contents:"
find "$DIST" -type f | sort | sed "s|$ROOT/||" | sed 's/^/  /'

# ---------- 5. JNI 冒烟测试 ----------
if [ "$SKIP_TEST" = "0" ]; then
  step "JNI smoke test"
  TEST_SRC="$ROOT/tools/jnitest"
  TEST_CLASSES="$ROOT/build/testclasses"
  rm -rf "$TEST_CLASSES"
  mkdir -p "$TEST_CLASSES"
  TEST_SOURCES=$(find "$TEST_SRC" -name '*.java')
  "$JAVAC" -encoding UTF-8 -d "$TEST_CLASSES" -classpath "$DIST_LIB/$LIBNAME.jar" $TEST_SOURCES
  "$JAVA" -cp "$DIST_LIB/$LIBNAME.jar:$TEST_CLASSES" org.example.cpplib.TestMain
  echo "JNI smoke test PASSED"
fi

# ---------- 6. 安装到 sketchbook ----------
if [ "$SKIP_INSTALL" = "0" ]; then
  # Linux/macOS sketchbook 常见位置
  SKETCHBOOK=""
  for cand in "$HOME/sketchbook" "$HOME/Documents/Processing" "$HOME/Documents/Processing/sketchbook"; do
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
  echo "Installed. 请重启/重开 Processing，然后 File > Examples > Libraries 即可看到两个示例。"
else
  echo ""
  echo "Not installed. 发布目录在: $DIST"
fi

echo ""
echo "All done."
