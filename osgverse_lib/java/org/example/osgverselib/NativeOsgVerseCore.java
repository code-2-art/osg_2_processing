package org.example.osgverselib;

import java.io.File;

/**
 * NativeOsgVerseCore - JNI 宿主类。
 *
 * <p>负责加载原生动态库（libosgverselib.so）并声明所有 native 方法。原生库会链接
 * osgVerse 与 OpenSceneGraph，运行时需要能找到 SDK 的 <code>lib/</code>
 * （已通过 rpath 写入，或用 LD_LIBRARY_PATH 指定）。</p>
 */
final class NativeOsgVerseCore {

    private static boolean nativeLoaded = false;

    private NativeOsgVerseCore() {
    }

    static synchronized void ensureLoaded() {
        if (nativeLoaded) {
            return;
        }
        String libFileName = System.mapLibraryName("osgverselib");
        String path = findNativeLibrary(libFileName);
        if (path == null) {
            System.loadLibrary("osgverselib");
        } else {
            System.load(path);
        }
        nativeLoaded = true;
    }

    private static String findNativeLibrary(String libFileName) {
        String overrideDir = System.getProperty("osgverselib.native.dir");
        if (overrideDir != null) {
            File f = new File(overrideDir, libFileName);
            if (f.isFile()) {
                return f.getAbsolutePath();
            }
        }
        try {
            File jarFile = new File(
                NativeOsgVerseCore.class.getProtectionDomain().getCodeSource().getLocation().toURI());
            File baseDir = jarFile.isDirectory() ? jarFile : jarFile.getParentFile();
            File subDir = new File(baseDir, "native/" + platformSubdir());
            File f = new File(subDir, libFileName);
            if (f.isFile()) {
                return f.getAbsolutePath();
            }
        } catch (Exception e) {
            // ignore and continue to fallback
        }
        return null;
    }

    private static String platformSubdir() {
        String osName = System.getProperty("os.name", "").toLowerCase();
        String osArch = System.getProperty("os.arch", "").toLowerCase();
        String os;
        if (osName.contains("win")) {
            os = "windows";
        } else if (osName.contains("mac") || osName.contains("darwin")) {
            os = "macosx";
        } else if (osName.contains("linux")) {
            os = "linux";
        } else {
            os = osName.replaceAll("\\s", "");
        }
        String arch = (osArch.contains("aarch64") || osArch.contains("arm64"))
            ? "aarch64"
            : (osArch.contains("64") ? "amd64" : "x86");
        return os + "-" + arch;
    }

    // ---- Native 方法声明（C++ 实现在 osgverse_lib/src/bridge.cpp） ----

    static native long nativeCreateViewer(int width, int height);

    static native void nativeDisposeViewer(long handle);

    static native void nativeResize(long handle, int width, int height);

    /** 渲染一帧。调用前必须确保 Processing 的 OpenGL 上下文已当前。 */
    static native void nativeFrame(long handle);

    /** PBR + 延迟渲染场景。path 为空字符串时用 SDK 自带的 Sponza.osgb。 */
    static native boolean nativeCreatePbrScene(long handle, String path);

    /** 数字地球场景（TMS 瓦片）。orthoUrl / startFile 为空字符串时用默认值。 */
    static native boolean nativeCreateEarthScene(long handle, String orthoUrl, String startFile);

    /** 地球相机定位：经度（度）、纬度（度）、距地面高度（米）。 */
    static native void nativeSetEarthViewpoint(long handle, double longitude, double latitude,
                                               double height);

    /** 当前场景类型：0=无 1=PBR 2=地球。 */
    static native int nativeGetMode(long handle);

    static native void nativeHome(long handle);

    /** 回读当前帧缓冲并保存为 PPM 文件（用于截图/自动化测试）。 */
    static native boolean nativeSaveScreenshot(long handle, String path);

    // ---- 输入事件转发（供 OSG 漫游器使用） ----

    /** 鼠标移动。坐标原点左上角，Y 向下。 */
    static native void nativeMouseMove(long handle, int x, int y);

    /** 鼠标按键按下。button: 1=左 2=中 3=右（OSG 约定）。 */
    static native void nativeMousePress(long handle, int x, int y, int button);

    /** 鼠标按键抬起。button: 1=左 2=中 3=右（OSG 约定）。 */
    static native void nativeMouseRelease(long handle, int x, int y, int button);

    /** 滚轮。updown: 1=上滚 -1=下滚。 */
    static native void nativeMouseScroll(long handle, int updown, int x, int y);

    /** 键盘按下。key 为 osgGA::GUIEventAdapter::KeySymbol。 */
    static native void nativeKeyPress(long handle, int key);

    /** 键盘抬起。key 为 osgGA::GUIEventAdapter::KeySymbol。 */
    static native void nativeKeyRelease(long handle, int key);
}
