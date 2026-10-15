#include "gl_view.h"

#include "config.h"
#include "gl_loader.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace cpplib {

namespace {

// 顶点布局：x, y, r, g, b, a （共 6 个 float）
constexpr int kStride = 6 * sizeof(float);
constexpr int kWavePoints = 96;

// 简单 HSV->RGB（h,s,v 均 0..1）
void hsvToRgb(float h, float s, float v, float& r, float& g, float& b) {
    h = h - static_cast<float>(static_cast<int>(h));  // wrap [0,1)
    const int i = static_cast<int>(h * 6.0f);
    const float f = h * 6.0f - i;
    const float p = v * (1.0f - s);
    const float q = v * (1.0f - s * f);
    const float t = v * (1.0f - s * (1.0f - f));
    switch (i % 6) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
}

bool compileShader(gl::Api& api, unsigned int type, const char* source,
                   unsigned int& outShader, std::string& err) {
    const unsigned int sh = api.CreateShader(type);
    if (sh == 0) {
        err = "glCreateShader failed";
        return false;
    }
    const char* src = source;
    api.ShaderSource(sh, 1, &src, nullptr);
    api.CompileShader(sh);

    int status = 0;
    api.GetShaderiv(sh, gl::kCompileStatus, &status);
    if (status != 0) {
        outShader = sh;
        return true;
    }
    int logLen = 0;
    api.GetShaderiv(sh, gl::kInfoLogLength, &logLen);
    if (logLen > 0) {
        std::vector<char> log(static_cast<std::size_t>(logLen) + 1);
        api.GetShaderInfoLog(sh, logLen, nullptr, log.data());
        err.assign(log.data());
    }
    api.DeleteShader(sh);
    outShader = 0;
    return false;
}

}  // namespace

struct GLView::Impl {
    gl::Api api{};
    bool glLoaded = false;
    bool initialized = false;

    unsigned int program = 0;
    unsigned int vboTriangle = 0;
    unsigned int vboWave = 0;
    unsigned int vao = 0;

    int glslMajor = 1;
    int glslMinor = 20;
    bool modernGlsl = false;

    int errorCount = 0;
    bool loggedFirstError = false;
    int fbWidth = 0;
    int fbHeight = 0;
    int uScaleLoc = -1;
};

GLView::GLView() : d_(new Impl()) {}

GLView::~GLView() {
    if (d_->initialized && d_->glLoaded) {
        gl::Api& gl = d_->api;
        if (d_->program) {
            gl.DeleteProgram(d_->program);
        }
        if (d_->vboTriangle) {
            if (gl.DeleteBuffers) {
                gl.DeleteBuffers(1, &d_->vboTriangle);
            }
        }
        if (d_->vboWave) {
            if (gl.DeleteBuffers) {
                gl.DeleteBuffers(1, &d_->vboWave);
            }
        }
        if (d_->vao && gl.DeleteVertexArrays) {
            gl.DeleteVertexArrays(1, &d_->vao);
        }
    }
    delete d_;
}

void GLView::resize(int fbWidth, int fbHeight) {
    d_->fbWidth = fbWidth;
    d_->fbHeight = fbHeight;
}

void GLView::render(int fbWidth, int fbHeight, float t) {
    gl::Api& gl = d_->api;

    if (!d_->glLoaded) {
        d_->glLoaded = gl::load(d_->api);
        if (!d_->glLoaded) {
            return;
        }
    }

    // ---- 首次真正初始化：查询版本 + 编译着色器 + 建 VBO ----
    if (!d_->initialized) {
        const unsigned char* ver = gl.GetString(gl::kShaderLanguageVersion);
        const std::string glslVer = ver ? reinterpret_cast<const char*>(ver) : "1.20";
        int major = 1, minor = 20;
        if (std::sscanf(glslVer.c_str(), "%d.%d", &major, &minor) != 2) {
            major = 1;
            minor = 20;
        }
        d_->glslMajor = major;
        d_->glslMinor = minor;

        // Processing 4 在桌面平台通常给出 GLSL 1.20(兼容GL2) 或 3.30+ (core)。
        // 我们据此选择 legacy / modern 两套着色器源码。
        const int v = major * 100 + minor;
        d_->modernGlsl = v >= 150;
        int glslPick = 120;
        if (d_->modernGlsl) {
            // 取 <= 当前 GLSL 且属于下面集合的最大版本
            const int choices[] = {460, 450, 440, 430, 420, 410, 400, 330, 150};
            for (int c : choices) {
                if (v >= c) {
                    glslPick = c;
                    break;
                }
            }
        }
        const std::string versionLine = "#version " + std::to_string(glslPick) + "\n";

        std::string vertSrc = versionLine;
        std::string fragSrc = versionLine;
        if (d_->modernGlsl) {
            vertSrc +=
                "uniform float u_scale;\n"
                "in vec2 position;\n"
                "in vec4 color;\n"
                "out vec4 vColor;\n"
                "void main() {\n"
                "  vColor = color;\n"
                "  gl_Position = vec4(position * u_scale, 0.0, 1.0);\n"
                "}\n";
            fragSrc +=
                "in vec4 vColor;\n"
                "out vec4 fragColor;\n"
                "void main() { fragColor = vColor; }\n";
        } else {
            vertSrc +=
                "uniform float u_scale;\n"
                "attribute vec2 position;\n"
                "attribute vec4 color;\n"
                "varying vec4 vColor;\n"
                "void main() {\n"
                "  vColor = color;\n"
                "  gl_Position = vec4(position * u_scale, 0.0, 1.0);\n"
                "}\n";
            fragSrc +=
                "varying vec4 vColor;\n"
                "void main() { gl_FragColor = vColor; }\n";
        }

        unsigned int vs = 0, fs = 0;
        std::string err;
        if (!compileShader(gl, gl::kVertexShader, vertSrc.c_str(), vs, err)) {
            std::fprintf(stderr, "cpplib[gl]: vertex shader error: %s\n", err.c_str());
            return;
        }
        if (!compileShader(gl, gl::kFragmentShader, fragSrc.c_str(), fs, err)) {
            std::fprintf(stderr, "cpplib[gl]: fragment shader error: %s\n", err.c_str());
            gl.DeleteShader(vs);
            return;
        }

        d_->program = gl.CreateProgram();
        gl.AttachShader(d_->program, vs);
        gl.AttachShader(d_->program, fs);
        gl.BindAttribLocation(d_->program, 0, "position");
        gl.BindAttribLocation(d_->program, 1, "color");
        gl.LinkProgram(d_->program);

        int linkStatus = 0;
        gl.GetProgramiv(d_->program, gl::kLinkStatus, &linkStatus);
        if (linkStatus == 0) {
            int logLen = 0;
            gl.GetProgramiv(d_->program, gl::kInfoLogLength, &logLen);
            std::vector<char> log(static_cast<std::size_t>(logLen) + 1);
            gl.GetProgramInfoLog(d_->program, logLen, nullptr, log.data());
            std::fprintf(stderr, "cpplib[gl]: program link error: %s\n", log.data());
            gl.DeleteProgram(d_->program);
            d_->program = 0;
            gl.DeleteShader(vs);
            gl.DeleteShader(fs);
            return;
        }
        gl.DeleteShader(vs);
        gl.DeleteShader(fs);

        // 缓存 uniform 位置
        d_->uScaleLoc = gl.GetUniformLocation(d_->program, "u_scale");

        // VAO（core profile 需要；GLES2/GL2 没有则跳过）
        if (d_->api.vaoSupported) {
            gl.GenVertexArrays(1, &d_->vao);
            gl.BindVertexArray(d_->vao);
        }

        // 两个 VBO：三角形(动态颜色)、正弦波浪(动态顶点)
        gl.GenBuffers(1, &d_->vboTriangle);
        gl.GenBuffers(1, &d_->vboWave);
        gl.BindBuffer(gl::kArrayBuffer, d_->vboTriangle);
        gl.BufferData(gl::kArrayBuffer, 3 * kStride, nullptr, gl::kDynamicDraw);
        gl.BindBuffer(gl::kArrayBuffer, d_->vboWave);
        gl.BufferData(gl::kArrayBuffer,
                      static_cast<long>(kWavePoints) * kStride, nullptr, gl::kDynamicDraw);
        gl.BindBuffer(gl::kArrayBuffer, 0);

        d_->initialized = true;
        std::fprintf(stdout,
                     "cpplib[gl]: initialized (GLSL %s, backend %s, vao=%d)\n",
                     glslVer.c_str(), d_->modernGlsl ? "modern" : "legacy",
                     static_cast<int>(d_->vao != 0));
        std::fflush(stdout);
    }

    if (!d_->initialized) {
        return;
    }

    // ---- 每帧绘制 ----
    if (fbWidth <= 0 || fbHeight <= 0) {
        fbWidth = std::max(d_->fbWidth, 1);
        fbHeight = std::max(d_->fbHeight, 1);
    }
    d_->fbWidth = fbWidth;
    d_->fbHeight = fbHeight;

    gl.Viewport(0, 0, fbWidth, fbHeight);
    gl.Disable(gl::kDepthTest);
    gl.Enable(gl::kBlend);
    gl.BlendFunc(gl::kSrcAlpha, gl::kOneMinusSrcAlpha);

    gl.UseProgram(d_->program);

    if (d_->vao) {
        gl.BindVertexArray(d_->vao);
    }

    const float aspect = static_cast<float>(fbWidth) / static_cast<float>(fbHeight);

    // 1) 脉冲缩放的彩色三角形：顶点固定，每帧刷新颜色（演示动态 VBO + uniform）
    std::vector<float> tri(3 * 6);
    const float triPos[3][2] = {{0.0f, 0.55f}, {0.5f, -0.45f}, {-0.5f, -0.45f}};
    for (int i = 0; i < 3; ++i) {
        float hue = t * 0.12f + i * 0.333f;
        float r, g, b;
        hsvToRgb(hue, 0.75f, 1.0f, r, g, b);
        tri[i * 6 + 0] = triPos[i][0] * std::min(1.0f, aspect);
        tri[i * 6 + 1] = triPos[i][1];
        tri[i * 6 + 2] = r;
        tri[i * 6 + 3] = g;
        tri[i * 6 + 4] = b;
        tri[i * 6 + 5] = 0.85f;
    }
    gl.BindBuffer(gl::kArrayBuffer, d_->vboTriangle);
    gl.BufferSubData(gl::kArrayBuffer, 0, tri.size() * sizeof(float), tri.data());
    gl.EnableVertexAttribArray(0);
    gl.EnableVertexAttribArray(1);
    gl.VertexAttribPointer(0, 2, gl::kFloat, 0, kStride, reinterpret_cast<const void*>(0));
    gl.VertexAttribPointer(1, 4, gl::kFloat, 0, kStride,
                           reinterpret_cast<const void*>(2 * sizeof(float)));
    if (d_->uScaleLoc >= 0) {
        gl.Uniform1f(d_->uScaleLoc, 1.0f + 0.10f * std::sin(t * 2.2f));
    }
    gl.DrawArrays(gl::kTriangles, 0, 3);

    // 2) 一条随 t 上下起伏的正弦“亮带”（演示 host 端逐帧生成顶点）
    std::vector<float> wave(static_cast<std::size_t>(kWavePoints) * 6);
    for (int i = 0; i < kWavePoints; ++i) {
        const float u = static_cast<float>(i) / static_cast<float>(kWavePoints - 1);
        const float x = (u * 2.0f - 1.0f) * 0.95f * std::min(1.0f, aspect);
        const float y = -0.55f + 0.45f * std::sin(x * 6.0f + t * 2.2f) * 0.5f;
        wave[i * 6 + 0] = x;
        wave[i * 6 + 1] = y;
        wave[i * 6 + 2] = 0.10f;
        wave[i * 6 + 3] = 0.90f;
        wave[i * 6 + 4] = 1.00f;
        wave[i * 6 + 5] = 0.90f;
    }
    gl.BindBuffer(gl::kArrayBuffer, d_->vboWave);
    gl.BufferSubData(gl::kArrayBuffer, 0, wave.size() * sizeof(float), wave.data());
    gl.VertexAttribPointer(0, 2, gl::kFloat, 0, kStride, reinterpret_cast<const void*>(0));
    gl.VertexAttribPointer(1, 4, gl::kFloat, 0, kStride,
                           reinterpret_cast<const void*>(2 * sizeof(float)));
    if (d_->uScaleLoc >= 0) {
        gl.Uniform1f(d_->uScaleLoc, 1.0f);
    }
    if (gl.LineWidth) {
        gl.LineWidth(3.0f);
    }
    gl.DrawArrays(gl::kLineStrip, 0, kWavePoints);

    // 3) 把状态还原到“中立”，避免影响 Processing 接下来的绘制
    gl.DisableVertexAttribArray(0);
    gl.DisableVertexAttribArray(1);
    gl.BindBuffer(gl::kArrayBuffer, 0);
    if (d_->vao) {
        gl.BindVertexArray(0);
    }
    gl.UseProgram(0);
    gl.Disable(gl::kBlend);

    // 简单的错误统计（只打印第一次，避免刷屏）
    const unsigned int err = gl.GetError();
    if (err != gl::kNoError) {
        ++d_->errorCount;
        if (!d_->loggedFirstError) {
            d_->loggedFirstError = true;
            std::fprintf(stderr, "cpplib[gl]: GL error 0x%04X after render\n", err);
        }
    }
}

GLView* createGLView() { return new GLView(); }

void deleteGLView(GLView* view) {
    if (view) {
        delete view;
    }
}

}  // namespace cpplib
