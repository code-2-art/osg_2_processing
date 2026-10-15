#pragma once

// ============================================================
//  OsgViewer：把 OpenSceneGraph 嵌入到外部 OpenGL 上下文（如 Processing 窗口）
//  的封装。它只负责持有 osgViewer::Viewer 和 GraphicsWindowEmbedded，
//  不管理底层 GL 上下文（假设调用 frame() 时 Processing 的 GL 上下文已当前）。
// ============================================================

#include <osg/ref_ptr>
#include <string>

namespace osgViewer { class Viewer; class GraphicsWindowEmbedded; }
namespace osg { class Group; class Node; }

namespace osglib {

class OsgViewer {
public:
    OsgViewer(int width, int height);
    ~OsgViewer();

    OsgViewer(const OsgViewer&) = delete;
    OsgViewer& operator=(const OsgViewer&) = delete;

    /** 窗口尺寸变化时调用（更新视口与投影矩阵，并产生 RESIZE 事件）。 */
    void resize(int width, int height);

    /** 渲染一帧。必须在外部 GL 上下文当前的线程里调用。 */
    void frame();

    /** 载入一个模型文件（osg/ive/obj/3ds/gltf 等，由 OSG 插件支持）。 */
    bool loadModel(const std::string& path);

    /** 重置相机到默认观察位置（包围整个场景）。 */
    void home();

    /** 设置相机视点（眼坐标 + heading/pitch/roll，度）。 */
    void setViewpoint(double x, double y, double z, double h, double p, double r);

    // ---- 输入事件转发（供 OSG 漫游器使用） ----
    // 坐标原点在窗口左上角，Y 向下，与 Processing 一致。

    /** 鼠标移动（不含按键按下）。 */
    void mouseMove(int x, int y);
    /** 鼠标按键按下。button: 1=左 2=中 3=右（OSG 约定）。 */
    void mouseButtonPress(int x, int y, int button);
    /** 鼠标按键抬起。button: 1=左 2=中 3=右（OSG 约定）。 */
    void mouseButtonRelease(int x, int y, int button);
    /** 滚轮。updown: 1=上滚（放大） -1=下滚（缩小）。 */
    void mouseScroll(int updown, int x, int y);
    /** 键盘按下。key 为 osgGA::GUIEventAdapter::KeySymbol。 */
    void keyPress(int key);
    /** 键盘抬起。key 为 osgGA::GUIEventAdapter::KeySymbol。 */
    void keyRelease(int key);

    /**
     * 把当前帧缓冲（颜色）回读并保存为 PPM 文件。
     * 必须在 frame() 之后、GL 上下文仍当前的线程里调用。
     * 主要用于自动化测试与截图。
     */
    bool saveScreenshot(const std::string& path);

private:
    osg::ref_ptr<osgViewer::Viewer> _viewer;
    osg::ref_ptr<osgViewer::GraphicsWindowEmbedded> _gw;
    osg::ref_ptr<osg::Group> _root;      // 场景根（含光源）
    osg::ref_ptr<osg::Group> _content;   // 实际内容（演示场景或载入的模型）
    int _width;
    int _height;
};

}  // namespace osglib

