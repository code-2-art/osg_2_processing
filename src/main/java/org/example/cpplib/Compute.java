package org.example.cpplib;

import processing.core.PApplet;

/**
 * Compute - “纯算法”示例。
 *
 * <p>在 C++ 侧持有一个长期存活的“计算引擎”对象（句柄为 long），
 * 通过 JNI 把重计算任务交给 C++ / CUDA，只把结果（例如像素数组）拿回 Java。</p>
 *
 * <p>这种“在 setup 里创建引擎 -> 每帧调用 -> 退出时 dispose”的模式，
 * 就是你以后把矩阵乘、FFT、神经网络推理等 CUDA/AI 算法放进 C++ 侧的基本框架。</p>
 *
 * <p>用法：</p>
 * <pre>
 * Compute compute = new Compute(this);
 * compute.mandelbrot(img.pixels, 320, 180, 200, x0, y0, x1, y1);
 * img.updatePixels();
 * </pre>
 */
public class Compute {

    /** parent 是对宿主 PApplet（sketch）的引用，Processing 类库惯例。 */
    protected PApplet parent;

    /** 指向 C++ ComputeEngine 对象的不透明句柄。0 表示尚未创建/已销毁。 */
    protected long handle;

    /**
     * 创建计算引擎。
     *
     * @param theParent 宿主 sketch
     */
    public Compute(PApplet theParent) {
        parent = theParent;
        NativeCore.ensureLoaded();
        handle = NativeCore.nativeCreateComputeEngine();
        if (handle == 0) {
            throw new RuntimeException("cpplib: failed to create native compute engine");
        }
        // 注册 sketch 退出回调：sketch 关闭时会自动调用 dispose()，释放 C++ 内存
        parent.registerMethod("dispose", this);
    }

    /** 返回后端构建信息（例如 CPU 线程数 / CUDA 是否启用）。 */
    public String info() {
        return NativeCore.nativeComputeInfo(handle);
    }

    /**
     * 计算 Mandelbrot 集合并写入 rgb 数组。
     *
     * @param rgb     长度为 w*h 的 ARGB 像素数组（通常是 PImage.pixels）
     * @param w       输出宽度
     * @param h       输出高度
     * @param maxIter 最大迭代次数，越大细节越多、越慢
     * @param x0, y0  复平面窗口左上角
     * @param x1, y1  复平面窗口右下角
     */
    public void mandelbrot(int[] rgb, int w, int h, int maxIter,
                           double x0, double y0, double x1, double y1) {
        NativeCore.nativeComputeMandelbrot(handle, rgb, w, h, maxIter, x0, y0, x1, y1);
    }

    /** 释放原生引擎。也可由 Processing 在 sketch 退出时自动调用。 */
    public void dispose() {
        if (handle != 0) {
            NativeCore.nativeDisposeComputeEngine(handle);
            handle = 0;
        }
    }
}
