package org.example.cpplib;

import java.io.File;

/**
 * NativeCore - JNI 宿主类。
 *
 * <p>这个类只做两件事：</p>
 * <ol>
 *   <li>在第一次使用时定位并加载原生动态库（cpplib.dll / libcpplib.so）；</li>
 *   <li>声明所有 native 方法（即 C++ 侧 JNI 导出函数的对应 Java 签名）。</li>
 * </ol>
 *
 * <p>重要：如果你修改了这里的 native 方法签名，需要重新生成 C++ 的 JNI 头文件，
 * 生成方式见 README（javac -h）或直接运行 tools/build-and-install.ps1。</p>
 *
 * <p>加载路径规则：默认从本 jar 同级的 {@code native/<os>-<arch>/} 目录加载，
 * 也就是发布包里的 {@code library/native/<os>-<arch>/cpplib.dll}。</p>
 */
final class NativeCore {

    /** native 库是否已经加载成功（保证幂等）。 */
    private static boolean nativeLoaded = false;

    private NativeCore() {
    }

    /**
     * 确保原生库已加载。在第一次真正调用 native 方法之前调用一次即可。
     */
    static synchronized void ensureLoaded() {
        if (nativeLoaded) {
            return;
        }

        String libFileName = System.mapLibraryName("cpplib"); // cpplib.dll / libcpplib.so
        String path = findNativeLibrary(libFileName);
        if (path == null) {
            // 最后的回退：依赖 java.library.path
            System.loadLibrary("cpplib");
        } else {
            System.load(path);
        }
        nativeLoaded = true;
    }

    /**
     * 依次尝试几种方式定位原生动态库。
     *
     * @return 找到的文件绝对路径；找不到返回 null
     */
    private static String findNativeLibrary(String libFileName) {
        // 1) 允许用户用系统属性覆盖 native 目录（便于调试，通常不需要）
        String overrideDir = System.getProperty("cpplib.native.dir");
        if (overrideDir != null) {
            File f = new File(overrideDir, libFileName);
            if (f.isFile()) {
                return f.getAbsolutePath();
            }
        }

        // 2) 根据本类所在 jar 的位置推导：
        //    <...>/cpplib/library/cpplib.jar
        //        -> native 目录 = <...>/cpplib/library/native/<os>-<arch>/cpplib.dll
        try {
            File jarFile = new File(
                NativeCore.class.getProtectionDomain().getCodeSource().getLocation().toURI());
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

    /** 例如 windows-amd64 / linux-amd64 / macosx-aarch64 ... */
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

        String arch;
        if (osArch.contains("aarch64") || osArch.contains("arm64")) {
            arch = "aarch64";
        } else {
            // amd64 / x86_64 -> amd64
            arch = osArch.contains("64") ? "amd64" : "x86";
        }
        return os + "-" + arch;
    }

    // ---------------------------------------------------------------
    // Native 方法声明。C++ 侧实现见 cpp/src/bridge.cpp，
    // 头文件为 native/include/org_example_cpplib_NativeCore.h（由 javac -h 生成）。
    // 警告：修改此处方法后请务必重新生成头文件！
    // ---------------------------------------------------------------

    /** 返回库的版本与后端（CPU / CUDA）信息字符串，不创建任何引擎。 */
    static native String nativeVersionString();

    // ---- 纯算法（Compute）引擎 ----

    /** 创建计算引擎（可能初始化 CUDA 上下文），返回指向 C++ 对象的句柄（long）。 */
    static native long nativeCreateComputeEngine();

    /** 销毁计算引擎。 */
    static native void nativeDisposeComputeEngine(long engine);

    /** 查询该引擎的构建信息（CPU 核数 / CUDA 是否可用等）。 */
    static native String nativeComputeInfo(long engine);

    /**
     * 用 CPU 多线程（或 CUDA）计算 Mandelbrot 集合，并把 0xAARRGGBB 像素写入 rgb。
     *
     * @param engine  引擎句柄
     * @param rgb     长度为 w*h 的输出数组
     * @param w       宽度
     * @param h       高度
     * @param maxIter 最大迭代次数
     * @param x0,y0,x1,y1 复平面可视窗口 [x0,x1] x [y0,y1]
     */
    static native void nativeComputeMandelbrot(long engine, int[] rgb, int w, int h,
                                               int maxIter,
                                               double x0, double y0, double x1, double y1);

    // ---- OpenGL 视图引擎 ----

    /**
     * 创建一个 OpenGL 渲染引擎。必须在 Processing 的 OpenGL 上下文已激活时调用
     * （例如 draw() 内、或 setup() 创建完画布之后）。返回句柄。
     */
    static native long nativeCreateGLView();

    /** 画布尺寸变化时通知原生层（可选）。 */
    static native void nativeResizeGL(long view, int fbWidth, int fbHeight);

    /**
     * 在 Processing 当前绑定的帧缓冲上执行一帧原生 OpenGL 绘制。
     * 调用前请先调用 parent.g.flush()，否则 Processing 待渲染的背景/图形会盖住原生内容。
     */
    static native void nativeRenderGL(long view, int fbWidth, int fbHeight, float time);

    /** 销毁 OpenGL 渲染引擎（需要 GL 上下文仍可用，一般在 dispose 事件里处理）。 */
    static native void nativeDisposeGL(long view);
}
