#include "osg_viewer.h"

#include <osg/Group>
#include <osg/Geode>
#include <osg/ShapeDrawable>
#include <osg/MatrixTransform>
#include <osg/AnimationPath>
#include <osg/Drawable>
#include <osg/Geometry>
#include <osg/LightSource>
#include <osg/Material>
#include <osg/Program>
#include <osg/Shader>
#include <osg/StateSet>
#include <osg/Camera>
#include <osgDB/ReadFile>
#include <osgDB/Registry>
#include <osgGA/TrackballManipulator>
#include <osgGA/EventQueue>
#include <osgGA/GUIEventAdapter>
#include <osgUtil/Optimizer>
#include <osgViewer/Viewer>
#include <osgViewer/GraphicsWindow>
#include <osg/GL>
#include <osg/GraphicsContext>
#include <osg/State>
#include <osg/Notify>

#include <cmath>
#include <cstdio>
#include <vector>

namespace osglib {

namespace {

// 构造一个不依赖外部资源的演示场景（GLCORE 安全，不使用固定管线光照）：
//   - 一个会自转的蓝色球体
//   - 一个绕球公转的橙色盒子
//   - 一块灰色地面
// 颜色直接作用于顶点，不依赖 glLight / glMaterial。
osg::Group* createDemoScene() {
    auto* root = new osg::Group;

    // 中心球体
    auto* sphere = new osg::Sphere(osg::Vec3(0, 0, 0), 1.0f);
    auto* sd = new osg::ShapeDrawable(sphere);
    sd->setColor(osg::Vec4(0.2f, 0.5f, 0.9f, 1.0f));
    auto* sphereGeode = new osg::Geode;
    sphereGeode->addDrawable(sd);

    auto* earthXform = new osg::MatrixTransform;
    earthXform->addChild(sphereGeode);
    earthXform->setUpdateCallback(new osg::AnimationPathCallback(
        osg::Vec3(0, 0, 0), osg::Z_AXIS, osg::inDegrees(30.0)));
    root->addChild(earthXform);

    // 卫星盒子（绕 Z 轴公转）
    auto* box = new osg::Box(osg::Vec3(2.2f, 0, 0), 0.5f);
    auto* sd2 = new osg::ShapeDrawable(box);
    sd2->setColor(osg::Vec4(1.0f, 0.6f, 0.2f, 1.0f));
    auto* boxGeode = new osg::Geode;
    boxGeode->addDrawable(sd2);

    auto* satXform = new osg::MatrixTransform;
    satXform->addChild(boxGeode);
    satXform->setUpdateCallback(new osg::AnimationPathCallback(
        osg::Vec3(0, 0, 0), osg::Z_AXIS, osg::inDegrees(90.0)));
    root->addChild(satXform);

    // 地面（扁平盒子）
    auto* ground = new osg::Box(osg::Vec3(0, 0, -1.5f), 10.0f, 10.0f, 0.1f);
    auto* sd3 = new osg::ShapeDrawable(ground);
    sd3->setColor(osg::Vec4(0.4f, 0.4f, 0.45f, 1.0f));
    auto* groundGeode = new osg::Geode;
    groundGeode->addDrawable(sd3);
    root->addChild(groundGeode);

    osgUtil::Optimizer opt;
    opt.optimize(root);
    return root;
}

// 这个 OSG SDK 是 GLCORE 构建：Light / Material / ShadeModel / LightModel / TexGen
// 这些固定管线属性都是空实现（apply() 直接打印 "not supported"）。带这些属性的模型
// 直接渲染会变黑，而且 GL_LIGHTn / GL_TEXTURE_GEN_* 在 core profile 下是非法枚举，
// 每帧都会刷 GL 错误。这里把载入的模型规整成“纯顶点色”形态：
//   1. 清掉所有节点/几何体 StateSet 上的光照与纹理生成模式及属性；
//   2. 给没有顶点色的 Geometry 逐顶点补上颜色（取原材质的漫反射色，缺省浅灰）。
//
// 注意：这里用递归而不是 osg::NodeVisitor。NodeVisitor 的重载是静态分派的，
// 非 Geode / Group 的节点（如 osg::LightSource、MatrixTransform）会被派发到
// NodeVisitor 基类的对应重载上，从而绕过我们的处理；用 asGroup()/asGeode()
// 递归则可以覆盖所有节点类型。
class ModelFixer {
public:
    static void fix(osg::Node& node) {
        // GLCORE 下没有固定管线光照，LightSource 只会每帧触发
        // "Light::apply() - not supported" 和 GL_INVALID_ENUM。
        // 清空它携带的 Light 即可（同时也会清掉它 StateSet 里的 GL_LIGHTn 模式）。
        if (auto* lightSource = dynamic_cast<osg::LightSource*>(&node)) {
            lightSource->setLight(nullptr);
        }

        if (node.getStateSet()) fixStateSet(node.getStateSet());

        if (osg::Geode* geode = node.asGeode()) {
            osg::Vec4 color = pickColor(node.getStateSet());
            for (unsigned int i = 0; i < geode->getNumDrawables(); ++i) {
                osg::Drawable* drawable = geode->getDrawable(i);
                osg::Vec4 c = color;
                if (drawable->getStateSet()) {
                    c = pickColor(drawable->getStateSet());
                    fixStateSet(drawable->getStateSet());
                }
                if (auto* geom = dynamic_cast<osg::Geometry*>(drawable)) {
                    addVertexColors(geom, c);
                }
            }
        }

        if (osg::Group* group = node.asGroup()) {
            for (unsigned int i = 0; i < group->getNumChildren(); ++i) {
                if (group->getChild(i)) fix(*group->getChild(i));
            }
        }
    }

private:
    static osg::Vec4 fallbackColor() { return osg::Vec4(0.75f, 0.78f, 0.82f, 1.0f); }

    static osg::Vec4 pickColor(osg::StateSet* ss) {
        if (ss) {
            auto* mat = dynamic_cast<osg::Material*>(
                ss->getAttribute(osg::StateAttribute::MATERIAL));
            if (mat) {
                osg::Vec4 c = mat->getDiffuse(osg::Material::FRONT_AND_BACK);
                if (c.r() + c.g() + c.b() > 0.05f) return c;
            }
        }
        return fallbackColor();
    }

    static bool isFixedFunction(osg::StateAttribute::Type t) {
        return t == osg::StateAttribute::MATERIAL ||
               t == osg::StateAttribute::SHADEMODEL ||
               t == osg::StateAttribute::LIGHTMODEL ||
               t == osg::StateAttribute::LIGHT ||
               t == osg::StateAttribute::TEXGEN;
    }

    static void fixStateSet(osg::StateSet* ss) {
        ss->removeMode(GL_LIGHTING);
        for (int i = 0; i < 8; ++i) ss->removeMode(GL_LIGHT0 + i);

        // core profile 移除了这些固定管线 GLMode，GL_TEXTURE_GEN_* / GL_TEXTURE_2D
        // 之类在 GLCORE 的 osg/GL.h 里已经没有定义，这里直接用它们的枚举值：
        //   0x0DE0 GL_TEXTURE_1D, 0x0DE1 GL_TEXTURE_2D   —— 固定管线纹理开关
        //   0x0C60..0x0C63 GL_TEXTURE_GEN_S/T/R/Q        —— 纹理坐标自动生成
        static const GLenum kLegacyModes[6] = {0x0DE0, 0x0DE1, 0x0C60, 0x0C61, 0x0C62, 0x0C63};

        // 纹理模式与纹理属性：osg::TexGen 在 OSG 里被当作纹理属性，
        // 既不出现在 getAttributeList()，也不在 getModeList() 里，只能单独清。
        for (unsigned int unit = 0; unit < ss->getTextureModeList().size(); ++unit) {
            std::vector<osg::StateAttribute::GLMode> modes;
            for (const auto& kv : ss->getTextureModeList()[unit]) modes.push_back(kv.first);
            for (osg::StateAttribute::GLMode m : modes) ss->removeTextureMode(unit, m);
        }
        for (unsigned int unit = 0; unit < ss->getTextureAttributeList().size(); ++unit) {
            ss->removeTextureAttribute(unit, osg::StateAttribute::TEXGEN);
        }
        for (GLenum mode : kLegacyModes) ss->removeMode(mode);

        // 普通属性：光照 / 材质 / 着色模型
        std::vector<osg::StateAttribute::TypeMemberPair> toRemove;
        for (const auto& kv : ss->getAttributeList()) {
            if (isFixedFunction(kv.first.first)) toRemove.push_back(kv.first);
        }
        for (const auto& k : toRemove) {
            ss->removeAttribute(k.first, k.second);
        }
    }

    static void addVertexColors(osg::Geometry* geom, const osg::Vec4& c) {
        // 自定义着色器只用顶点位置和顶点色，去掉法线/纹理坐标可减少无谓的属性绑定。
        geom->setNormalArray(nullptr);
        for (unsigned int u = 0; u < 8; ++u) geom->setTexCoordArray(u, nullptr);
        // 标记为静态，让 OSG 走 VBO 路径（core profile 不允许客户端顶点数组）
        geom->setDataVariance(osg::Object::STATIC);
        geom->setUseDisplayList(false);
        geom->setUseVertexBufferObjects(true);

        if (geom->getColorArray() && geom->getColorArray()->getNumElements() > 0) return;
        if (!geom->getVertexArray()) return;
        unsigned int n = geom->getVertexArray()->getNumElements();
        if (n == 0) return;
        osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array(n);
        for (unsigned int i = 0; i < n; ++i) (*colors)[i] = c;
        geom->setColorArray(colors.get(), osg::Array::BIND_PER_VERTEX);
    }
};

// 给模型指定一个最小化的“仅顶点色”着色器。
// OSG 自带的 GL3 默认着色器会声明并启用 osg_MultiTexCoord0 属性且采样 baseTexture，
// 而很多模型（如 cessna.osg）既没有纹理也没有纹理坐标，在 core profile 下会因
// “启用了但没有数据的顶点属性”触发 GL_INVALID_OPERATION，导致整个模型画不出来。
// 这个自定义着色器只声明 osg_Vertex 和 osg_Color，彻底绕开该问题。
osg::Program* createFlatColorProgram() {
    static const char* vert = R"(
#version 330 core
in vec4 osg_Vertex;
in vec4 osg_Color;
uniform mat4 osg_ModelViewProjectionMatrix;
out vec4 vColor;
void main() {
    vColor = osg_Color;
    gl_Position = osg_ModelViewProjectionMatrix * osg_Vertex;
}
)";
    static const char* frag = R"(
#version 330 core
in vec4 vColor;
out vec4 fragColor;
void main() {
    fragColor = vColor;
}
)";
    auto* program = new osg::Program;
    program->addShader(new osg::Shader(osg::Shader::VERTEX, vert));
    program->addShader(new osg::Shader(osg::Shader::FRAGMENT, frag));
    return program;
}

}  // namespace

OsgViewer::OsgViewer(int width, int height)
    : _width(width), _height(height) {
    // 让 osgDB 能找到模型插件（osgdb_osg.so 等）。
    // OSG 会在每个搜索路径下找 "osgPlugins-<版本>/osgdb_<扩展名>.so"，
    // 因此这里加入 OSG SDK 的 lib 目录（构建时由 CMake 写入）。
#ifdef OSGLIB_OSG_LIB_DIR
    osgDB::Registry::instance()->getLibraryFilePathList().push_back(OSGLIB_OSG_LIB_DIR);
#endif

    _viewer = new osgViewer::Viewer;

    // 关键：用 GraphicsWindowEmbedded 接入外部 GL 上下文。
    // 它不调用 makeCurrent / swapBuffers，假设外部（Processing）始终持有当前上下文。
    _gw = _viewer->setUpViewerAsEmbeddedInWindow(0, 0, width, height);

    // 初始化事件队列的窗口矩形 / 鼠标输入范围（Processing 坐标：左上角原点，Y 向下），
    // 这样后续把 Processing 的鼠标键盘事件推进去后，OSG 漫游器就能正确响应。
    if (_gw.valid()) {
        _gw->getEventQueue()->windowResize(0, 0, width, height);
    }

    // 清屏颜色（背景）。如果想让 Processing 的 background() 生效，
    // 可把 setClearMask 设为只清深度缓冲 GL_DEPTH_BUFFER_BIT。
    _viewer->getCamera()->setClearColor(osg::Vec4(0.12f, 0.12f, 0.16f, 1.0f));

    // 单线程（外部线程调用 frame）
    _viewer->setThreadingModel(osgViewer::Viewer::SingleThreaded);

    // 场景结构：_root（场景根）→ _content（内容）
    // 载入模型时只替换 _content 的子节点。
    _root = new osg::Group;
    _content = new osg::Group;
    _root->addChild(_content);
    _content->addChild(createDemoScene());
    _viewer->setSceneData(_root);

    // 轨迹球相机控制器
    _viewer->setCameraManipulator(new osgGA::TrackballManipulator);

    _viewer->home();
}

OsgViewer::~OsgViewer() {
    _viewer = nullptr;
    _gw = nullptr;
    _content = nullptr;
    _root = nullptr;
}

void OsgViewer::resize(int width, int height) {
    // 尺寸没变就不产生 RESIZE 事件，避免每帧往事件队列里塞无用的 resize。
    if (width == _width && height == _height) {
        return;
    }
    _width = width;
    _height = height;
    if (_gw.valid()) {
        _gw->getEventQueue()->windowResize(0, 0, width, height);
    }
    if (_viewer && _viewer->getCamera()) {
        _viewer->getCamera()->setViewport(0, 0, width, height);
        // 保持透视投影的宽高比
        double aspect = (height > 0) ? static_cast<double>(width) / height : 1.0;
        _viewer->getCamera()->setProjectionMatrixAsPerspective(30.0, aspect, 1.0, 10000.0);
    }
}

void OsgViewer::frame() {
    if (!_viewer) return;

    osg::GraphicsContext* gc = _viewer->getCamera()->getGraphicsContext();
    osg::State* state = gc ? gc->getState() : nullptr;

    // OSG 与 Processing 共用同一个 GL 上下文，必须小心处理 VAO / Buffer 绑定：
    //
    //   1) OSG 渲染完会留下它自己的 VAO 绑定；Processing 之后绘制 rect()/text() 时
    //      会把自己缓存的 EBO 绑到"当前 VAO"（其实是 OSG 的 VAO）上，把 OSG 的
    //      VAO 索引缓冲改坏，下一帧 OSG 就越界崩溃（SIGSEGV）。
    //   2) 反过来，Processing 渲染时也会改掉 VAO / ARRAY_BUFFER / ELEMENT_ARRAY_BUFFER
    //      绑定，而 OSG 内部对这些绑定有缓存（State::_currentVAO、
    //      VertexArrayState::_currentVBO/_currentEBO）。缓存一旦和真实 GL 状态不一致，
    //      OSG 就会"以为还绑着"而跳过绑定，导致 core profile 下用无效状态绘制
    //      （GL_INVALID_OPERATION，模型画不出来）。
    //
    // 处理方式：进入时保存 Processing 的绑定并解绑 → OSG 渲染 → 恢复绑定，
    // 同时把 OSG 的相关缓存置为"未知"，迫使它下一帧重新绑定。
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
        // 重置 OSG 的状态缓存，强制它重新应用所有 GL 状态。
        // reset() 内部会在 NOTICE 级别打印一行日志，这里临时提高通知级别避免每帧刷屏。
        osg::NotifySeverity saved = osg::getNotifyLevel();
        osg::setNotifyLevel(osg::WARN);
        state->reset();
        osg::setNotifyLevel(saved);
    }

    _viewer->frame();

    if (state) {
        // 我们刚刚动了 VAO / Buffer 绑定，OSG 的缓存已失效，必须显式作废。
        state->setCurrentVertexArrayObject(0);
        if (osg::VertexArrayState* vas = state->getCurrentVertexArrayState()) {
            vas->resetBufferObjectPointers();
        }
        state->setCurrentToGlobalVertexArrayState();
    }

    // 恢复 Processing 的绑定，保证它的状态缓存与真实 GL 状态一致。
    glUseProgram(prevProgram);
    glBindVertexArray(prevVAO);
    glBindBuffer(GL_ARRAY_BUFFER, prevArrayBuf);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, prevElemBuf);
}

bool OsgViewer::loadModel(const std::string& path) {
    osg::ref_ptr<osg::Node> node = osgDB::readNodeFile(path);
    if (!node) {
        return false;
    }

    // 规整为 GLCORE 可渲染的形态（去掉固定管线光照/材质/纹理生成，补顶点色）
    ModelFixer::fix(*node);

    // 用最小化着色器，避免 OSG 默认着色器对纹理/纹理坐标属性的要求
    node->getOrCreateStateSet()->setAttribute(createFlatColorProgram());

    // 合并/索引化几何体，并把非三角形图元（QUADS / QUAD_STRIP / POLYGON）三角化：
    //   - 老模型（如 cessna.osg）常有上千个独立 TRIANGLE_STRIP，索引合并后只剩一次绘制；
    //   - GL_QUADS / GL_POLYGON 在 core profile 下是非法枚举（如 morphing.osg），
    //     不三角化就整块画不出来。
    osgUtil::Optimizer optimizer;
    optimizer.optimize(node.get(), osgUtil::Optimizer::INDEX_MESH |
                                   osgUtil::Optimizer::MERGE_GEOMETRY |
                                   osgUtil::Optimizer::TESSELLATE_GEOMETRY);

    // 只替换 _content 的内容
    _content->removeChildren(0, _content->getNumChildren());
    _content->addChild(node);

    _viewer->home();
    return true;
}

// ------------------------------------------------------------
// 输入事件转发：把 Processing 的鼠标/键盘事件推进 OSG 的事件队列，
// OSG 的 Viewer 会在 frame() 时消费它们并分发给 CameraManipulator。
// ------------------------------------------------------------

void OsgViewer::mouseMove(int x, int y) {
    if (_gw.valid()) {
        _gw->getEventQueue()->mouseMotion(static_cast<float>(x), static_cast<float>(y));
    }
}

void OsgViewer::mouseButtonPress(int x, int y, int button) {
    if (_gw.valid()) {
        _gw->getEventQueue()->mouseButtonPress(
            static_cast<float>(x), static_cast<float>(y), static_cast<unsigned int>(button));
    }
}

void OsgViewer::mouseButtonRelease(int x, int y, int button) {
    if (_gw.valid()) {
        _gw->getEventQueue()->mouseButtonRelease(
            static_cast<float>(x), static_cast<float>(y), static_cast<unsigned int>(button));
    }
}

void OsgViewer::mouseScroll(int updown, int x, int y) {
    if (!_gw.valid()) return;
    // 滚轮事件不带坐标，先更新一下当前位置，再投递滚动事件
    _gw->getEventQueue()->mouseMotion(static_cast<float>(x), static_cast<float>(y));
    _gw->getEventQueue()->mouseScroll(
        updown > 0 ? osgGA::GUIEventAdapter::SCROLL_UP : osgGA::GUIEventAdapter::SCROLL_DOWN);
}

void OsgViewer::keyPress(int key) {
    if (_gw.valid()) {
        _gw->getEventQueue()->keyPress(key);
    }
}

void OsgViewer::keyRelease(int key) {
    if (_gw.valid()) {
        _gw->getEventQueue()->keyRelease(key);
    }
}

void OsgViewer::home() {
    if (_viewer) {
        _viewer->home();
    }
}

void OsgViewer::setViewpoint(double x, double y, double z,
                             double h, double p, double r) {
    if (!_viewer) return;
    osg::Vec3d eye(x, y, z);
    osg::Vec3d center(0, 0, 0);
    osg::Vec3d up(0, 0, 1);
    // 用 heading/pitch/roll 构造一个简单的 view matrix
    double hr = osg::inDegrees(h);
    double pr = osg::inDegrees(p);
    // 简化：把 hpr 转成前方向
    osg::Vec3d forward(
        std::cos(pr) * std::cos(hr),
        std::cos(pr) * std::sin(hr),
        std::sin(pr));
    center = eye + forward;
    _viewer->getCamera()->setViewMatrixAsLookAt(eye, center, up);
}

bool OsgViewer::saveScreenshot(const std::string& path) {
    if (!_viewer) return false;
    osg::GraphicsContext* gc = _viewer->getCamera()->getGraphicsContext();
    if (!gc) return false;
    osg::State* state = gc->getState();
    if (!state) return false;

    int w = _width;
    int h = _height;
    if (w <= 0 || h <= 0) return false;

    std::vector<unsigned char> pixels(static_cast<size_t>(w) * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    // Processing 在绘制几何时会绑定它自己的 FBO，直接读 GL_BACK 会得到黑图。
    // 这里显式切到默认读帧缓冲的后缓冲来抓屏，读完再还原绑定。
    GLint prevReadFbo = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevReadFbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(prevReadFbo));

    FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp) return false;
    std::fprintf(fp, "P6\n%d %d\n255\n", w, h);
    // OpenGL 原点在左下，PPM 原点在左上，需要竖直翻转
    for (int y = h - 1; y >= 0; --y) {
        std::fwrite(pixels.data() + static_cast<size_t>(y) * w * 3, 1, w * 3, fp);
    }
    std::fclose(fp);
    return true;
}

}  // namespace osglib
