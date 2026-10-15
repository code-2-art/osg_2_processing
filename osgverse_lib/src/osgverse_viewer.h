#pragma once

// ============================================================
//  OsgVerseViewer：把 osgVerse 的场景跑在外部 OpenGL 上下文（Processing 窗口）里。
//
//  和 osg_lib 的 OsgViewer 一样，它只用 osgViewer::GraphicsWindowEmbedded 接入
//  外部上下文，不管理 GL 上下文、不调用 makeCurrent。
//
//  支持两种场景（二者互斥，后创建的会替换掉前者）：
//    1. createPbrScene()   —— PBR 材质 + 延迟渲染管线 + 阴影 + 后处理
//    2. createEarthScene() —— TMS 瓦片数字地球 + EarthManipulator 漫游
// ============================================================

#include <osg/ref_ptr>
#include <osgVerse/pipeline/Pipeline.h>

#include <string>

namespace osg { class Group; class Node; class Camera; }
namespace osgViewer { class GraphicsWindowEmbedded; }
namespace osgVerse { class LightDrawable; class SkyBox; }

namespace osgverselib {

class OsgVerseViewer {
public:
    enum Mode { MODE_NONE = 0, MODE_PBR = 1, MODE_EARTH = 2 };

    OsgVerseViewer(int width, int height);
    ~OsgVerseViewer();

    OsgVerseViewer(const OsgVerseViewer&) = delete;
    OsgVerseViewer& operator=(const OsgVerseViewer&) = delete;

    /** 窗口尺寸变化时调用（会同步视口/投影，并重建延迟管线的内部缓冲）。 */
    void resize(int width, int height);

    /** 渲染一帧。必须在外部 GL 上下文当前的线程里调用。 */
    void frame();

    /**
     * 创建 PBR + 延迟渲染场景。
     * @param modelPath 模型文件；为空时使用 SDK 自带的 models/Sponza.osgb
     */
    bool createPbrScene(const std::string& modelPath);

    /**
     * 创建数字地球场景（TMS 插件 + EarthManipulator）。
     * @param orthoUrl 影像瓦片 URL 模板（含 {x}/{y}/{z}）；为空时使用高德地图影像瓦片。
     * @param startFile 起始 TMS 根节点名，默认 "0-0-0.verse_tms"
     */
    bool createEarthScene(const std::string& orthoUrl, const std::string& startFile);

    /** 让地球相机定位到指定经纬度（度）与距地面高度（米）。 */
    void setEarthViewpoint(double longitude, double latitude, double height);

    /** 复位相机（PBR 场景回到包围整个场景的位置；地球场景回到默认视角）。 */
    void home();

    /** 把当前帧缓冲（颜色）回读并保存为 PPM 文件。必须在 frame() 之后调用。 */
    bool saveScreenshot(const std::string& path);

    /** 当前场景类型。 */
    Mode getMode() const { return _mode; }

    // ---- 输入事件转发（坐标原点左上角、Y 向下；按键 1=左 2=中 3=右） ----

    void mouseMove(int x, int y);
    void mouseButtonPress(int x, int y, int button);
    void mouseButtonRelease(int x, int y, int button);
    void mouseScroll(int updown, int x, int y);
    void keyPress(int key);
    void keyRelease(int key);

private:
    class PipelineViewer;

    void ensurePbrParams();
    void setupPbrPipeline(int width, int height);
    void wirePbrModules();

    osg::ref_ptr<PipelineViewer> _viewer;
    osg::ref_ptr<osgViewer::GraphicsWindowEmbedded> _gw;
    osg::ref_ptr<osg::Group> _root;      // 场景根（作为 viewer 的 scene data）
    osg::ref_ptr<osg::Group> _content;   // 当前场景内容
    osg::ref_ptr<osg::Group> _pbrRoot;   // PBR 场景根（含天空盒相机、光源等）

    osg::ref_ptr<osgVerse::Pipeline> _pipeline;
    osg::ref_ptr<osgVerse::LightDrawable> _mainLight;
    osg::ref_ptr<osg::Camera> _pbrPostCamera;
    osg::ref_ptr<osgVerse::SkyBox> _pbrSkybox;
    osg::ref_ptr<osg::Node> _shadowFrustum;
    osgVerse::StandardPipelineParameters _pbrParams;

    osg::ref_ptr<osg::Node> _worldNode;  // 地球场景的 world node

    Mode _mode;
    int _width;
    int _height;
    bool _pbrParamsReady;
};

}  // namespace osgverselib
