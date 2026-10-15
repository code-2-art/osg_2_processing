package org.example.osgverselib;

import processing.core.PApplet;
import processing.core.PConstants;

/**
 * OsgVerseView - 在 Processing 窗口里渲染 osgVerse 场景。
 *
 * <p>支持的场景：</p>
 * <ul>
 *   <li>{@link #createPbrScene(String)} —— PBR 材质 + 延迟渲染（G-Buffer / SSAO / 阴影 /
 *       泛光 / 色调映射 / FXAA），默认模型是 osgVerse SDK 自带的 Sponza。</li>
 *   <li>{@link #createEarthScene(String, String)} —— TMS 瓦片数字地球 + EarthManipulator
 *       漫游，默认使用高德影像瓦片。</li>
 * </ul>
 *
 * <p>用法：</p>
 * <pre>
 * import org.example.osgverselib.*;
 * OsgVerseView view;
 *
 * void setup() {
 *   size(1280, 720, P2D);           // 必须用 OpenGL 渲染器
 *   view = new OsgVerseView(this);  // 默认自动转发鼠标/键盘事件
 *   view.createPbrScene("");        // 或 view.createEarthScene("", "");
 * }
 *
 * void draw() {
 *   view.frame();
 * }
 * </pre>
 *
 * <p>注意：PBR 场景的渲染分辨率与管线内部分辨率绑定，窗口尺寸变化时会重建管线，
 * 重建过程有短暂卡顿属于正常现象。</p>
 */
public class OsgVerseView {

    /** 无场景。 */
    public static final int MODE_NONE = 0;
    /** PBR + 延迟渲染场景。 */
    public static final int MODE_PBR = 1;
    /** 数字地球场景。 */
    public static final int MODE_EARTH = 2;

    protected PApplet parent;
    protected long handle;
    protected boolean forwardEvents;

    /** 默认自动转发鼠标/键盘事件到 OSG（让漫游器开箱可用）。 */
    public OsgVerseView(PApplet theParent) {
        this(theParent, true);
    }

    /**
     * @param theParent     Processing sketch
     * @param forwardEvents 是否自动把鼠标/键盘事件转发给 OSG 漫游器
     */
    public OsgVerseView(PApplet theParent, boolean forwardEvents) {
        parent = theParent;
        this.forwardEvents = forwardEvents;
        NativeOsgVerseCore.ensureLoaded();
        handle = NativeOsgVerseCore.nativeCreateViewer(theParent.pixelWidth, theParent.pixelHeight);
        if (handle == 0) {
            throw new RuntimeException("osgverselib: failed to create native osgVerse viewer");
        }
        parent.registerMethod("dispose", this);
        if (forwardEvents) {
            parent.registerMethod("mouseEvent", this);
            parent.registerMethod("keyEvent", this);
        }
    }

    /** 渲染一帧 osgVerse 场景。建议放在 draw() 的末尾。 */
    public void frame() {
        if (handle == 0) return;
        NativeOsgVerseCore.nativeResize(handle, parent.pixelWidth, parent.pixelHeight);
        NativeOsgVerseCore.nativeFrame(handle);
    }

    /**
     * 创建 PBR + 延迟渲染场景。
     *
     * @param modelPath 模型文件路径；传空字符串或 null 时使用 SDK 自带的
     *                  <code>models/Sponza.osgb</code>
     * @return 是否创建成功
     */
    public boolean createPbrScene(String modelPath) {
        if (handle == 0) return false;
        return NativeOsgVerseCore.nativeCreatePbrScene(handle, modelPath == null ? "" : modelPath);
    }

    /**
     * 创建数字地球场景。
     *
     * @param orthoUrl  影像瓦片 URL 模板，需含 <code>{x}/{y}/{z}</code>；
     *                  传空字符串或 null 时使用高德影像瓦片
     * @param startFile 起始 TMS 根节点名；传空字符串或 null 时使用 <code>0-0-0.verse_tms</code>
     * @return 是否创建成功
     */
    public boolean createEarthScene(String orthoUrl, String startFile) {
        if (handle == 0) return false;
        return NativeOsgVerseCore.nativeCreateEarthScene(
            handle, orthoUrl == null ? "" : orthoUrl, startFile == null ? "" : startFile);
    }

    /** 地球相机定位到指定经纬度（度）与距地面高度（米）。仅地球场景有效。 */
    public void setEarthViewpoint(double longitude, double latitude, double height) {
        if (handle != 0) {
            NativeOsgVerseCore.nativeSetEarthViewpoint(handle, longitude, latitude, height);
        }
    }

    /** 当前场景类型（MODE_NONE / MODE_PBR / MODE_EARTH）。 */
    public int getMode() {
        if (handle == 0) return MODE_NONE;
        return NativeOsgVerseCore.nativeGetMode(handle);
    }

    /** 复位相机。 */
    public void home() {
        if (handle != 0) NativeOsgVerseCore.nativeHome(handle);
    }

    /**
     * 回读当前帧缓冲并保存为 PPM 文件（用于截图/自动化测试）。
     * 必须在 frame() 之后调用。
     */
    public boolean saveScreenshot(String path) {
        if (handle == 0) return false;
        return NativeOsgVerseCore.nativeSaveScreenshot(handle, path);
    }

    // ---- 手动事件转发 API（当关闭自动转发时使用） ----

    public void mouseMove(int x, int y) {
        if (handle != 0) NativeOsgVerseCore.nativeMouseMove(handle, x, y);
    }

    public void mousePress(int x, int y, int osgButton) {
        if (handle != 0) NativeOsgVerseCore.nativeMousePress(handle, x, y, osgButton);
    }

    public void mouseRelease(int x, int y, int osgButton) {
        if (handle != 0) NativeOsgVerseCore.nativeMouseRelease(handle, x, y, osgButton);
    }

    public void mouseScroll(int updown, int x, int y) {
        if (handle != 0) NativeOsgVerseCore.nativeMouseScroll(handle, updown, x, y);
    }

    public void keyPress(int osgKey) {
        if (handle != 0) NativeOsgVerseCore.nativeKeyPress(handle, osgKey);
    }

    public void keyRelease(int osgKey) {
        if (handle != 0) NativeOsgVerseCore.nativeKeyRelease(handle, osgKey);
    }

    // ---- Processing 事件回调（自动转发时由 Processing 调用） ----

    /** Processing 鼠标事件入口（registerMethod("mouseEvent") 回调）。 */
    public void mouseEvent(processing.event.MouseEvent e) {
        if (handle == 0) return;
        switch (e.getAction()) {
            case processing.event.MouseEvent.PRESS:
                NativeOsgVerseCore.nativeMousePress(handle, e.getX(), e.getY(), mapButton(e.getButton()));
                break;
            case processing.event.MouseEvent.RELEASE:
                NativeOsgVerseCore.nativeMouseRelease(handle, e.getX(), e.getY(), mapButton(e.getButton()));
                break;
            case processing.event.MouseEvent.DRAG:
            case processing.event.MouseEvent.MOVE:
                NativeOsgVerseCore.nativeMouseMove(handle, e.getX(), e.getY());
                break;
            case processing.event.MouseEvent.WHEEL:
                // Processing: count>0 表示向下滚 → OSG: -1 缩小
                int updown = (e.getCount() > 0) ? -1 : 1;
                NativeOsgVerseCore.nativeMouseScroll(handle, updown, e.getX(), e.getY());
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
                NativeOsgVerseCore.nativeKeyPress(handle, osgKey);
                break;
            case processing.event.KeyEvent.RELEASE:
                NativeOsgVerseCore.nativeKeyRelease(handle, osgKey);
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
                NativeOsgVerseCore.nativeDisposeViewer(handle);
            } catch (Throwable t) {
                System.err.println("osgverselib: nativeDisposeViewer warning: " + t);
            }
            handle = 0;
        }
    }
}
