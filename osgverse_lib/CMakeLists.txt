cmake_minimum_required(VERSION 3.20)

# ============================================================
#  osgverse_lib：把 osgVerse 嵌进 Processing 的原生库
#    - PBR + 延迟渲染管线（osgVerse::Pipeline）
#    - 数字地球（TMS 瓦片 + EarthManipulator）
#  产物：libosgverselib.so (Linux)
#
#  需要 osgVerse SDK（GLCORE 构建）：包含
#    include/ lib/ lib/cmake/osgVerse/ bin/ shaders/ skyboxes/ models/
#  默认探测 ~/Public/osgverse/build/sdk_core，可用 -DOSGVERSE_SDK_ROOT= 指定。
# ============================================================

project(osgverselib LANGUAGES C CXX)

set(CPPLIB_VERSION "1.0.0" CACHE STRING "osgverselib version string")

# osgVerse SDK 根目录
set(OSGVERSE_SDK_ROOT "" CACHE PATH "Path to osgVerse SDK (contains include/, lib/, shaders/)")

# Java 安装目录（提供 jni.h）
set(CPPLIB_JAVA_HOME "" CACHE PATH "Path to a JDK (contains include/jni.h)")

# GL 头文件目录（GL/glcorearb.h、GL/glext.h 等）。osgVerse 的 GLCORE SDK 自身不带
# include/GL，构建时需要额外的 GL 头文件搜索路径（通常位于 OSG 源码树的
# helpers/toolchain_builder/opengl 或 3rdparty 目录下）。
set(OSG_GL_INCLUDE_DIR "" CACHE PATH "Path to GL headers (contains GL/glcorearb.h)")

# 最终 .so 输出目录
set(CPPLIB_OUTPUT_DIR "" CACHE PATH "Where to place the final shared library")

# ------------------------------------------------------------
# 探测 osgVerse SDK
# ------------------------------------------------------------
if(NOT OSGVERSE_SDK_ROOT)
  if(DEFINED ENV{OSGVERSE_SDK_ROOT})
    set(OSGVERSE_SDK_ROOT "$ENV{OSGVERSE_SDK_ROOT}")
  elseif(EXISTS "$ENV{HOME}/Public/osgverse/build/sdk_core")
    set(OSGVERSE_SDK_ROOT "$ENV{HOME}/Public/osgverse/build/sdk_core")
  endif()
endif()

if(NOT OSGVERSE_SDK_ROOT OR NOT EXISTS "${OSGVERSE_SDK_ROOT}/include/osgVerse/pipeline/Pipeline.h")
  message(FATAL_ERROR
    "osgVerse SDK not found. Set -DOSGVERSE_SDK_ROOT=<path> "
    "(must contain include/osgVerse/pipeline/Pipeline.h).\n"
    "  Default probe: $ENV{HOME}/Public/osgverse/build/sdk_core")
endif()

message(STATUS "Using osgVerse SDK: ${OSGVERSE_SDK_ROOT}")

# ------------------------------------------------------------
# 用 SDK 自带的 CMake 脚本导入 osgVerse（同时也会导入它依赖的 OSG）
# ------------------------------------------------------------
list(APPEND CMAKE_PREFIX_PATH "${OSGVERSE_SDK_ROOT}/lib/cmake")
find_package(osgVerse REQUIRED)
message(STATUS "osgVerse_API=${osgVerse_API} platform=${osgVerse_PLATFORM} version=${osgVerse_VERSION}")

if(NOT osgVerse_API STREQUAL "GLCORE")
  message(WARNING "This library is written for the GLCORE build of osgVerse, but the SDK reports"
                  " osgVerse_API=${osgVerse_API}. Vertex arrays / fixed-function handling may differ.")
endif()

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
add_library(osgverselib SHARED
  src/bridge.cpp
  src/osgverse_viewer.cpp
)

# Windows 上 GL 3.x 核心函数无法通过 opengl32.lib 静态链接，需经 wglGetProcAddress 解析
if(WIN32)
  target_sources(osgverselib PRIVATE src/gl_windows_loader.cpp)
endif()

set_target_properties(osgverselib PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)

target_include_directories(osgverselib PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CMAKE_CURRENT_SOURCE_DIR}/native/include"   # javac -h 生成的 JNI 头
  "${OSGVERSE_SDK_ROOT}/include"
  "${OSGVERSE_SDK_ROOT}/include/osgVerse"        # 让 <pipeline/...> 这种写法也能用
)

# GL 头文件（GLCORE 构建需要 GL/glcorearb.h 等，SDK 自身不带）
if(OSG_GL_INCLUDE_DIR)
  target_include_directories(osgverselib PRIVATE "${OSG_GL_INCLUDE_DIR}")
endif()

if(CPPLIB_JAVA_HOME)
  target_include_directories(osgverselib PRIVATE
    "${CPPLIB_JAVA_HOME}/include"
    "${CPPLIB_JAVA_HOME}/include/linux"
    "${CPPLIB_JAVA_HOME}/include/darwin"
    "${CPPLIB_JAVA_HOME}/include/win32"
  )
elseif(JNI_INCLUDE_DIRS)
  target_include_directories(osgverselib PRIVATE ${JNI_INCLUDE_DIRS})
endif()

# SDK 根目录：运行时 osgVerse 用它定位 shaders/ skyboxes/ models/（BASE_DIR）。
file(TO_CMAKE_PATH "${OSGVERSE_SDK_ROOT}" OSGVERSE_SDK_DIR)
target_compile_definitions(osgverselib PRIVATE
  OSGVERSE_SDK_ROOT="${OSGVERSE_SDK_DIR}"
  CPPLIB_VERSION="${CPPLIB_VERSION}"
)

# osgVerse / OSG 的插件目录（osgPlugins-<版本>/）：
#   - Linux：<sdk>/lib/osgPlugins-<版本>/
#   - Windows：<sdk>/bin/osgPlugins-<版本>/
# osgverse_viewer.cpp 会把它加入 osgDB 的搜索路径。
if(WIN32)
  file(TO_CMAKE_PATH "${OSGVERSE_SDK_ROOT}/bin" OSGVERSE_PLUGIN_DIR)
else()
  file(TO_CMAKE_PATH "${OSGVERSE_SDK_ROOT}/lib" OSGVERSE_PLUGIN_DIR)
endif()
target_compile_definitions(osgverselib PRIVATE OSGVERSE_PLUGIN_DIR="${OSGVERSE_PLUGIN_DIR}")

# osgVerse + OSG 的库、头文件路径、依赖全部由 SDK 的 osgVerseConfig.cmake 提供
target_link_libraries(osgverselib PRIVATE osgVerse::Verse osgVerse::OSG)

# Windows 上需要 opengl32.lib 提供 wglGetProcAddress（gl_windows_loader.cpp 使用）
if(WIN32)
  target_link_libraries(osgverselib PRIVATE opengl32)
endif()

# 把 SDK 的 lib 目录写进 rpath，运行时能找到 libosgVerse*.so / libosg.so 等
set_target_properties(osgverselib PROPERTIES
  BUILD_WITH_INSTALL_RPATH TRUE
  INSTALL_RPATH "${OSGVERSE_SDK_ROOT}/lib"
)

if(UNIX)
  target_link_libraries(osgverselib PRIVATE ${CMAKE_DL_LIBS})
endif()

# ------------------------------------------------------------
# 输出位置
# ------------------------------------------------------------
if(CPPLIB_OUTPUT_DIR)
  set_target_properties(osgverselib PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CPPLIB_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${CPPLIB_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${CPPLIB_OUTPUT_DIR}"
  )
  foreach(_cfg Release Debug RelWithDebInfo MinSizeRel)
    set_target_properties(osgverselib PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY_${_cfg} "${CPPLIB_OUTPUT_DIR}"
      LIBRARY_OUTPUT_DIRECTORY_${_cfg} "${CPPLIB_OUTPUT_DIR}"
      ARCHIVE_OUTPUT_DIRECTORY_${_cfg} "${CPPLIB_OUTPUT_DIR}"
    )
  endforeach()
endif()
