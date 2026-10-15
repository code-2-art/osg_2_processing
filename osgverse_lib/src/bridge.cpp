// ============================================================
//  JNI 桥接层：把 Java 侧 NativeOsgVerseCore 的 native 方法接到 OsgVerseViewer。
//  头文件 native/include/org_example_osgverselib_NativeOsgVerseCore.h 由 javac -h 生成。
// ============================================================

#include <jni.h>

#include <string>

#include "osgverse_viewer.h"

#include "org_example_osgverselib_NativeOsgVerseCore.h"

namespace {

std::string toStdString(JNIEnv* env, jstring text) {
    if (!text) return std::string();
    const char* chars = env->GetStringUTFChars(text, nullptr);
    std::string result;
    if (chars) {
        result = chars;
        env->ReleaseStringUTFChars(text, chars);
    }
    return result;
}

}  // namespace

extern "C" JNIEXPORT jlong JNICALL
Java_org_example_osgverselib_NativeOsgVerseCore_nativeCreateViewer(JNIEnv*, jclass,
                                                                   jint width, jint height) {
    auto* viewer = new osgverselib::OsgVerseViewer(width, height);
    return reinterpret_cast<jlong>(viewer);
}

extern "C" JNIEXPORT void JNICALL
Java_org_example_osgverselib_NativeOsgVerseCore_nativeDisposeViewer(JNIEnv*, jclass,
                                                                    jlong handle) {
    delete reinterpret_cast<osgverselib::OsgVerseViewer*>(handle);
}

extern "C" JNIEXPORT void JNICALL
Java_org_example_osgverselib_NativeOsgVerseCore_nativeResize(JNIEnv*, jclass, jlong handle,
                                                             jint width, jint height) {
    auto* v = reinterpret_cast<osgverselib::OsgVerseViewer*>(handle);
    if (v) v->resize(width, height);
}

extern "C" JNIEXPORT void JNICALL
Java_org_example_osgverselib_NativeOsgVerseCore_nativeFrame(JNIEnv*, jclass, jlong handle) {
    auto* v = reinterpret_cast<osgverselib::OsgVerseViewer*>(handle);
    if (v) v->frame();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_example_osgverselib_NativeOsgVerseCore_nativeCreatePbrScene(JNIEnv* env, jclass,
                                                                     jlong handle,
                                                                     jstring path) {
    auto* v = reinterpret_cast<osgverselib::OsgVerseViewer*>(handle);
    if (!v) return JNI_FALSE;
    return v->createPbrScene(toStdString(env, path)) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_example_osgverselib_NativeOsgVerseCore_nativeCreateEarthScene(JNIEnv* env, jclass,
                                                                       jlong handle,
                                                                       jstring orthoUrl,
                                                                       jstring startFile) {
    auto* v = reinterpret_cast<osgverselib::OsgVerseViewer*>(handle);
    if (!v) return JNI_FALSE;
    return v->createEarthScene(toStdString(env, orthoUrl), toStdString(env, startFile))
               ? JNI_TRUE
               : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_org_example_osgverselib_NativeOsgVerseCore_nativeSetEarthViewpoint(JNIEnv*, jclass,
                                                                       jlong handle,
                                                                       jdouble longitude,
                                                                       jdouble latitude,
                                                                       jdouble height) {
    auto* v = reinterpret_cast<osgverselib::OsgVerseViewer*>(handle);
    if (v) v->setEarthViewpoint(longitude, latitude, height);
}

extern "C" JNIEXPORT jint JNICALL
Java_org_example_osgverselib_NativeOsgVerseCore_nativeGetMode(JNIEnv*, jclass, jlong handle) {
    auto* v = reinterpret_cast<osgverselib::OsgVerseViewer*>(handle);
    return v ? static_cast<jint>(v->getMode()) : 0;
}

extern "C" JNIEXPORT void JNICALL
Java_org_example_osgverselib_NativeOsgVerseCore_nativeHome(JNIEnv*, jclass, jlong handle) {
    auto* v = reinterpret_cast<osgverselib::OsgVerseViewer*>(handle);
    if (v) v->home();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_example_osgverselib_NativeOsgVerseCore_nativeSaveScreenshot(JNIEnv* env, jclass,
                                                                     jlong handle,
                                                                     jstring path) {
    auto* v = reinterpret_cast<osgverselib::OsgVerseViewer*>(handle);
    if (!v) return JNI_FALSE;
    return v->saveScreenshot(toStdString(env, path)) ? JNI_TRUE : JNI_FALSE;
}

// ---- 输入事件转发 ----

extern "C" JNIEXPORT void JNICALL
Java_org_example_osgverselib_NativeOsgVerseCore_nativeMouseMove(JNIEnv*, jclass, jlong handle,
                                                                jint x, jint y) {
    auto* v = reinterpret_cast<osgverselib::OsgVerseViewer*>(handle);
    if (v) v->mouseMove(x, y);
}

extern "C" JNIEXPORT void JNICALL
Java_org_example_osgverselib_NativeOsgVerseCore_nativeMousePress(JNIEnv*, jclass, jlong handle,
                                                                 jint x, jint y, jint button) {
    auto* v = reinterpret_cast<osgverselib::OsgVerseViewer*>(handle);
    if (v) v->mouseButtonPress(x, y, button);
}

extern "C" JNIEXPORT void JNICALL
Java_org_example_osgverselib_NativeOsgVerseCore_nativeMouseRelease(JNIEnv*, jclass, jlong handle,
                                                                   jint x, jint y, jint button) {
    auto* v = reinterpret_cast<osgverselib::OsgVerseViewer*>(handle);
    if (v) v->mouseButtonRelease(x, y, button);
}

extern "C" JNIEXPORT void JNICALL
Java_org_example_osgverselib_NativeOsgVerseCore_nativeMouseScroll(JNIEnv*, jclass, jlong handle,
                                                                  jint updown, jint x, jint y) {
    auto* v = reinterpret_cast<osgverselib::OsgVerseViewer*>(handle);
    if (v) v->mouseScroll(updown, x, y);
}

extern "C" JNIEXPORT void JNICALL
Java_org_example_osgverselib_NativeOsgVerseCore_nativeKeyPress(JNIEnv*, jclass, jlong handle,
                                                               jint key) {
    auto* v = reinterpret_cast<osgverselib::OsgVerseViewer*>(handle);
    if (v) v->keyPress(key);
}

extern "C" JNIEXPORT void JNICALL
Java_org_example_osgverselib_NativeOsgVerseCore_nativeKeyRelease(JNIEnv*, jclass, jlong handle,
                                                                 jint key) {
    auto* v = reinterpret_cast<osgverselib::OsgVerseViewer*>(handle);
    if (v) v->keyRelease(key);
}
