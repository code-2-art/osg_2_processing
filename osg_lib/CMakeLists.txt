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

# GL 头文件目录（GL/glcorearb.h、GL/glext.h 等）。
# OSG 的 GLCORE SDK 自身不带 include/GL，构建时需要额外的 GL 头文件搜索路径。
# 在 OSG 源码树里通常是 <osg-source>/helpers/toolchain_builder/opengl 或 <osg-source>/3rdparty。
set(OSG_GL_INCLUDE_DIR "" CACHE PATH "Path to GL headers (contains GL/glcorearb.h)")

# 最终 .so / .dll 输出目录
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
    "OpenSceneGraph SDK not found. Set -DOSG_SDK_ROOT=<path> (must contain include/osgViewer/Viewer and lib/libosgViewer.so / osgViewer.lib).\n"
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

# Windows 上 GL 3.x 核心函数无法通过 opengl32.lib 静态链接，需经 wglGetProcAddress 解析
if(WIN32)
  target_sources(osglib PRIVATE src/gl_windows_loader.cpp)
endif()

set_target_properties(osglib PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
if(MSVC)
  target_compile_options(osglib PRIVATE /utf-8)
endif()

target_include_directories(osglib PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CMAKE_CURRENT_SOURCE_DIR}/native/include"   # javac -h 生成的 JNI 头
  "${OSG_SDK_ROOT}/include"
)

# GL 头文件（GLCORE 构建的 OSG 需要 GL/glcorearb.h 等，SDK 自身不带）
if(OSG_GL_INCLUDE_DIR)
  target_include_directories(osglib PRIVATE "${OSG_GL_INCLUDE_DIR}")
endif()

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

# OSG SDK 的插件目录：运行时 osgDB 用它定位插件（osgPlugins-<版本>/osgdb_<ext>.so/.dll）。
#   - Linux：插件在 <sdk>/lib/osgPlugins-<版本>/
#   - Windows：插件在 <sdk>/bin/osgPlugins-<版本>/
# 字符串里的反斜杠在 Windows 上需要转义，这里统一用正斜杠（Windows 也接受）。
if(WIN32)
  file(TO_CMAKE_PATH "${OSG_SDK_ROOT}/bin" OSGLIB_OSG_LIB_DIR)
else()
  file(TO_CMAKE_PATH "${OSG_SDK_ROOT}/lib" OSGLIB_OSG_LIB_DIR)
endif()
target_compile_definitions(osglib PRIVATE OSGLIB_OSG_LIB_DIR="${OSGLIB_OSG_LIB_DIR}")

# ------------------------------------------------------------
# 链接 OSG 库
#   - Linux：直接按 SDK 内的绝对路径链接 .so
#   - Windows：链接 .lib 导入库（同目录下，与 bin 里的 .dll 同名去掉扩展名的 .lib）
# ------------------------------------------------------------
if(WIN32)
  target_link_libraries(osglib PRIVATE
    "${OSG_SDK_ROOT}/lib/osgViewer.lib"
    "${OSG_SDK_ROOT}/lib/osgGA.lib"
    "${OSG_SDK_ROOT}/lib/osgDB.lib"
    "${OSG_SDK_ROOT}/lib/osgUtil.lib"
    "${OSG_SDK_ROOT}/lib/osgText.lib"
    "${OSG_SDK_ROOT}/lib/osg.lib"
    "${OSG_SDK_ROOT}/lib/OpenThreads.lib"
  )
  # Windows 不需要 rpath；运行时的 .dll 由 build 脚本拷贝到 native 目录或经 PATH 提供。
  # 链接 opengl32.lib 以解析 OpenGL 1.1 函数（GL 3.x 由 gl_windows_loader.cpp 经
  # wglGetProcAddress 提供）。
  target_link_libraries(osglib PRIVATE opengl32)
else()
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
endif()

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
