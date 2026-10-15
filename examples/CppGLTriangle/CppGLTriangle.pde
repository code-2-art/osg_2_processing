/**
 * CppGLTriangle
 * -------------
 * OpenGL 绘制示例：C++ 在 Processing 的 OpenGL 上下文里“亲手”画东西。
 * 屏幕上看到的三角形 + 正弦亮带，全部由 cpplib.dll 内部完成
 * （编译着色器、上传 VBO、glDrawArrays），Processing 没有参与这部分渲染。
 *
 * 使用要点：
 *  1) size(w, h, P2D) / P3D 必须使用 OpenGL 渲染器；
 *  2) gl.render(t) 必须放在 draw() 的**最后一行**。渲染前 Java wrapper 会先
 *     flush 掉 Processing 本帧累积的几何，避免背景把原生内容盖住；
 *  3) 想做 CUDA->OpenGL 互操作（例如 GPU 算完直接渲染纹理/顶点），
 *     在 C++ 侧 gl_view / renderer 里加 cudaGraphicsGLRegisterBuffer 即可。
 */

import org.example.cpplib.*;

GLView gl;

void setup() {
  size(800, 600, P2D);   // 也可以用 P3D
  gl = new GLView(this);
  println("cpplib version: " + CPPLib.version());
}

void draw() {
  // Processing 先正常画自己的内容（背景、普通图形……）
  background(24);
  noStroke();
  fill(255, 60);
  ellipse(width * 0.5, height * 0.35, 40, 40);
  fill(255, 120);
  rect(10, height - 46, 4, 36);

  // 让 C++ 在本窗口上叠加原生 OpenGL 绘制（必须最后调用）
  gl.render(millis() / 1000.0);
}
