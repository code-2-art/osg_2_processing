/**
 * CppOsgVerseEarth
 * ----------------
 * 在 Processing 窗口里展示 osgVerse 的数字地球。
 *
 * 组成：
 *  - osgdb_verse_tms 插件：按 TMS 金字塔规则从网络下载影像瓦片并拼成球面地形
 *    （默认使用高德影像瓦片，和其他瓦片源只需换 URL，见 createEarthScene）；
 *  - osgVerse::EarthManipulator：以经纬度/高度为单位的地球漫游器；
 *  - osgVerse::createDefaultProgram("BaseTexture")：GLCORE 下给瓦片用的最简贴图着色器。
 *
 * 原理：
 *  - Processing 用 P2D 创建 OpenGL 上下文；
 *  - osgverse_lib 在 C++ 侧创建一个 GraphicsWindowEmbedded（不管理 GL 上下文）；
 *  - 每帧 draw() 里调用 view.frame()，osgVerse 直接在 Processing 当前的 GL 上下文里渲染。
 *
 * 交互（OsgVerseView 已自动把 Processing 的鼠标/键盘事件转发给 OSG）：
 *  - 鼠标左键拖拽：转动地球（按住拖动改变经纬度）
 *  - 鼠标右键/滚轮：拉近拉远
 *  - 'b'：飞到北京上空 200km（演示 setEarthViewpoint）   'h'：复位相机   's'：截图（PPM）
 *
 * 注意：瓦片是联网按需下载的，首次运行会先看到低分辨率地球，随后逐级清晰；
 * 离线环境只能看到空白地球。
 *
 * 前置：运行 bash osgverse_lib/build.sh 安装 osgverselib 库，并重启 Processing。
 */

import org.example.osgverselib.*;

OsgVerseView view;
String statusText = "";

void setup() {
  size(1280, 720, P2D);       // 嵌入 osgVerse 必须使用 OpenGL 渲染器
  view = new OsgVerseView(this);

  // 第 1 个参数是影像瓦片 URL（空 = 高德），第 2 个是起始 TMS 根节点（空 = 0-0-0.verse_tms）
  boolean ok = view.createEarthScene("", "");
  statusText = ok
    ? "Earth: TMS (AutoNavi imagery) + EarthManipulator"
    : "create FAILED (check network / TMS plugin)";
  println("[CppOsgVerseEarth] " + statusText);
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
  text("osgVerse digital earth in Processing   |   drag=rotate  wheel=zoom  b=Beijing  h=home  s=shot",
       10, 6);
  fill(180, 230, 255);
  text(statusText, 10, 26);
}

void keyPressed() {
  if (key == 'h') {
    view.home();
  } else if (key == 'b') {
    view.setEarthViewpoint(116.391, 39.907, 200000.0);   // 北京（经度, 纬度, 距地面高度/米）
  } else if (key == 's') {
    String path = sketchPath("osgverse_earth.ppm");
    println("[CppOsgVerseEarth] saveScreenshot -> " + view.saveScreenshot(path) + " : " + path);
  }
}
