#include "osgverse_viewer.h"

#include <osg/io_utils>
#include <osg/Geode>
#include <osg/Group>
#include <osg/MatrixTransform>
#include <osg/Program>
#include <osg/Uniform>
#include <osg/GraphicsContext>
#include <osg/ContextData>
#include <osg/Notify>
#include <osg/State>
#include <osgDB/ReadFile>
#include <osgDB/Registry>
#include <osgGA/EventQueue>
#include <osgGA/GUIEventAdapter>
#include <osgGA/TrackballManipulator>
#include <osgViewer/GraphicsWindow>
#include <osgViewer/Viewer>

#include <osgVerse/pipeline/Global.h>
#include <osgVerse/pipeline/LightDrawable.h>
#include <osgVerse/pipeline/LightModule.h>
#include <osgVerse/pipeline/Pipeline.h>
#include <osgVerse/pipeline/ShadowModule.h>
#include <osgVerse/pipeline/SkyBox.h>
#include <osgVerse/pipeline/Utilities.h>
#include <osgVerse/readerwriter/EarthManipulator.h>
#include <osgVerse/readerwriter/Utilities.h>
#include <osgVerse/animation/TweenAnimation.h>
#include <osgVerse/wrappers/Export.h>

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace osgverselib {

namespace {

// 默认影像瓦片：高德地图（和 osgVerse 的 earth_test 保持一致）
const char* kDefaultOrthoUrl =
    "https://webst01.is.autonavi.com/appmaptile?style%3d6&x%3d{x}&y%3d{y}&z%3d{z}";

// ------------------------------------------------------------
// 释放所有「已经没有使用者」的 osg::ContextData。
//
// 为什么需要这一步：GraphicsWindowEmbedded 在 OSG 内部被大量对象长期持有
// （实测有 17 个引用），所以 ~GraphicsContext 不会在 viewer 析构时触发，
// contextID 对应的 ContextData 就会一直留在 libosg 的静态 map 里直到进程退出。
// 而 libosg 里静态对象的析构顺序是：ContextData 的 map 晚于 GLExtensions 里
// s_glExtensionSetList 之类的静态数组，于是
//     ~ContextData → ~VertexArrayStateManager → ~VertexArrayState → ~GLExtensions
// 会去访问已被析构的静态数组，导致 JVM 在退出时 SIGSEGV（栈顶是
// osg::GLExtensions::~GLExtensions()）。
//
// 这里在 GL 上下文和 OSG 静态对象都还健在的时候主动把空闲的 ContextData
// 释放掉，避开这个析构顺序问题。
// ------------------------------------------------------------
void releaseIdleContextData() {
    const unsigned int maxId = osg::GraphicsContext::getMaxContextID();
    for (unsigned int id = 0; id <= maxId; ++id) {
        osg::ContextData* cd = osg::getContextData(id);
        if (cd && cd->getNumContexts() == 0) {
            osg::GraphicsContext::decrementContextIDUsageCount(id);
        }
    }
}

// ------------------------------------------------------------
// osgVerse 的全局初始化：只做一次。
//   - 设置 BASE_DIR（= SDK 根目录），这样 SHADER_DIR / SKYBOX_DIR / MISC_DIR
//     都指向 SDK 自带的 shaders/skyboxes/misc 目录；
//   - 注册 osgVerse 的读文件回调（自动清理固定管线数据、生成切线等，PBR 需要）；
//   - 注册 osgdb_verse_* 插件（TMS / 网络下载 / 地形等）。
// 注意：这里传 argc=0，避免 osgVerse 去改 Processing 的当前工作目录。
// ------------------------------------------------------------
void ensureGlobalInit() {
    static bool done = false;
    if (done) return;
    done = true;

    // 先加上 SDK 的 lib 目录，globalInitialize() 里按名字加载的插件才找得到
    osgDB::Registry::instance()->getLibraryFilePathList().push_back(
        std::string(OSGVERSE_SDK_ROOT) + "/lib");

    osgVerse::InitParameters params = osgVerse::defaultInitParameters();
    params.verseBaseDir = OSGVERSE_SDK_ROOT;
    osgVerse::globalInitialize(0, nullptr, params);
    // 注意：osgVerse 的 globalInitialize() 只把 BASE_DIR 设成 verseBaseDir，
    // SHADER_DIR / SKYBOX_DIR / MISC_DIR 仍是静态初始化时的 "../xxx/"（它假定
    // 可执行文件在 <BASE_DIR>/bin 下运行）。嵌入式场景里我们不改工作目录，
    // 所以这里必须自己把它们补齐。
    BASE_DIR = OSGVERSE_SDK_ROOT;
    SHADER_DIR = BASE_DIR + "/shaders/";
    SKYBOX_DIR = BASE_DIR + "/skyboxes/";
    MISC_DIR = BASE_DIR + "/misc/";

    osg::setNotifyHandler(new osgVerse::ConsoleHandler);
    osgVerse::updateOsgBinaryWrappers();

    osgDB::Registry::instance()->addFileExtensionAlias("tif", "verse_tiff");

    // OSG 默认会逐次检查 GL 错误。osgVerse 的 LightDrawable 在 GL Core 下会尝试应用
    // 固定管线的 GL_LIGHTING（非法枚举），如果逐次检查就会每帧往控制台刷警告。
    // 改成「每个属性只报一次」，既不丢诊断信息，也不会刷屏。
    if (!getenv("OSG_GL_ERROR_CHECKING")) {
        setenv("OSG_GL_ERROR_CHECKING", "ONCE_PER_ATTRIBUTE", 0);
    }
}

}  // namespace

// ------------------------------------------------------------
// 覆盖 Viewer::createRenderer()，让 viewer 使用 osgVerse 延迟管线的 Renderer。
// 必须在第一次 frame()（即 realize）之前把 _pipeline 设好。
// ------------------------------------------------------------
class OsgVerseViewer::PipelineViewer : public osgViewer::Viewer {
public:
    PipelineViewer() = default;
    osg::ref_ptr<osgVerse::Pipeline> pipeline;

protected:
    osg::GraphicsOperation* createRenderer(osg::Camera* camera) override {
        if (pipeline.valid()) return pipeline->createRenderer(camera);
        return osgViewer::Viewer::createRenderer(camera);
    }
};

OsgVerseViewer::OsgVerseViewer(int width, int height)
    : _mode(MODE_NONE), _width(width), _height(height), _pbrParamsReady(false) {
    ensureGlobalInit();

    _viewer = new PipelineViewer;

    // 关键：用 GraphicsWindowEmbedded 接入外部（Processing）的 GL 上下文，
    // 它不调用 makeCurrent / swapBuffers，假设上下文始终当前。
    _gw = _viewer->setUpViewerAsEmbeddedInWindow(0, 0, width, height);
    if (_gw.valid()) {
        _gw->getEventQueue()->windowResize(0, 0, width, height);
    }

    _viewer->getCamera()->setClearColor(osg::Vec4(0.1f, 0.1f, 0.12f, 1.0f));

    // 延迟管线的阴影/后处理阶段依赖跨阶段的渲染顺序，必须单线程
    _viewer->setThreadingModel(osgViewer::Viewer::SingleThreaded);

    _root = new osg::Group;
    _content = new osg::Group;
    _root->addChild(_content);
    _viewer->setSceneData(_root);

    // 默认用轨迹球；地球场景会替换成 EarthManipulator
    _viewer->setCameraManipulator(new osgGA::TrackballManipulator);
    _viewer->home();
}

OsgVerseViewer::~OsgVerseViewer() {
    // 顺序不敏感，全部显式释放掉，保证 scene graph / 管线 / 窗口都先于本对象销毁
    _viewer = nullptr;
    _gw = nullptr;
    _pipeline = nullptr;
    _pbrPostCamera = nullptr;
    _pbrSkybox = nullptr;
    _shadowFrustum = nullptr;
    _mainLight = nullptr;
    _worldNode = nullptr;
    _content = nullptr;
    _pbrRoot = nullptr;
    _root = nullptr;

    // 必须在 GL 上下文（Processing 的）仍然有效时调用，见函数注释
    releaseIdleContextData();
}

// ------------------------------------------------------------
// PBR + 延迟渲染场景
// ------------------------------------------------------------
bool OsgVerseViewer::createPbrScene(const std::string& modelPath) {
    std::string file = modelPath;
    if (file.empty()) file = std::string(OSGVERSE_SDK_ROOT) + "/models/Sponza.osgb";

    osg::ref_ptr<osg::Node> scene = osgDB::readNodeFile(file);
    if (!scene.valid()) {
        fprintf(stderr, "[osgverse] failed to read scene model: %s\n", file.c_str());
        return false;
    }

    // 必须先准备好参数：它会加载 skybox 贴图、编译管线 shader，
    // 后面的 SkyBox / 光源 / 管线都依赖它（尤其是 skyboxMap 作为 IBL 环境贴图）。
    ensurePbrParams();

    // 管线必须先于 SkyBox 创建（SkyBox 构造时要拿管线做 shader 定义）
    _pipeline = new osgVerse::Pipeline;

    _pbrRoot = new osg::Group;
    _pbrRoot->setName("PbrRoot");

    osg::ref_ptr<osg::MatrixTransform> sceneRoot = new osg::MatrixTransform;
    sceneRoot->setName("PbrSceneRoot");
    sceneRoot->addChild(scene.get());
    // 让场景既进 G-Buffer 又参与阴影投射
    osgVerse::Pipeline::setPipelineMask(*sceneRoot, DEFERRED_SCENE_MASK | SHADOW_CASTER_MASK);
    _pbrRoot->addChild(sceneRoot.get());

    // 播放模型自带的默认动画
    std::vector<osgVerse::NodeAnimationPair> animList = osgVerse::obtainAnimations(scene.get());
    for (size_t i = 0; i < animList.size(); ++i) {
        animList[i].second->play("", osgVerse::TweenAnimation::Looping);
    }

    // 主平行光
    _mainLight = new osgVerse::LightDrawable;
    _mainLight->setColor(osg::Vec3(1.5f, 1.5f, 1.2f));
    _mainLight->setDirection(osg::Vec3(0.02f, 0.1f, -1.0f));
    _mainLight->setDirectional(true);
    osg::ref_ptr<osg::Geode> lightGeode = new osg::Geode;
    lightGeode->addDrawable(_mainLight.get());
    _pbrRoot->addChild(lightGeode.get());

    // 天空盒：由独立的 post camera 在延迟阶段之后绘制
    _pbrPostCamera = osgVerse::SkyBox::createSkyCamera();
    osgVerse::Pipeline::setPipelineMask(*_pbrPostCamera, FORWARD_SCENE_MASK);
    _pbrRoot->addChild(_pbrPostCamera.get());

    _pbrSkybox = new osgVerse::SkyBox(_pipeline.get());
    _pbrSkybox->setSkyShaders(
        osgDB::readShaderFile(osg::Shader::VERTEX, SHADER_DIR + "skybox.vert.glsl"),
        osgDB::readShaderFile(osg::Shader::FRAGMENT, SHADER_DIR + "skybox.frag.glsl"));
    _pbrSkybox->setEnvironmentMap(_pbrParams.skyboxMap.get(), false);
    _pbrPostCamera->addChild(_pbrSkybox.get());

    // 交给 viewer：Renderer 由 pipeline 提供
    _viewer->pipeline = _pipeline;

    _content->removeChildren(0, _content->getNumChildren());
    _content->addChild(_pbrRoot.get());
    _mode = MODE_PBR;

    // 轨迹球漫游
    _viewer->setCameraManipulator(new osgGA::TrackballManipulator);

    // 参数里的 originWidth/Height 决定管线内部缓冲尺寸与主相机视口，
    // 因此必须用当前窗口尺寸；setupStandardPipeline 也必须在 view 设置好之后调用。
    setupPbrPipeline(_width, _height);

    _viewer->home();
    return true;
}

void OsgVerseViewer::ensurePbrParams() {
    if (_pbrParamsReady) return;
    _pbrParamsReady = true;

    // 构造函数会加载 skybox 贴图（以及 .ibl.rseq 环境光数据）、编译标准管线的所有 shader
    _pbrParams = osgVerse::StandardPipelineParameters(SHADER_DIR, SKYBOX_DIR + "barcelona.hdr");
    _pbrParams.enablePostEffects = true;
    _pbrParams.enableAO = true;
    _pbrParams.enableUserInput = true;
    // 用 Processing 的 GL 上下文去探测 GL/GLSL 版本，避免 osgVerse 自己另开一个窗口
    _pbrParams.withEmbeddedViewer = true;
    _pbrParams.addUserInputStage("Forward", CUSTOM_INPUT_MASK,
                                 osgVerse::StandardPipelineParameters::BEFORE_FINAL_STAGE);
}

void OsgVerseViewer::setupPbrPipeline(int width, int height) {
    // originWidth/Height 决定管线内部缓冲尺寸与主相机视口，必须用当前窗口尺寸；
    // setupStandardPipeline 也必须在 view 设置好之后调用。
    _pbrParams.originWidth = static_cast<unsigned int>(width);
    _pbrParams.originHeight = static_cast<unsigned int>(height);

    osgVerse::setupStandardPipeline(_pipeline.get(), _viewer.get(), _pbrParams);
    wirePbrModules();
}

void OsgVerseViewer::wirePbrModules() {
    // 开启 TAA 时天空盒由延迟 pass 负责绘制，不再用独立的 post camera
    bool deferredSky = false;
    if (_pipeline->getDeferredCallback()) {
        deferredSky = _pipeline->getDeferredCallback()->isJitterEnabled();
    }
    if (_pbrPostCamera.valid() && _pbrSkybox.valid()) {
        if (deferredSky) _pbrPostCamera->removeChild(_pbrSkybox.get());
        else if (_pbrSkybox->getNumParents() == 0) _pbrPostCamera->addChild(_pbrSkybox.get());
    }

    // 阴影模块的视锥网格需要作为普通场景内容显示
    osgVerse::ShadowModule* shadow =
        static_cast<osgVerse::ShadowModule*>(_pipeline->getModule("Shadow"));
    if (shadow && shadow->getFrustumGeode()) {
        osg::Node* frustum = shadow->getFrustumGeode();
        osgVerse::Pipeline::setPipelineMask(*frustum, FORWARD_SCENE_MASK);
        if (_shadowFrustum.valid() && _shadowFrustum.get() != frustum
            && _shadowFrustum->getNumParents() > 0) {
            _shadowFrustum->getParent(0)->removeChild(_shadowFrustum.get());
        }
        if (frustum->getNumParents() == 0 && _pbrRoot.valid()) {
            _pbrRoot->addChild(frustum);
        }
        _shadowFrustum = frustum;
    }

    osgVerse::LightModule* light =
        static_cast<osgVerse::LightModule*>(_pipeline->getModule("Light"));
    if (light && _mainLight.valid()) light->setMainLight(_mainLight.get(), "Shadow");
}

// ------------------------------------------------------------
// 数字地球场景（TMS 瓦片 + EarthManipulator）
// ------------------------------------------------------------
bool OsgVerseViewer::createEarthScene(const std::string& orthoUrl,
                                      const std::string& startFile) {
    const std::string ortho = orthoUrl.empty() ? kDefaultOrthoUrl : orthoUrl;
    const std::string root = startFile.empty() ? "0-0-0.verse_tms" : startFile;

    // 高程/影像约定：与 osgVerse earth_test 的默认值一致
    const std::string earthUrls = "Orthophoto=" + ortho +
                                  " OriginBottomLeft=0 UseWebMercator=1 UseEarth3D=1";
    osg::ref_ptr<osgDB::Options> options = new osgDB::Options(earthUrls);

    osg::ref_ptr<osg::Node> earth = osgDB::readNodeFile(root, options.get());
    if (!earth.valid()) {
        fprintf(stderr, "[osgverse] failed to create earth from TMS: %s\n", earthUrls.c_str());
        return false;
    }
    _worldNode = earth;

    // GLCORE 下需要给地球瓦片挂一个最简单的“贴图”着色器
    osg::ref_ptr<osg::MatrixTransform> earthRoot = new osg::MatrixTransform;
    earthRoot->setName("EarthRoot");
    earthRoot->getOrCreateStateSet()->setAttribute(osgVerse::createDefaultProgram("BaseTexture"));
    earthRoot->getOrCreateStateSet()->addUniform(new osg::Uniform("BaseTexture", (int)0));
    earthRoot->addChild(earth.get());

    _content->removeChildren(0, _content->getNumChildren());
    _content->addChild(earthRoot.get());
    _mode = MODE_EARTH;

    // 地球半径以百万米计，默认 near/far 会把它裁掉
    _viewer->getCamera()->setNearFarRatio(0.00001);

    osg::ref_ptr<osgVerse::EarthManipulator> manipulator = new osgVerse::EarthManipulator;
    manipulator->setWorldNode(earth.get());
    _viewer->setCameraManipulator(manipulator.get());

    return true;
}

void OsgVerseViewer::setEarthViewpoint(double longitude, double latitude, double height) {
    if (_mode != MODE_EARTH) return;
    osgVerse::EarthManipulator* manipulator =
        dynamic_cast<osgVerse::EarthManipulator*>(_viewer->getCameraManipulator());
    if (!manipulator) return;
    // setByEye 的参数顺序是 (latitude, longitude, height)
    manipulator->setByEye(latitude, longitude, height);
}

// ------------------------------------------------------------
// 通用
// ------------------------------------------------------------
void OsgVerseViewer::resize(int width, int height) {
    if (width == _width && height == _height) return;
    _width = width;
    _height = height;

    if (_gw.valid()) {
        _gw->getEventQueue()->windowResize(0, 0, width, height);
    }

    if (_mode == MODE_PBR && _pipeline.valid()) {
        // 延迟管线的内部缓冲尺寸在 setup 时就固定了，尺寸变化只能重建 stage
        _pipeline->clearStagesFromView(_viewer.get(), _viewer->getCamera());
        setupPbrPipeline(width, height);
        return;
    }

    if (_viewer && _viewer->getCamera()) {
        _viewer->getCamera()->setViewport(0, 0, width, height);
        double aspect = (height > 0) ? static_cast<double>(width) / height : 1.0;
        _viewer->getCamera()->setProjectionMatrixAsPerspective(30.0, aspect, 1.0, 10000.0);
    }
}

void OsgVerseViewer::frame() {
    if (!_viewer) return;

    // 注意：延迟管线会把主相机的 GraphicsContext 置空（真正的绘制由各 stage 相机
    // 完成），所以这里必须用我们自己的嵌入窗口，而不是 camera->getGraphicsContext()。
    osg::GraphicsContext* gc = _gw.valid() ? static_cast<osg::GraphicsContext*>(_gw.get()) : nullptr;
    osg::State* state = gc ? gc->getState() : nullptr;

    // OSG 与 Processing 共用同一个 GL 上下文，两边的 VAO / VBO / EBO 绑定要互相隔离：
    //   1) 进入时保存 Processing 的绑定并解绑，让 OSG 从干净状态开始；
    //   2) OSG 内部对 VAO / Buffer 绑定有缓存（State::_currentVAO、
    //      VertexArrayState::_currentVBO/_currentEBO），我们改了真实绑定后必须把
    //      缓存作废，否则下一帧 OSG 会“以为还绑着”而跳过绑定，在 core profile 下
    //      用无效状态绘制（模型画不出来 + 每帧 GL_INVALID_OPERATION）；
    //   3) 退出时恢复 Processing 的绑定，避免它把 EBO 绑到 OSG 的 VAO 上把索引缓冲改坏。
    GLint prevVAO = 0, prevArrayBuf = 0, prevElemBuf = 0, prevProgram = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVAO);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prevArrayBuf);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &prevElemBuf);
    glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);

    glUseProgram(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    if (state) {
        // state->reset() 会打印大量 notice，这里临时只保留警告以上级别
        osg::NotifySeverity saved = osg::getNotifyLevel();
        osg::setNotifyLevel(osg::WARN);
        state->reset();
        osg::setNotifyLevel(saved);
    }

    _viewer->frame();

    if (state) {
        state->setCurrentVertexArrayObject(0);
        if (osg::VertexArrayState* vas = state->getCurrentVertexArrayState()) {
            vas->resetBufferObjectPointers();
        }
        state->setCurrentToGlobalVertexArrayState();
    }

    glUseProgram(prevProgram);
    glBindVertexArray(prevVAO);
    glBindBuffer(GL_ARRAY_BUFFER, prevArrayBuf);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, prevElemBuf);
}

void OsgVerseViewer::home() {
    if (_viewer) _viewer->home();
}

bool OsgVerseViewer::saveScreenshot(const std::string& path) {
    if (!_viewer) return false;
    osg::GraphicsContext* gc = _gw.valid() ? static_cast<osg::GraphicsContext*>(_gw.get()) : nullptr;
    if (!gc) {
        fprintf(stderr, "[osgverse] saveScreenshot: no graphics context\n");
        return false;
    }

    int w = _width, h = _height;
    if (w <= 0 || h <= 0) {
        fprintf(stderr, "[osgverse] saveScreenshot: bad size %dx%d\n", w, h);
        return false;
    }

    std::vector<unsigned char> pixels(static_cast<size_t>(w) * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    // Processing 绘制几何时会绑定自己的 FBO，直接读 GL_BACK 会得到黑图，
    // 这里显式切到默认读帧缓冲，读完再还原。
    GLint prevReadFbo = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevReadFbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(prevReadFbo));

    FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp) {
        fprintf(stderr, "[osgverse] cannot open %s for writing\n", path.c_str());
        return false;
    }
    std::fprintf(fp, "P6\n%d %d\n255\n", w, h);
    for (int y = h - 1; y >= 0; --y) {  // OpenGL 原点在左下，PPM 在左上
        std::fwrite(pixels.data() + static_cast<size_t>(y) * w * 3, 1, w * 3, fp);
    }
    std::fclose(fp);
    return true;
}

// ------------------------------------------------------------
// 输入事件转发
// ------------------------------------------------------------
void OsgVerseViewer::mouseMove(int x, int y) {
    if (_gw.valid()) _gw->getEventQueue()->mouseMotion((float)x, (float)y);
}

void OsgVerseViewer::mouseButtonPress(int x, int y, int button) {
    if (_gw.valid()) {
        _gw->getEventQueue()->mouseButtonPress((float)x, (float)y, (unsigned int)button);
    }
}

void OsgVerseViewer::mouseButtonRelease(int x, int y, int button) {
    if (_gw.valid()) {
        _gw->getEventQueue()->mouseButtonRelease((float)x, (float)y, (unsigned int)button);
    }
}

void OsgVerseViewer::mouseScroll(int updown, int x, int y) {
    if (!_gw.valid()) return;
    _gw->getEventQueue()->mouseMotion((float)x, (float)y);
    _gw->getEventQueue()->mouseScroll(
        updown > 0 ? osgGA::GUIEventAdapter::SCROLL_UP : osgGA::GUIEventAdapter::SCROLL_DOWN);
}

void OsgVerseViewer::keyPress(int key) {
    if (_gw.valid()) _gw->getEventQueue()->keyPress(key);
}

void OsgVerseViewer::keyRelease(int key) {
    if (_gw.valid()) _gw->getEventQueue()->keyRelease(key);
}

}  // namespace osgverselib
