/**
 * CppOsgVerseViewer
 * -----------------
 * 在 Processing 窗口里跑 osgVerse 的 PBR + 延迟渲染管线。
 *
 * 渲染链路（全部由 osgVerse::Pipeline 自动搭建）：
 *   G-Buffer → SSAO → 阴影合成 → PBR 光照 → 泛光/色调映射 → FXAA → 显示
 * 另有一个独立的天空盒相机在延迟阶段之后绘制 HDR 环境（barcelona.hdr + IBL）。
 *
 * 原理：
 *  - Processing 用 P2D 创建 OpenGL 上下文；
 *  - osgverse_lib 在 C++ 侧创建一个 GraphicsWindowEmbedded（不管理 GL 上下文）；
 *  - 每帧 draw() 里调用 view.frame()，osgVerse 直接在 Processing 当前的 GL 上下文里渲染。
 *
 * 交互（OsgVerseView 已自动把 Processing 的鼠标/键盘事件转发给 OSG）：
 *  - 鼠标左键拖拽：旋转   中键/右键拖拽：平移   滚轮：缩放
 *  - 'h'：复位相机   's'：截图（PPM）
 *
 * 默认模型是 osgVerse SDK 自带的 models/Sponza.osgb；
 * 想换成自己的模型时，设置环境变量 OSGVERSE_MODEL=<模型路径> 再运行即可。
 *
 * 前置：运行 bash osgverse_lib/build.sh 安装 osgverselib 库，并重启 Processing。
 */

import org.example.osgverselib.*;

OsgVerseView view;
String statusText = "";

void setup() {
  size(1280, 720, P2D);       // 嵌入 osgVerse 必须使用 OpenGL 渲染器
  view = new OsgVerseView(this);

  String model = System.getenv("OSGVERSE_MODEL");
  if (model == null) model = "";          // 空 = 使用 SDK 自带的 Sponza.osgb
  boolean ok = view.createPbrScene(model);
  statusText = ok
    ? ("PBR scene: " + (model.isEmpty() ? "Sponza.osgb (SDK)" : model))
    : ("create FAILED: " + model);
  println("[CppOsgVerseViewer] " + statusText);
}

void draw() {
  view.frame();               // 渲染一帧 osgVerse 场景

  // 在 3D 之上叠一层 Processing HUD
  noStroke();
  fill(0, 150);
  rect(0, 0, width, 46);
  fill(255);
  textAlign(LEFT, TOP);
  textSize(13);
  text("osgVerse PBR + deferred pipeline in Processing   |   drag=rotate  wheel=zoom  right-drag=pan  h=home  s=shot",
       10, 6);
  fill(180, 230, 255);
  text(statusText, 10, 26);
}

void keyPressed() {
  if (key == 'h') {
    view.home();
  } else if (key == 's') {
    String path = sketchPath("osgverse_pbr.ppm");
    println("[CppOsgVerseViewer] saveScreenshot -> " + view.saveScreenshot(path) + " : " + path);
  }
}
