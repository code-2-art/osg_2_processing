// ============================================================
//  JNI 桥接层：把 Java 侧 NativeOsgCore 的 native 方法接到 OsgViewer。
//  头文件 native/include/org_example_osglib_NativeOsgCore.h 由 javac -h 生成。
// ============================================================

#include <jni.h>
#include <cstdio>
#include <string>

#include "osg_viewer.h"

#include "org_example_osglib_NativeOsgCore.h"

namespace {

jstring toJString(JNIEnv* env, const std::string& text) {
    return env->NewStringUTF(text.c_str());
}

}  // namespace

extern "C" JNIEXPORT jlong JNICALL
Java_org_example_osglib_NativeOsgCore_nativeCreateViewer(JNIEnv*, jclass,
                                                         jint width, jint height) {
    auto* viewer = new osglib::OsgViewer(width, height);
    return reinterpret_cast<jlong>(viewer);
}

extern "C" JNIEXPORT void JNICALL
Java_org_example_osglib_NativeOsgCore_nativeDisposeViewer(JNIEnv*, jclass, jlong handle) {
    delete reinterpret_cast<osglib::OsgViewer*>(handle);
}

extern "C" JNIEXPORT void JNICALL
Java_org_example_osglib_NativeOsgCore_nativeResize(JNIEnv*, jclass, jlong handle,
                                                   jint width, jint height) {
    auto* v = reinterpret_cast<osglib::OsgViewer*>(handle);
    if (v) v->resize(width, height);
}

extern "C" JNIEXPORT void JNICALL
Java_org_example_osglib_NativeOsgCore_nativeFrame(JNIEnv*, jclass, jlong handle) {
    auto* v = reinterpret_cast<osglib::OsgViewer*>(handle);
    if (v) v->frame();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_example_osglib_NativeOsgCore_nativeLoadModel(JNIEnv* env, jclass, jlong handle,
                                                      jstring path) {
    auto* v = reinterpret_cast<osglib::OsgViewer*>(handle);
    if (!v || !path) return JNI_FALSE;
    const char* chars = env->GetStringUTFChars(path, nullptr);
    bool ok = false;
    if (chars) {
        ok = v->loadModel(std::string(chars));
        env->ReleaseStringUTFChars(path, chars);
    }
    return ok ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_org_example_osglib_NativeOsgCore_nativeHome(JNIEnv*, jclass, jlong handle) {
    auto* v = reinterpret_cast<osglib::OsgViewer*>(handle);
    if (v) v->home();
}

extern "C" JNIEXPORT void JNICALL
Java_org_example_osglib_NativeOsgCore_nativeSetViewpoint(JNIEnv*, jclass, jlong handle,
                                                         jdouble x, jdouble y, jdouble z,
                                                         jdouble h, jdouble p, jdouble r) {
    auto* v = reinterpret_cast<osglib::OsgViewer*>(handle);
    if (v) v->setViewpoint(x, y, z, h, p, r);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_example_osglib_NativeOsgCore_nativeSaveScreenshot(JNIEnv* env, jclass, jlong handle,
                                                           jstring path) {
    auto* v = reinterpret_cast<osglib::OsgViewer*>(handle);
    if (!v || !path) return JNI_FALSE;
    const char* chars = env->GetStringUTFChars(path, nullptr);
    bool ok = false;
    if (chars) {
        ok = v->saveScreenshot(std::string(chars));
        env->ReleaseStringUTFChars(path, chars);
    }
    return ok ? JNI_TRUE : JNI_FALSE;
}

// ---- 输入事件转发 ----

extern "C" JNIEXPORT void JNICALL
Java_org_example_osglib_NativeOsgCore_nativeMouseMove(JNIEnv*, jclass, jlong handle,
                                                      jint x, jint y) {
    auto* v = reinterpret_cast<osglib::OsgViewer*>(handle);
    if (v) v->mouseMove(x, y);
}

extern "C" JNIEXPORT void JNICALL
Java_org_example_osglib_NativeOsgCore_nativeMousePress(JNIEnv*, jclass, jlong handle,
                                                       jint x, jint y, jint button) {
    auto* v = reinterpret_cast<osglib::OsgViewer*>(handle);
    if (v) v->mouseButtonPress(x, y, button);
}

extern "C" JNIEXPORT void JNICALL
Java_org_example_osglib_NativeOsgCore_nativeMouseRelease(JNIEnv*, jclass, jlong handle,
                                                         jint x, jint y, jint button) {
    auto* v = reinterpret_cast<osglib::OsgViewer*>(handle);
    if (v) v->mouseButtonRelease(x, y, button);
}

extern "C" JNIEXPORT void JNICALL
Java_org_example_osglib_NativeOsgCore_nativeMouseScroll(JNIEnv*, jclass, jlong handle,
                                                        jint updown, jint x, jint y) {
    auto* v = reinterpret_cast<osglib::OsgViewer*>(handle);
    if (v) v->mouseScroll(updown, x, y);
}

extern "C" JNIEXPORT void JNICALL
Java_org_example_osglib_NativeOsgCore_nativeKeyPress(JNIEnv*, jclass, jlong handle, jint key) {
    auto* v = reinterpret_cast<osglib::OsgViewer*>(handle);
    if (v) v->keyPress(key);
}

extern "C" JNIEXPORT void JNICALL
Java_org_example_osglib_NativeOsgCore_nativeKeyRelease(JNIEnv*, jclass, jlong handle, jint key) {
    auto* v = reinterpret_cast<osglib::OsgViewer*>(handle);
    if (v) v->keyRelease(key);
}
