package org.example.cpplib;

import processing.core.PApplet;

/**
 * GLView - “OpenGL 绘制”示例。
 *
 * <p>C++ 侧在同一 OpenGL 上下文里直接做原生 GL 绘制（编译着色器、上传 VBO、
 * 调用 glDrawArrays 等）。这样以后可以用 CUDA-OpenGL 互操作（例如把 CUDA 计算出的
 * 结果直接写进 VBO/纹理，然后由 OpenGL 渲染到 Processing 窗口）。</p>
 *
 * <p>重要前提与约定：</p>
 * <ol>
 *   <li>Sketch 必须使用 OpenGL 渲染器：size(w, h, P2D) 或 size(w, h, P3D)；</li>
 *   <li>render() 必须在 draw() 的<b>最后</b>调用。渲染前 wrapper 会先
 *       flush() Processing 本帧累积的几何，避免背景把原生内容盖掉；</li>
 *   <li>原生视图（native view）在第一次 render() 时才真正创建，
 *       因为那时 GL 上下文一定处于当前线程的激活状态。</li>
 * </ol>
 */
public class GLView {

    protected PApplet parent;
    protected long handle;
    protected boolean warnedNonGL;

    /**
     * @param theParent 宿主 sketch
     */
    public GLView(PApplet theParent) {
        parent = theParent;
        NativeCore.ensureLoaded();
        parent.registerMethod("dispose", this);
    }

    /**
     * 在 draw() 最后调用本方法：先 flush Processing，再让 C++ 画一帧原生 GL 内容。
     *
     * @param time 时间（秒），用于动画
     */
    public void render(float time) {
        if (parent.g == null || !parent.g.isGL()) {
            if (!warnedNonGL) {
                warnedNonGL = true;
                System.err.println("cpplib.GLView requires an OpenGL renderer: "
                        + "call size(w, h, P2D) or size(w, h, P3D) in setup().");
            }
            return;
        }

        if (handle == 0) {
            handle = NativeCore.nativeCreateGLView();
            if (handle == 0) {
                throw new RuntimeException("cpplib: failed to create native GL view");
            }
        }

        // 让 Processing 先把手头累积的几何（含 background 与之前画的内容）真正画出来，
        // 否则 endDraw() 里的 flush 会把我们刚画的原生内容覆盖掉。
        parent.g.flush();

        // pixelWidth/pixelHeight 是真实帧缓冲尺寸（考虑了像素密度）
        NativeCore.nativeRenderGL(handle, parent.pixelWidth, parent.pixelHeight, time);
    }

    /** 返回 C++ 侧该视图句柄；0 表示还未创建。 */
    public long nativeHandle() {
        return handle;
    }

    /** 释放原生视图。sketch 退出时会自动调用。 */
    public void dispose() {
        if (handle != 0) {
            try {
                NativeCore.nativeDisposeGL(handle);
            } catch (Throwable t) {
                // sketch 关闭时 GL 上下文可能已经失效，忽略清理阶段的异常
                System.err.println("cpplib: nativeDisposeGL warning: " + t);
            }
            handle = 0;
        }
    }
}
