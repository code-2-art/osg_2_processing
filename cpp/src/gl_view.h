#pragma once

namespace cpplib {

// ------------------------------------------------------------------
// GLView：在“Processing 当前激活的 OpenGL 上下文”里做原生绘制的对象。
//
// 生命周期约定（与 Java 侧 GLView 对应）：
//   1. createGLView() 只分配对象（不要依赖任何 GL 状态）；
//   2. 第一次 render() 时才真正加载函数指针、编译着色器（此时必在 draw() 里，
//      GL 上下文一定已激活）；
//   3. deleteGLView() 在 sketch 退出时调用（Java dispose 事件）。
// ------------------------------------------------------------------
class GLView {
public:
    GLView();
    ~GLView();

    GLView(const GLView&) = delete;
    GLView& operator=(const GLView&) = delete;

    void resize(int fbWidth, int fbHeight);
    void render(int fbWidth, int fbHeight, float timeSeconds);

private:
    struct Impl;
    Impl* d_;
};

GLView* createGLView();
void deleteGLView(GLView* view);

}  // namespace cpplib
