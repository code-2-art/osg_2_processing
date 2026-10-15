/**
 * CppOsgViewer
 * -------------
 * 在 Processing 窗口里嵌入 OpenSceneGraph 渲染，并直接载入一个真实的 3D 模型。
 *
 * 原理：
 *  - Processing 用 P2D 创建 OpenGL 上下文；
 *  - osglib 在 C++ 侧创建一个 GraphicsWindowEmbedded（不管理 GL 上下文）；
 *  - 每帧 draw() 里调用 osg.frame()，OSG 直接在 Processing 当前的 GL 上下文里渲染。
 *
 * 交互（由 OSG 的 TrackballManipulator 处理，OsgView 已自动把 Processing 的
 * 鼠标/键盘事件转发给 OSG）：
 *  - 鼠标左键拖拽：旋转视角
 *  - 鼠标中键拖拽 / 右键拖拽：平移
 *  - 鼠标滚轮：缩放
 *  - 方向键：旋转（← → 绕 Z 轴，↑ ↓ 绕水平轴）
 *  - 'h'：复位相机（home）  's'：截图  'l'：选择其它模型文件
 *
 * 前置：运行 bash osg_lib/build.sh 安装 osglib 库，并重启 Processing。
 */

import org.example.osglib.*;

OsgView osg;
String statusText = "";

// 默认载入的模型：OSG 官方示例数据里的 cessna 飞机
String DEFAULT_MODEL =
  System.getProperty("user.home") + "/Public/Assets/osg-data/cessna.osg";

void setup() {
  size(1000, 720, P2D);       // 嵌入 OSG 必须使用 OpenGL 渲染器
  osg = new OsgView(this);    // 默认自动转发鼠标/键盘事件，漫游器开箱可用

  boolean ok = osg.loadModel(DEFAULT_MODEL);
  statusText = ok ? ("loaded: " + DEFAULT_MODEL) : ("load FAILED: " + DEFAULT_MODEL);
  println("[CppOsgViewer] " + statusText);
}

void draw() {
  osg.frame();                // 渲染 OSG 场景

  // 在 3D 之上叠一层 Processing HUD
  noStroke();
  fill(0, 150);
  rect(0, 0, width, 46);
  fill(255);
  textAlign(LEFT, TOP);
  textSize(13);
  text("OpenSceneGraph in Processing   |   drag=rotate  wheel=zoom  arrows=rotate  h=home  s=shot  l=load",
       10, 6);
  fill(180, 230, 255);
  text(statusText, 10, 26);
}

void keyPressed() {
  if (key == 'h') {
    osg.home();
  } else if (key == 's') {
    String path = sketchPath("osg_shot.ppm");
    println("saveScreenshot -> " + osg.saveScreenshot(path) + " : " + path);
  } else if (key == 'l') {
    selectInput("Select a 3D model file:", "modelSelected");
  }
}

void modelSelected(File selection) {
  if (selection == null) {
    println("no file selected");
    return;
  }
  boolean ok = osg.loadModel(selection.getAbsolutePath());
  if (ok) {
    osg.home();
    statusText = "loaded: " + selection.getAbsolutePath();
  } else {
    statusText = "load FAILED: " + selection.getAbsolutePath();
  }
  println("[CppOsgViewer] " + statusText);
}
