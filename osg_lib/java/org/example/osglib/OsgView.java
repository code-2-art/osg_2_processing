package org.example.osglib;

import processing.core.PApplet;
import processing.core.PConstants;

/**
 * OsgView - 在 Processing 窗口里渲染 OpenSceneGraph 场景。
 *
 * <p>用法：</p>
 * <pre>
 * import org.example.osglib.*;
 * OsgView osg;
 *
 * void setup() {
 *   size(800, 600, P2D);   // 必须用 OpenGL 渲染器
 *   osg = new OsgView(this);   // 默认自动转发鼠标/键盘事件，OSG 漫游器开箱可用
 *   osg.loadModel("/path/to/model.osg");   // 可选：载入模型
 * }
 *
 * void draw() {
 *   osg.frame();           // 渲染一帧 OSG 场景
 * }
 * </pre>
 *
 * <p>重要前提：</p>
 * <ol>
 *   <li>Sketch 必须使用 OpenGL 渲染器：size(w, h, P2D) 或 P3D；</li>
 *   <li>frame() 必须在 draw() 里调用（此时 Processing 的 GL 上下文已当前）；</li>
 *   <li>OSG 不管理 GL 上下文，它通过 GraphicsWindowEmbedded 直接使用当前上下文。</li>
 * </ol>
 *
 * <p>事件转发：默认情况下 OsgView 会把自己注册为 Processing 的 mouseEvent / keyEvent
 * 监听器，把鼠标键盘事件转发给 OSG，使 {@code osgGA::CameraManipulator}（如
 * TrackballManipulator）能直接工作（拖拽旋转、滚轮缩放等）。如果你希望自己控制事件，
 * 可用 {@code new OsgView(this, false)} 关闭自动转发，再手动调用
 * mouseMove / mousePress / ... 等方法。</p>
 */
public class OsgView {

    protected PApplet parent;
    protected long handle;
    protected boolean forwardEvents;

    /** 默认自动转发鼠标/键盘事件到 OSG（让漫游器开箱可用）。 */
    public OsgView(PApplet theParent) {
        this(theParent, true);
    }

    /**
     * @param theParent     Processing sketch
     * @param forwardEvents 是否自动把鼠标/键盘事件转发给 OSG 漫游器
     */
    public OsgView(PApplet theParent, boolean forwardEvents) {
        parent = theParent;
        this.forwardEvents = forwardEvents;
        NativeOsgCore.ensureLoaded();
        handle = NativeOsgCore.nativeCreateViewer(theParent.pixelWidth, theParent.pixelHeight);
        if (handle == 0) {
            throw new RuntimeException("osglib: failed to create native OSG viewer");
        }
        parent.registerMethod("dispose", this);
        if (forwardEvents) {
            parent.registerMethod("mouseEvent", this);
            parent.registerMethod("keyEvent", this);
        }
    }

    /**
     * 渲染一帧 OSG 场景。建议放在 draw() 的最后。
     */
    public void frame() {
        if (handle == 0) return;
        // 确保尺寸同步（窗口缩放时）
        NativeOsgCore.nativeResize(handle, parent.pixelWidth, parent.pixelHeight);
        NativeOsgCore.nativeFrame(handle);
    }

    /** 加载一个 3D 模型文件（osg/ive/obj/3ds/gltf 等，取决于 OSG 插件）。 */
    public boolean loadModel(String path) {
        if (handle == 0) return false;
        return NativeOsgCore.nativeLoadModel(handle, path);
    }

    /** 重置相机到默认观察位置（包围整个场景）。 */
    public void home() {
        if (handle != 0) NativeOsgCore.nativeHome(handle);
    }

    /** 设置相机视点（眼位置 + 朝向角，度）。 */
    public void setViewpoint(double x, double y, double z,
                             double heading, double pitch, double roll) {
        if (handle != 0) {
            NativeOsgCore.nativeSetViewpoint(handle, x, y, z, heading, pitch, roll);
        }
    }

    /**
     * 回读当前帧缓冲并保存为 PPM 文件（用于截图/自动化测试）。
     * 必须在 frame() 之后调用。
     */
    public boolean saveScreenshot(String path) {
        if (handle == 0) return false;
        return NativeOsgCore.nativeSaveScreenshot(handle, path);
    }

    // ---- 手动事件转发 API（当关闭自动转发时使用） ----

    public void mouseMove(int x, int y) {
        if (handle != 0) NativeOsgCore.nativeMouseMove(handle, x, y);
    }

    public void mousePress(int x, int y, int osgButton) {
        if (handle != 0) NativeOsgCore.nativeMousePress(handle, x, y, osgButton);
    }

    public void mouseRelease(int x, int y, int osgButton) {
        if (handle != 0) NativeOsgCore.nativeMouseRelease(handle, x, y, osgButton);
    }

    public void mouseScroll(int updown, int x, int y) {
        if (handle != 0) NativeOsgCore.nativeMouseScroll(handle, updown, x, y);
    }

    public void keyPress(int osgKey) {
        if (handle != 0) NativeOsgCore.nativeKeyPress(handle, osgKey);
    }

    public void keyRelease(int osgKey) {
        if (handle != 0) NativeOsgCore.nativeKeyRelease(handle, osgKey);
    }

    // ---- Processing 事件回调（自动转发时由 Processing 调用） ----

    /** Processing 鼠标事件入口（registerMethod("mouseEvent") 回调）。 */
    public void mouseEvent(processing.event.MouseEvent e) {
        if (handle == 0) return;
        switch (e.getAction()) {
            case processing.event.MouseEvent.PRESS:
                NativeOsgCore.nativeMousePress(handle, e.getX(), e.getY(), mapButton(e.getButton()));
                break;
            case processing.event.MouseEvent.RELEASE:
                NativeOsgCore.nativeMouseRelease(handle, e.getX(), e.getY(), mapButton(e.getButton()));
                break;
            case processing.event.MouseEvent.DRAG:
            case processing.event.MouseEvent.MOVE:
                NativeOsgCore.nativeMouseMove(handle, e.getX(), e.getY());
                break;
            case processing.event.MouseEvent.WHEEL:
                // Processing: count>0 表示向下滚 → OSG: -1 缩小
                int updown = (e.getCount() > 0) ? -1 : 1;
                NativeOsgCore.nativeMouseScroll(handle, updown, e.getX(), e.getY());
                break;
            default:
                break;
        }
    }

    /** Processing 键盘事件入口（registerMethod("keyEvent") 回调）。 */
    public void keyEvent(processing.event.KeyEvent e) {
        if (handle == 0) return;
        int osgKey = mapKey(e.getKeyCode());
        switch (e.getAction()) {
            case processing.event.KeyEvent.PRESS:
                NativeOsgCore.nativeKeyPress(handle, osgKey);
                break;
            case processing.event.KeyEvent.RELEASE:
                NativeOsgCore.nativeKeyRelease(handle, osgKey);
                break;
            default:
                break;
        }
    }

    /** Processing 鼠标按键 → OSG 按键（1=左 2=中 3=右）。 */
    protected static int mapButton(int processingButton) {
        switch (processingButton) {
            case PConstants.LEFT:   return 1;
            case PConstants.CENTER: return 2;
            case PConstants.RIGHT:  return 3;
            default:                return 1;
        }
    }

    /** Processing keyCode → osgGA::GUIEventAdapter::KeySymbol。 */
    protected static int mapKey(int code) {
        switch (code) {
            case 37: return 0xFF51; // LEFT
            case 38: return 0xFF52; // UP
            case 39: return 0xFF53; // RIGHT
            case 40: return 0xFF54; // DOWN
            case 10:
            case 13: return 0xFF0D; // ENTER
            case 27: return 0xFF1B; // ESC
            case 8:  return 0xFF08; // BACKSPACE
            case 9:  return 0xFF09; // TAB
            case 16: return 0xFFE1; // SHIFT
            case 17: return 0xFFE3; // CONTROL
            case 18: return 0xFFE9; // ALT
            case 127: return 0xFFFF; // DELETE
            case 36: return 0xFF50; // HOME
            case 35: return 0xFF57; // END
            case 33: return 0xFF55; // PAGE UP
            case 34: return 0xFF56; // PAGE DOWN
            default: return code;   // 字母/数字/空格等与 ASCII 一致
        }
    }

    /** sketch 退出时自动调用。 */
    public void dispose() {
        if (handle != 0) {
            try {
                NativeOsgCore.nativeDisposeViewer(handle);
            } catch (Throwable t) {
                System.err.println("osglib: nativeDisposeViewer warning: " + t);
            }
            handle = 0;
        }
    }
}
