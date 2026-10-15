package org.example.cpplib;

/**
 * JNI 冒烟测试：不依赖 Processing 窗口，直接调用 NativeCore 的静态 native 方法，
 * 验证 DLL 能被找到并正确加载、计算引擎可用、数组能正确来回传递。
 *
 * 由 tools/build.ps1 自动编译运行。运行前请先构建好
 * dist/cpplib/library/cpplib.jar 与 library/native/windows-amd64/cpplib.dll。
 */
public class TestMain {

    public static void main(String[] args) {
        // 触发 DLL 加载（必须先于任何 native 方法调用）
        NativeCore.ensureLoaded();
        // 触发 DLL 加载
        System.out.println("[1] nativeVersionString -> " + NativeCore.nativeVersionString());

        // 创建/销毁计算引擎
        long engine = NativeCore.nativeCreateComputeEngine();
        if (engine == 0) {
            throw new IllegalStateException("nativeCreateComputeEngine returned 0");
        }
        System.out.println("[2] nativeComputeInfo     -> " + NativeCore.nativeComputeInfo(engine));

        // 跑一小块 Mandelbrot（96x54，CPU 或 CUDA 都很快）
        int w = 96, h = 54;
        int[] pixels = new int[w * h];
        NativeCore.nativeComputeMandelbrot(engine, pixels, w, h, 120,
                -2.0, -1.2, 0.8, 1.2);
        int nonBlack = 0;
        for (int v : pixels) {
            if (v != 0xFF000000) {
                nonBlack++;
            }
        }
        System.out.println("[3] mandelbrot: " + nonBlack + " non-black pixels / " + (w * h));
        if (nonBlack < w * h / 100) {
            throw new IllegalStateException("compute result suspicious: " + nonBlack);
        }

        NativeCore.nativeDisposeComputeEngine(engine);
        System.out.println("[OK] JNI TEST PASSED");
    }
}
