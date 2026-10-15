cmake_minimum_required(VERSION 3.20)

# ============================================================
#  osg_lib：把 OpenSceneGraph 嵌入 Processing 的原生库
#  产物：libosglib.so (Linux) / osglib.dll (Windows)
#  需要 OSG SDK（GLCORE 构建）。默认探测 ~/Public/osgverse/build/sdk_core。
#  可用 -DOSG_SDK_ROOT=... 指定。
# ============================================================

project(osglib LANGUAGES C CXX)

set(CPPLIB_VERSION "1.0.0" CACHE STRING "osglib version string")

# OSG SDK 根目录（包含 include/ 和 lib/）
set(OSG_SDK_ROOT "" CACHE PATH "Path to OpenSceneGraph SDK (contains include/ and lib/)")

# Java 安装目录（提供 jni.h）
set(CPPLIB_JAVA_HOME "" CACHE PATH "Path to a JDK (contains include/jni.h)")

# 最终 .so 输出目录
set(CPPLIB_OUTPUT_DIR "" CACHE PATH "Where to place the final shared library")

# ------------------------------------------------------------
# 探测 OSG SDK
# ------------------------------------------------------------
if(NOT OSG_SDK_ROOT)
  if(DEFINED ENV{OSG_SDK_ROOT})
    set(OSG_SDK_ROOT "$ENV{OSG_SDK_ROOT}")
  elseif(EXISTS "$ENV{HOME}/Public/osgverse/build/sdk_core")
    set(OSG_SDK_ROOT "$ENV{HOME}/Public/osgverse/build/sdk_core")
  endif()
endif()

if(NOT OSG_SDK_ROOT OR NOT EXISTS "${OSG_SDK_ROOT}/include/osgViewer/Viewer")
  message(FATAL_ERROR
    "OpenSceneGraph SDK not found. Set -DOSG_SDK_ROOT=<path> (must contain include/osgViewer/Viewer and lib/libosgViewer.so).\n"
    "  Default probe: $ENV{HOME}/Public/osgverse/build/sdk_core")
endif()

message(STATUS "Using OSG SDK: ${OSG_SDK_ROOT}")

# ------------------------------------------------------------
# 探测 JDK（jni.h）
# ------------------------------------------------------------
if(NOT CPPLIB_JAVA_HOME)
  if(DEFINED ENV{JAVA_HOME} AND EXISTS "$ENV{JAVA_HOME}/include/jni.h")
    set(CPPLIB_JAVA_HOME "$ENV{JAVA_HOME}")
  elseif(EXISTS "$ENV{HOME}/Public/Misc/Processing/lib/app/resources/jdk/include/jni.h")
    set(CPPLIB_JAVA_HOME "$ENV{HOME}/Public/Misc/Processing/lib/app/resources/jdk")
  elseif(EXISTS "$ENV{PROCESSING_HOME}/app/resources/jdk/include/jni.h")
    set(CPPLIB_JAVA_HOME "$ENV{PROCESSING_HOME}/app/resources/jdk")
  else()
    find_package(JNI QUIET)
  endif()
endif()

# ------------------------------------------------------------
# 库本体
# ------------------------------------------------------------
add_library(osglib SHARED
  src/bridge.cpp
  src/osg_viewer.cpp
)

set_target_properties(osglib PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

target_include_directories(osglib PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CMAKE_CURRENT_SOURCE_DIR}/native/include"   # javac -h 生成的 JNI 头
  "${OSG_SDK_ROOT}/include"
)

if(CPPLIB_JAVA_HOME)
  target_include_directories(osglib PRIVATE
    "${CPPLIB_JAVA_HOME}/include"
    "${CPPLIB_JAVA_HOME}/include/linux"
    "${CPPLIB_JAVA_HOME}/include/darwin"
    "${CPPLIB_JAVA_HOME}/include/win32"
  )
elseif(JNI_INCLUDE_DIRS)
  target_include_directories(osglib PRIVATE ${JNI_INCLUDE_DIRS})
endif()

target_compile_definitions(osglib PRIVATE CPPLIB_VERSION="${CPPLIB_VERSION}")

# OSG SDK 的 lib 目录：运行时用它来定位插件（osgPlugins-<版本>/osgdb_<ext>.so）。
# 字符串里的反斜杠在 Windows 上需要转义，这里统一用正斜杠（Windows 也接受）。
file(TO_CMAKE_PATH "${OSG_SDK_ROOT}/lib" OSGLIB_OSG_LIB_DIR)
target_compile_definitions(osglib PRIVATE OSGLIB_OSG_LIB_DIR="${OSGLIB_OSG_LIB_DIR}")

# OSG 库（直接按 SDK 内的绝对路径链接，避免依赖系统 find_package）
target_link_libraries(osglib PRIVATE
  "${OSG_SDK_ROOT}/lib/libosgViewer.so"
  "${OSG_SDK_ROOT}/lib/libosgGA.so"
  "${OSG_SDK_ROOT}/lib/libosgDB.so"
  "${OSG_SDK_ROOT}/lib/libosgUtil.so"
  "${OSG_SDK_ROOT}/lib/libosgText.so"
  "${OSG_SDK_ROOT}/lib/libosg.so"
  "${OSG_SDK_ROOT}/lib/libOpenThreads.so"
)

# 把 OSG lib 目录写进 rpath，运行时能找到 osg 依赖
set_target_properties(osglib PROPERTIES
  BUILD_WITH_INSTALL_RPATH TRUE
  INSTALL_RPATH "${OSG_SDK_ROOT}/lib"
)

# Unix 需要 dl
if(UNIX)
  target_link_libraries(osglib PRIVATE ${CMAKE_DL_LIBS})
endif()

# ------------------------------------------------------------
# 输出位置
# ------------------------------------------------------------
if(CPPLIB_OUTPUT_DIR)
  set_target_properties(osglib PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CPPLIB_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${CPPLIB_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${CPPLIB_OUTPUT_DIR}"
  )
  foreach(_cfg Release Debug RelWithDebInfo MinSizeRel)
    set_target_properties(osglib PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY_${_cfg} "${CPPLIB_OUTPUT_DIR}"
      LIBRARY_OUTPUT_DIRECTORY_${_cfg} "${CPPLIB_OUTPUT_DIR}"
      ARCHIVE_OUTPUT_DIRECTORY_${_cfg} "${CPPLIB_OUTPUT_DIR}"
    )
  endforeach()
endif()
