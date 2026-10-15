package org.example.osglib;

import java.io.File;

/**
 * NativeOsgCore - JNI 宿主类。
 *
 * <p>负责加载原生动态库（libosglib.so / osglib.dll）并声明所有 native 方法。
 * 原生库会进一步链接 OpenSceneGraph，因此运行环境必须能找到 OSG 的 .so/.dll
 * （通过 rpath 或 LD_LIBRARY_PATH / PATH）。</p>
 *
 * <p>加载路径规则：默认从本 jar 同级的 {@code native/<os>-<arch>/} 目录加载，
 * 即发布包里的 {@code library/native/<os>-<arch>/libosglib.so}。</p>
 */
final class NativeOsgCore {

    private static boolean nativeLoaded = false;

    private NativeOsgCore() {
    }

    static synchronized void ensureLoaded() {
        if (nativeLoaded) {
            return;
        }
        String libFileName = System.mapLibraryName("osglib");
        String path = findNativeLibrary(libFileName);
        if (path == null) {
            System.loadLibrary("osglib");
        } else {
            System.load(path);
        }
        nativeLoaded = true;
    }

    private static String findNativeLibrary(String libFileName) {
        String overrideDir = System.getProperty("osglib.native.dir");
        if (overrideDir != null) {
            File f = new File(overrideDir, libFileName);
            if (f.isFile()) {
                return f.getAbsolutePath();
            }
        }
        try {
            File jarFile = new File(
                NativeOsgCore.class.getProtectionDomain().getCodeSource().getLocation().toURI());
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

    // ---- Native 方法声明（C++ 实现在 osg_lib/src/bridge.cpp） ----

    static native long nativeCreateViewer(int width, int height);

    static native void nativeDisposeViewer(long handle);

    static native void nativeResize(long handle, int width, int height);

    /** 渲染一帧 OSG 场景。调用前必须确保 Processing 的 OpenGL 上下文已当前。 */
    static native void nativeFrame(long handle);

    static native boolean nativeLoadModel(long handle, String path);

    static native void nativeHome(long handle);

    static native void nativeSetViewpoint(long handle,
                                          double x, double y, double z,
                                          double h, double p, double r);

    /** 回读当前帧缓冲并保存为 PPM 文件（用于截图/测试）。 */
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
