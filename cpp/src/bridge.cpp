// ============================================================
// JNI 桥接层：把 Java 侧 NativeCore 的 native 方法接到 C++ 实现。
// 头文件 native/include/org_example_cpplib_NativeCore.h 由 javac -h 生成，
// 修改 Java 方法签名后请重新生成（见 tools/build.ps1）。
// ============================================================

#include <jni.h>

#include <cstdio>

#include "config.h"
#include "engine.h"
#include "gl_view.h"
#include "mandelbrot.h"

#include <cstdint>
#include <string>

#if defined(CPPLIB_USE_CUDA)
#include "mandelbrot_cuda.h"
#endif

#include "org_example_cpplib_NativeCore.h"

namespace {

jstring toJString(JNIEnv* env, const std::string& text) {
    return env->NewStringUTF(text.c_str());
}

std::string backendInfo() {
#if defined(CPPLIB_USE_CUDA)
    if (cpplib::cuda::available()) {
        return "CUDA/" + cpplib::cuda::deviceName();
    }
    return "CUDA(unavailable)";
#else
    return "CPU";
#endif
}

}  // namespace

// ------------------------------------------------------------
// org.example.cpplib.NativeCore.nativeVersionString
// ------------------------------------------------------------
extern "C" JNIEXPORT jstring JNICALL
Java_org_example_cpplib_NativeCore_nativeVersionString(JNIEnv* env, jclass) {
    return toJString(env, std::string("cpplib ") + CPPLIB_VERSION +
                             " backend=" + backendInfo());
}

// ------------------------------------------------------------
// nativeCreateComputeEngine / nativeDisposeComputeEngine / nativeComputeInfo
// ------------------------------------------------------------
extern "C" JNIEXPORT jlong JNICALL
Java_org_example_cpplib_NativeCore_nativeCreateComputeEngine(JNIEnv*, jclass) {
    auto* engine = new cpplib::ComputeEngine();
    return reinterpret_cast<jlong>(engine);
}

extern "C" JNIEXPORT void JNICALL
Java_org_example_cpplib_NativeCore_nativeDisposeComputeEngine(JNIEnv*, jclass, jlong engine) {
    delete reinterpret_cast<cpplib::ComputeEngine*>(engine);
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_example_cpplib_NativeCore_nativeComputeInfo(JNIEnv* env, jclass, jlong engine) {
    auto* e = reinterpret_cast<cpplib::ComputeEngine*>(engine);
    if (e == nullptr) {
        return toJString(env, "cpplib: invalid engine handle");
    }
    return toJString(env, e->info());
}

// ------------------------------------------------------------
// nativeComputeMandelbrot
// ------------------------------------------------------------
extern "C" JNIEXPORT void JNICALL
Java_org_example_cpplib_NativeCore_nativeComputeMandelbrot(JNIEnv* env, jclass, jlong engine,
                                                           jintArray rgb, jint w, jint h,
                                                           jint maxIter,
                                                           jdouble x0, jdouble y0,
                                                           jdouble x1, jdouble y1) {
    auto* e = reinterpret_cast<cpplib::ComputeEngine*>(engine);
    if (e == nullptr) {
        return;
    }
    if (rgb == nullptr || w <= 0 || h <= 0) {
        return;
    }
    const jsize n = env->GetArrayLength(rgb);
    if (n < static_cast<jsize>(w) * h) {
        std::fprintf(stderr, "cpplib: output array too small (%d < %d)\n",
                     static_cast<int>(n), w * h);
        return;
    }

    jint* pixels = env->GetIntArrayElements(rgb, nullptr);
    if (pixels == nullptr) {
        return;  // OutOfMemoryError pending
    }
    e->mandelbrot(w, h, maxIter, x0, y0, x1, y1,
                  reinterpret_cast<std::uint32_t*>(pixels));
    env->ReleaseIntArrayElements(rgb, pixels, 0);  // 0 -> 复制回 Java
}

// ------------------------------------------------------------
// GL：nativeCreateGLView / nativeResizeGL / nativeRenderGL / nativeDisposeGL
// ------------------------------------------------------------
extern "C" JNIEXPORT jlong JNICALL
Java_org_example_cpplib_NativeCore_nativeCreateGLView(JNIEnv*, jclass) {
    return reinterpret_cast<jlong>(cpplib::createGLView());
}

extern "C" JNIEXPORT void JNICALL
Java_org_example_cpplib_NativeCore_nativeResizeGL(JNIEnv*, jclass, jlong view,
                                                  jint fbWidth, jint fbHeight) {
    auto* v = reinterpret_cast<cpplib::GLView*>(view);
    if (v != nullptr) {
        v->resize(fbWidth, fbHeight);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_org_example_cpplib_NativeCore_nativeRenderGL(JNIEnv*, jclass, jlong view,
                                                  jint fbWidth, jint fbHeight, jfloat time) {
    auto* v = reinterpret_cast<cpplib::GLView*>(view);
    if (v != nullptr) {
        v->render(fbWidth, fbHeight, static_cast<float>(time));
    }
}

extern "C" JNIEXPORT void JNICALL
Java_org_example_cpplib_NativeCore_nativeDisposeGL(JNIEnv*, jclass, jlong view) {
    auto* v = reinterpret_cast<cpplib::GLView*>(view);
    if (v != nullptr) {
        cpplib::deleteGLView(v);
    }
}
