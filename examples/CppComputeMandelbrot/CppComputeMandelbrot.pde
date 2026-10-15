/**
 * CppComputeMandelbrot
 * --------------------
 * 纯算法示例：重计算发生在 C++ 侧（CPU 多线程；若构建时启用 CUDA 则自动走 GPU），
 * Processing 只负责接收像素结果并显示。
 *
 * 说明：
 *  - Compute cpp = new Compute(this);   // setup() 里创建一次 C++ 引擎
 *  - cpp.mandelbrot(pixels, w, h, ...); // 每帧把结果算进 int[]（PImage.pixels）
 *  - sketch 退出时 Compute.dispose() 会被 Processing 自动调用（registerMethod）
 *
 * 如果你要在 C++ 侧接自己的算法（矩阵乘 / FFT / AI 推理…），
 * 只要在 engine / bridge 层按同样的“Java 传参 -> C++ 算 -> 结果写回数组”格式加方法即可。
 */

import org.example.cpplib.*;

Compute cpp;
PImage img;          // 低分辨率缓存图，再放大显示，避免每帧传太多数据
int cw = 480;        // 计算宽度（像素）
int ch = 270;        // 计算高度（像素）
int maxIter = 180;
String infoText = "";

void setup() {
  size(960, 540);
  noSmooth();
  img = createImage(cw, ch, RGB);
  cpp = new Compute(this);
  infoText = cpp.info();
  println("backend: " + infoText);
}

void draw() {
  background(12);
  double t = millis() / 1000.0;

  // 动画化的观察窗口（中心缓慢漂移 + 缩放）
  double centerX = -0.65 + 0.20 * Math.sin(t * 0.11);
  double centerY = 0.02 + 0.15 * Math.cos(t * 0.13);
  double zoom = 0.7 + 0.65 * Math.sin(t * 0.23);   // 控制窗口大小
  double spanY = 2.0 / Math.max(0.35, zoom);
  double spanX = spanY * (double) cw / (double) ch;

  double x0 = centerX - spanX / 2, x1 = centerX + spanX / 2;
  double y0 = centerY - spanY / 2, y1 = centerY + spanY / 2;

  // C++ 计算整幅 Mandelbrot，直接写进 img.pixels
  cpp.mandelbrot(img.pixels, cw, ch, maxIter, x0, y0, x1, y1);
  img.updatePixels();
  image(img, 0, 0, width, height);

  // 顶部信息栏（Processing 侧 UI）
  noStroke();
  fill(0, 180);
  rect(0, 0, width, 26);
  fill(255);
  textAlign(LEFT, CENTER);
  textSize(13);
  text("cpplib " + CPPLib.version() + "  |  fps " + nf(frameRate, 2, 1)
     + "  |  " + infoText, 8, 13);
}

void mousePressed() {
  // 点一下切换一次最高迭代次数，看看“慢”的效果
  maxIter = (maxIter == 180) ? 500 : 180;
}
