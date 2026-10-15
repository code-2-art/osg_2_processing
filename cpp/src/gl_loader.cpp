#include "gl_loader.h"

#include <cstdio>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__APPLE__)
#include <dlfcn.h>
#else
#include <dlfcn.h>
#endif

namespace cpplib {
namespace gl {

namespace {

#if defined(_WIN32)
HMODULE s_opengl32 = nullptr;
using WglGetProcAddrFn = PROC(WINAPI*)(LPCSTR);
WglGetProcAddrFn s_wglGetProcAddress = nullptr;

bool initModule() {
    s_opengl32 = LoadLibraryA("opengl32.dll");
    if (s_opengl32) {
        s_wglGetProcAddress =
            reinterpret_cast<WglGetProcAddrFn>(GetProcAddress(s_opengl32, "wglGetProcAddress"));
    }
    return s_opengl32 != nullptr;
}

void* lookup(const char* name) {
    // 1.2+ 的 core / 扩展函数由驱动提供，需要当前 context
    if (s_wglGetProcAddress) {
        PROC p = s_wglGetProcAddress(name);
        if (p) {
            return reinterpret_cast<void*>(p);
        }
    }
    // GL 1.1 函数直接来自 opengl32.dll
    if (s_opengl32) {
        FARPROC p = GetProcAddress(s_opengl32, name);
        if (p) {
            return reinterpret_cast<void*>(p);
        }
    }
    return nullptr;
}

#elif defined(__APPLE__)

void* s_framework = nullptr;

bool initModule() {
    s_framework = dlopen(
        "/System/Library/Frameworks/OpenGL.framework/Versions/Current/OpenGL", RTLD_NOW);
    return s_framework != nullptr;
}

void* lookup(const char* name) {
    return s_framework ? dlsym(s_framework, name) : nullptr;
}

#else  // Linux / other POSIX

void* s_libgl = nullptr;

bool initModule() {
    s_libgl = dlopen("libGL.so.1", RTLD_NOW | RTLD_GLOBAL);
    if (!s_libgl) {
        s_libgl = dlopen("libGL.so", RTLD_NOW | RTLD_GLOBAL);
    }
    return s_libgl != nullptr;
}

void* lookup(const char* name) {
    if (!s_libgl) {
        return nullptr;
    }
    void* p = dlsym(s_libgl, name);
    if (!p) {
        // 某些扩展只通过 glXGetProcAddress 暴露
        using GlXGetProc = void* (*)(const unsigned char*);
        static GlXGetProc s_glx =
            reinterpret_cast<GlXGetProc>(dlsym(s_libgl, "glXGetProcAddressARB"));
        if (s_glx) {
            p = s_glx(reinterpret_cast<const unsigned char*>(name));
        }
    }
    return p;
}

#endif

#define LOAD_REQUIRED(Field, Symbol)                                                          \
    do {                                                                                      \
        void* p = lookup(Symbol);                                                             \
        if (!p) {                                                                             \
            std::fprintf(stderr, "cpplib[gl]: required GL function not found: %s\n", Symbol); \
            return false;                                                                     \
        }                                                                                     \
        api.Field = reinterpret_cast<decltype(api.Field)>(p);                                 \
    } while (0)

#define LOAD_OPTIONAL(Field, Symbol)                                                    \
    do {                                                                                \
        api.Field = reinterpret_cast<decltype(api.Field)>(lookup(Symbol));              \
    } while (0)

}  // namespace

bool load(Api& api) {
    static bool moduleOk = initModule();
    if (!moduleOk) {
        std::fprintf(stderr, "cpplib[gl]: cannot open OpenGL library.\n");
        return false;
    }

    // GL 1.1
    LOAD_REQUIRED(GetString, "glGetString");
    LOAD_REQUIRED(GetError, "glGetError");
    LOAD_REQUIRED(ClearColor, "glClearColor");
    LOAD_REQUIRED(Clear, "glClear");
    LOAD_REQUIRED(Viewport, "glViewport");
    LOAD_REQUIRED(Enable, "glEnable");
    LOAD_REQUIRED(Disable, "glDisable");
    LOAD_REQUIRED(BlendFunc, "glBlendFunc");
    LOAD_REQUIRED(GetIntegerv, "glGetIntegerv");
    LOAD_OPTIONAL(LineWidth, "glLineWidth");  // 某些核心 profile 支持但会 clamp，可空

    // 1.5 / 2.0
    LOAD_REQUIRED(GenBuffers, "glGenBuffers");
    LOAD_REQUIRED(BindBuffer, "glBindBuffer");
    LOAD_REQUIRED(BufferData, "glBufferData");
    LOAD_REQUIRED(BufferSubData, "glBufferSubData");
    LOAD_OPTIONAL(DeleteBuffers, "glDeleteBuffers");
    LOAD_REQUIRED(CreateShader, "glCreateShader");
    LOAD_REQUIRED(ShaderSource, "glShaderSource");
    LOAD_REQUIRED(CompileShader, "glCompileShader");
    LOAD_REQUIRED(GetShaderiv, "glGetShaderiv");
    LOAD_REQUIRED(GetShaderInfoLog, "glGetShaderInfoLog");
    LOAD_REQUIRED(DeleteShader, "glDeleteShader");
    LOAD_REQUIRED(CreateProgram, "glCreateProgram");
    LOAD_REQUIRED(AttachShader, "glAttachShader");
    LOAD_REQUIRED(LinkProgram, "glLinkProgram");
    LOAD_REQUIRED(GetProgramiv, "glGetProgramiv");
    LOAD_REQUIRED(GetProgramInfoLog, "glGetProgramInfoLog");
    LOAD_REQUIRED(DeleteProgram, "glDeleteProgram");
    LOAD_REQUIRED(UseProgram, "glUseProgram");
    LOAD_REQUIRED(BindAttribLocation, "glBindAttribLocation");
    LOAD_REQUIRED(GetUniformLocation, "glGetUniformLocation");
    LOAD_REQUIRED(Uniform1f, "glUniform1f");
    LOAD_REQUIRED(Uniform2f, "glUniform2f");
    LOAD_REQUIRED(EnableVertexAttribArray, "glEnableVertexAttribArray");
    LOAD_REQUIRED(DisableVertexAttribArray, "glDisableVertexAttribArray");
    LOAD_REQUIRED(VertexAttribPointer, "glVertexAttribPointer");
    LOAD_REQUIRED(DrawArrays, "glDrawArrays");

    // 可选 VAO
    LOAD_OPTIONAL(GenVertexArrays, "glGenVertexArrays");
    LOAD_OPTIONAL(BindVertexArray, "glBindVertexArray");
    LOAD_OPTIONAL(DeleteVertexArrays, "glDeleteVertexArrays");
    api.vaoSupported = api.GenVertexArrays && api.BindVertexArray && api.DeleteVertexArrays;

    return true;
}

}  // namespace gl
}  // namespace cpplib
