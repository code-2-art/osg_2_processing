// Windows 专属：opengl32.lib 只静态导出 OpenGL 1.1 函数；GL 3.x 核心函数
// （glBindVertexArray / glUseProgram / glBindBuffer / glBindFramebuffer）在 Windows
// 上只能通过 wglGetProcAddress 获取。这里提供同名符号，首次调用时惰性解析。
//
// 仅 64 位（x64）下使用：x64 平台只有单一调用约定，无需关心 stdcall / cdecl 差异。
// glcorearb.h 把 GLAPI 定义为 extern、APIENTRY 定义为空，因此本文件提供的函数
// 与 OSG 头里声明的签名一致，可以正常链接。

#include <windows.h>
#include <GL/glcorearb.h>

#define IMPL_GL_VOID_1(name, t1)                                            \
    typedef void (APIENTRY *name##_t)(t1);                                  \
    static name##_t name##_ptr = nullptr;                                   \
    extern "C" void APIENTRY name(t1 a1) {                                  \
        if (!name##_ptr) name##_ptr = (name##_t)wglGetProcAddress(#name);   \
        if (name##_ptr) name##_ptr(a1);                                     \
    }

#define IMPL_GL_VOID_2(name, t1, t2)                                        \
    typedef void (APIENTRY *name##_t)(t1, t2);                              \
    static name##_t name##_ptr = nullptr;                                   \
    extern "C" void APIENTRY name(t1 a1, t2 a2) {                           \
        if (!name##_ptr) name##_ptr = (name##_t)wglGetProcAddress(#name);   \
        if (name##_ptr) name##_ptr(a1, a2);                                 \
    }

IMPL_GL_VOID_1(glBindVertexArray, GLuint)
IMPL_GL_VOID_1(glUseProgram, GLuint)
IMPL_GL_VOID_2(glBindBuffer, GLenum, GLuint)
IMPL_GL_VOID_2(glBindFramebuffer, GLenum, GLuint)
