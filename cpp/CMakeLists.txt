cmake_minimum_required(VERSION 3.20)

# ============================================================
# cpplib native library
# ------------------------------------------------------------
# 这个目录是“你的 C++/CUDA/AI 库”在本模板中的落脚点。
# 规则：
#   1. 它最终被编译成一个动态库（windows: cpplib.dll / linux: libcpplib.so / mac: libcpplib.dylib）
#   2. Java 通过 JNI 调用这里导出的函数（见 src/bridge.cpp）
#   3. 想要引入 CUDA： cmake -DCPPLIB_USE_CUDA=ON  （CMake 会自动找 CUDA Toolkit）
#   4. 想要引入其它 C++ 库（TensorRT / ONNX Runtime / cuDNN 等）：
#      在这里 find_package(...) 然后 target_link_libraries(cpplib PRIVATE ...)
# ============================================================

project(cpplib LANGUAGES C CXX)

# ------------------------------------------------------------
# 可配置项
# ------------------------------------------------------------
# 版本号，一般与 Java 侧 release.properties 的 prettyVersion 保持一致
set(CPPLIB_VERSION "1.0.0" CACHE STRING "cpplib version string")
# 是否启用 CUDA（本机装了 CUDA Toolkit 才能用）
option(CPPLIB_USE_CUDA "Compile the CUDA-accelerated backend (requires NVIDIA CUDA Toolkit)" OFF)

# Java 安装目录（提供 jni.h）。留空时自动探测：
#   PROCESSING_HOME / C:/Program Files/Processing 自带的 JDK -> $JAVA_HOME
set(CPPLIB_JAVA_HOME "" CACHE PATH "Path to a JDK (contains include/jni.h)")

# 最终 dll 输出目录。若为空则输出到 build 目录内。
# 建议由 build-native.bat/build.ps1 显式传入，让产物直接进入
# 项目根目录 native/<os>-<arch>/，Gradle/安装脚本会从这里取文件。
set(CPPLIB_OUTPUT_DIR "" CACHE PATH "Where to place the final shared library")

# ------------------------------------------------------------
# 自动探测 JDK（只要 jni.h / jni_md.h）
# ------------------------------------------------------------
if(NOT CPPLIB_JAVA_HOME)
  if(DEFINED ENV{JAVA_HOME} AND EXISTS "$ENV{JAVA_HOME}/include/jni.h")
    set(CPPLIB_JAVA_HOME "$ENV{JAVA_HOME}")
  elseif(EXISTS "C:/Program Files/Processing/app/resources/jdk/include/jni.h")
    # Processing 自带的 JDK 是最省事的来源（与运行库完全同版本）
    set(CPPLIB_JAVA_HOME "C:/Program Files/Processing/app/resources/jdk")
  elseif(EXISTS "$ENV{PROCESSING_HOME}/app/resources/jdk/include/jni.h")
    set(CPPLIB_JAVA_HOME "$ENV{PROCESSING_HOME}/app/resources/jdk")
  else()
    find_package(JNI QUIET)
    if(JNI_INCLUDE_DIRS)
      message(WARNING "CPPLIB_JAVA_HOME 未设置，使用 find_package(JNI) 找到的头文件")
    endif()
  endif()
endif()

# ------------------------------------------------------------
# 库本体
# ------------------------------------------------------------
add_library(cpplib SHARED
  src/bridge.cpp
  src/engine.cpp
  src/mandelbrot.cpp
  src/gl_loader.cpp
  src/gl_view.cpp
)

# C++17，并对 MSVC 开启 UTF-8 保证中文注释/字符串不出乱码
set_target_properties(cpplib PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
if(MSVC)
  target_compile_options(cpplib PRIVATE /utf-8)
endif()

target_include_directories(cpplib PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CMAKE_CURRENT_SOURCE_DIR}/../native/include"   # javac -h 生成的 JNI 头
)
if(CPPLIB_JAVA_HOME)
  target_include_directories(cpplib PRIVATE "${CPPLIB_JAVA_HOME}/include")
  if(EXISTS "${CPPLIB_JAVA_HOME}/include/win32")
    target_include_directories(cpplib PRIVATE "${CPPLIB_JAVA_HOME}/include/win32")
  endif()
  if(EXISTS "${CPPLIB_JAVA_HOME}/include/darwin")
    target_include_directories(cpplib PRIVATE "${CPPLIB_JAVA_HOME}/include/darwin")
  endif()
  if(EXISTS "${CPPLIB_JAVA_HOME}/include/linux")
    target_include_directories(cpplib PRIVATE "${CPPLIB_JAVA_HOME}/include/linux")
  endif()
else()
  target_include_directories(cpplib PRIVATE ${JNI_INCLUDE_DIRS})
endif()

target_compile_definitions(cpplib PRIVATE CPPLIB_VERSION="${CPPLIB_VERSION}")

# Windows 上需要链接 opengl32（gl_loader.cpp 通过 GetProcAddress 动态取函数指针，
# 但链接器仍需要能解析 LoadLibrary 之外的入口……实际上动态加载不需要，加它无害）
if(WIN32)
  target_link_libraries(cpplib PRIVATE opengl32)
endif()
# Linux/mac 需要 dl
if(UNIX)
  target_link_libraries(cpplib PRIVATE ${CMAKE_DL_LIBS})
endif()

# 纯 CPU 构建使用静态 CRT（/MT），这样发布给用户的 cpplib.dll 不依赖 VC 运行库
if(NOT CPPLIB_USE_CUDA AND MSVC)
  set_property(TARGET cpplib PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# ------------------------------------------------------------
# 可选 CUDA 后端
#   编译后会用 GPU 计算 Mandelbrot。生成物仍叫 cpplib.dll，
#   但依赖 cudart64_*.dll —— 安装脚本会尝试把它拷到 native 目录一起发布。
# ------------------------------------------------------------
if(CPPLIB_USE_CUDA)
  enable_language(CUDA)
  find_package(CUDAToolkit REQUIRED)
  target_sources(cpplib PRIVATE src/mandelbrot_cuda.cu)
  target_link_libraries(cpplib PRIVATE CUDA::cudart)
  target_compile_definitions(cpplib PRIVATE CPPLIB_USE_CUDA=1)
  set_target_properties(cpplib PROPERTIES
    CUDA_STANDARD 17
    CUDA_STANDARD_REQUIRED ON)
endif()

# ------------------------------------------------------------
# 输出位置：支持单/多配置生成器，把最终二进制直接放到 CPPLIB_OUTPUT_DIR
# ------------------------------------------------------------
if(CPPLIB_OUTPUT_DIR)
  # 基础输出目录：单配置生成器（Unix Makefiles / Ninja 等，Linux 默认）只看这个
  set_target_properties(cpplib PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CPPLIB_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${CPPLIB_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${CPPLIB_OUTPUT_DIR}"
  )
  # 每配置输出目录：多配置生成器（Visual Studio / Xcode）看这些
  foreach(_cfg Release Debug RelWithDebInfo MinSizeRel)
    set_target_properties(cpplib PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY_${_cfg} "${CPPLIB_OUTPUT_DIR}"
      LIBRARY_OUTPUT_DIRECTORY_${_cfg} "${CPPLIB_OUTPUT_DIR}"
      ARCHIVE_OUTPUT_DIRECTORY_${_cfg} "${CPPLIB_OUTPUT_DIR}"
    )
  endforeach()
endif()
