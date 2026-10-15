#pragma once

// ============================================================
// 极简 OpenGL 动态加载器。
// ------------------------------------------------------------
// 我们不引入 GLEW/GLAD，而是在运行时从“当前已激活的 GL 上下文”
// （Processing 的 JOGL 上下文）里解析函数指针：
//   - 这样 C++ 才能和 Processing 共享同一个 OpenGL 上下文；
//   - 用到的枚举常量在下方自行定义（均为公开 GL 规范值）。
//
// 平台支持：
//   Windows : opengl32.dll (GL 1.1) + wglGetProcAddress (1.2+ / 扩展)
//   Linux   : libGL.so.1   + glXGetProcAddress
//   macOS   : OpenGL.framework (dlsym)
// ============================================================

namespace cpplib {
namespace gl {

// 用到的 GL 常量（避免依赖任何第三方头文件）
enum Const : unsigned int {
    kDepthTest = 0x0B71,
    kBlend = 0x0BE2,
    kSrcAlpha = 0x0302,
    kOneMinusSrcAlpha = 0x0303,
    kArrayBuffer = 0x8892,
    kDynamicDraw = 0x88E8,
    kStaticDraw = 0x88E4,
    kVertexShader = 0x8B31,
    kFragmentShader = 0x8B30,
    kCompileStatus = 0x8B81,
    kLinkStatus = 0x8B82,
    kInfoLogLength = 0x8B84,
    kTriangles = 0x0004,
    kLineStrip = 0x0003,
    kFloat = 0x1406,
    kColorBufferBit = 0x00004000,
    kShaderLanguageVersion = 0x8B8C,
    kNoError = 0,
    kVersion = 0x1F02,
};

// 我们需要的全部函数指针
struct Api {
    // GL 1.1 核心
    const unsigned char* (*GetString)(unsigned int);
    unsigned int (*GetError)();
    void (*ClearColor)(float, float, float, float);
    void (*Clear)(unsigned int);
    void (*Viewport)(int, int, int, int);
    void (*Enable)(unsigned int);
    void (*Disable)(unsigned int);
    void (*BlendFunc)(unsigned int, unsigned int);
    void (*GetIntegerv)(unsigned int, int*);
    void (*LineWidth)(float);

    // GL 1.5 / 2.0：缓冲、着色器、attribute、绘制
    void (*GenBuffers)(int, unsigned int*);
    void (*BindBuffer)(unsigned int, unsigned int);
    void (*BufferData)(unsigned int, long, const void*, unsigned int);
    void (*BufferSubData)(unsigned int, long, long, const void*);
    void (*DeleteBuffers)(int, const unsigned int*);
    unsigned int (*CreateShader)(unsigned int);
    void (*ShaderSource)(unsigned int, int, const char* const*, const int*);
    void (*CompileShader)(unsigned int);
    void (*GetShaderiv)(unsigned int, unsigned int, int*);
    void (*GetShaderInfoLog)(unsigned int, int, int*, char*);
    void (*DeleteShader)(unsigned int);
    unsigned int (*CreateProgram)();
    void (*AttachShader)(unsigned int, unsigned int);
    void (*LinkProgram)(unsigned int);
    void (*GetProgramiv)(unsigned int, unsigned int, int*);
    void (*GetProgramInfoLog)(unsigned int, int, int*, char*);
    void (*DeleteProgram)(unsigned int);
    void (*UseProgram)(unsigned int);
    void (*BindAttribLocation)(unsigned int, unsigned int, const char*);
    int (*GetUniformLocation)(unsigned int, const char*);
    void (*Uniform1f)(int, float);
    void (*Uniform2f)(int, float, float);
    void (*EnableVertexAttribArray)(unsigned int);
    void (*DisableVertexAttribArray)(unsigned int);
    void (*VertexAttribPointer)(unsigned int, int, unsigned int, unsigned char, int,
                                const void*);
    void (*DrawArrays)(unsigned int, int, int);

    // 可选：VAO（GL >= 3.0 核心 profile 必需，GLES2 里不存在则为 null）
    void (*GenVertexArrays)(int, unsigned int*);
    void (*BindVertexArray)(unsigned int);
    void (*DeleteVertexArrays)(int, const unsigned int*);

    bool vaoSupported = false;
};

// 从当前线程的 GL 上下文加载函数指针。成功返回 true。
bool load(Api& api);

}  // namespace gl
}  // namespace cpplib
