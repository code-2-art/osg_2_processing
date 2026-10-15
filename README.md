# cpplib —— Processing 原生库（C++/JNI）构建指南

cpplib 是一个 Processing 4 库模板：Java 侧（`src/main/java`）提供 `Compute` / `GLView` 等 API，真正干活的是 C++ 原生库（`cpp/`），通过 JNI 调用，产物是：

```
native/<os>-<arch>/libcpplib.so (或 cpplib.dll)   原生动态库（CMake 构建）
build/cpplib.jar                                  Java 侧 jar
dist/cpplib/                                      组装好的完整可发布/可安装目录
<sketchbook>/libraries/cpplib                     安装到 Processing 的结果
```

> 本文面向“把代码拷到一台**全新空白机器**”的场景，给出推荐的软件安装顺序和脚本执行顺序。
> 主要入口是一键脚本：
> - **Windows**：`tools/build.ps1`（PowerShell）
> - **Linux / macOS**：`tools/build.sh`（Bash）
>
> 两者都不依赖 Gradle、不依赖网络；Gradle 仅是可选替代。

---

## 1. 准备软件（按平台）

### 1.1 Windows

以下软件**全部装完即可，安装先后不影响结果**，但建议按此顺序操作（编译器 > 构建工具 > 运行库）：

| 顺序 | 软件 | 版本要求 | 说明 / 检查 |
| --- | --- | --- | --- |
| 1 | Visual Studio（含 MSVC） | 2019/2022 或更新 | 安装时必须勾选 **“使用 C++ 的桌面开发”**（Desktop development with C++），否则 CMake 找不到编译器 |
| 2 | CMake | ≥ 3.20 | 安装时勾选 **Add CMake to PATH**；检查：`cmake --version` |
| 3 | Processing 4 | 4.x（自带 JDK 17） | 官方下载：<https://processing.org/download>。自带 `app/resources/jdk`（JDK 17）和 `app/resources/core/library/core-*.jar`，**无需单独装 JDK** |
| 4 | NVIDIA CUDA Toolkit | 与你的 VS 版本匹配 | **可选**，仅当你需要 GPU 后端时安装 |
| 5 | Git | 任意较新版本 | 可选，用于克隆/更新代码 |

### 1.2 Linux

| 顺序 | 软件 | 版本要求 | 说明 / 检查 |
| --- | --- | --- | --- |
| 1 | GCC / G++ | ≥ 9（支持 C++17） | Ubuntu/Debian：`sudo apt install build-essential`；检查：`g++ --version` |
| 2 | CMake | ≥ 3.20 | `sudo apt install cmake`；检查：`cmake --version` |
| 3 | Processing 4 | 4.x（自带 JDK 17） | 官方下载 Linux 版并解压，例如 `~/Public/Misc/Processing`。自带 `lib/app/resources/jdk`（JDK 17）和 `lib/app/resources/core/library/core-*.jar` |
| 4 | OpenGL 驱动 / Mesa | 任意支持 OpenGL 2.0+ | 仅 GLView 示例需要；纯算法示例（Compute）不需要 GPU |
| 5 | NVIDIA CUDA Toolkit | 任意较新版本 | **可选**，仅当你需要 GPU 后端时安装 |

关于 JDK 的补充说明（Windows 与 Linux 通用）：

- 默认流程完全使用 **Processing 自带 JDK17**，无需安装任何 JDK。
- 若你希望用独立 JDK，安装一个 **JDK 17**（或更新的 17 系 LTS）并设置 `JAVA_HOME`，或在脚本里显式指定（`-JavaHome` / `--java-home`）。
- 原生构建（CMake）需要 `jni.h`，会自动按 `JAVA_HOME` → Processing 自带 JDK → `PROCESSING_HOME` → 系统 JNI 的顺序探测，一般无需手动配置。

---

## 2. 拷贝代码并执行构建

### 步骤 2.1：把代码拷到新机器

把整个仓库（**保持目录结构不变**，尤其 `cpp/`、`src/`、`tools/`、`native/`、`examples/`）复制到任意路径，避免放在含空格或特殊符号过长的路径。

### 步骤 2.2：一键构建（推荐，CPU 版）

**Windows（PowerShell）：**

```powershell
cd D:\dev\cpplib-template
powershell -ExecutionPolicy Bypass -File tools\build.ps1
```

**Linux / macOS（Bash）：**

```bash
cd ~/dev/cpplib-template
bash tools/build.sh
```

脚本会按顺序自动完成：

1. 定位 Processing / JDK / core.jar；
2. `javac` 编译 Java 并重新生成 JNI 头 `native/include/org_example_cpplib_NativeCore.h`；
3. 打包 `build/cpplib.jar`；
4. CMake 编译 C++ 原生库 → `native/<os>-<arch>/libcpplib.so`（Linux）或 `cpplib.dll`（Windows）；
5. 组装 `dist/cpplib/`（jar + 原生库 + library.properties + examples）；
6. JNI 命令行冒烟测试（不依赖 Processing 窗口）；
7. 安装到 sketchbook。

看到 **`JNI smoke test PASSED`** 和 **`All done.`** 即构建成功。

### 步骤 2.3：可选参数

**build.ps1（Windows）：**

| 参数 | 作用 |
| --- | --- |
| `-Cuda` | 启用 CUDA 后端 |
| `-SkipNative` | 跳过 C++/CMake 构建，只打包 Java + 组装 |
| `-SkipTest` | 跳过 JNI 冒烟测试 |
| `-SkipInstall` | 只产出 `dist\cpplib`，不安装到 sketchbook |
| `-JavaHome <path>` | 指定 JDK（默认用 Processing 自带 JDK17） |

**build.sh（Linux / macOS）：**

| 参数 | 作用 |
| --- | --- |
| `--cuda` | 启用 CUDA 后端 |
| `--skip-native` | 跳过 C++/CMake 构建 |
| `--skip-test` | 跳过 JNI 冒烟测试 |
| `--skip-install` | 只产出 `dist/cpplib`，不安装到 sketchbook |
| `--java-home <path>` | 指定 JDK |

示例：

```bash
# 只改过 Java 代码时，跳过 C++ 编译快速重装
bash tools/build.sh --skip-native

# 只想看 dist 产物，不装到 Processing
bash tools/build.sh --skip-install
```

> 关于 `PROCESSING_HOME`：脚本会自动探测常见安装位置。若 Processing 不在默认位置，设置环境变量：
> ```bash
> export PROCESSING_HOME=/path/to/Processing/lib   # Linux
> # 或
> $env:PROCESSING_HOME="C:\Program Files\Processing"  # Windows
> ```

### 步骤 2.4：手动分步构建（可选）

仅编译原生库：

```bash
# Linux
cpp/build-native.sh linux-amd64

# Windows
cpp\build-native.bat windows-amd64
```

> Gradle 可选路径：`./gradlew buildNative`（只编译 C++）、`./gradlew buildAndDeploy`（原生 + Java + 安装）。
> Gradle 路径首次运行需要联网下载 Gradle 与依赖，故非默认推荐方式。

---

## 3. 在 Processing 里使用

1. 若 Processing 已打开，请 **完全退出并重新打开**；
2. 菜单 `File > Examples > Libraries > cpplib`，应能看到两个示例：
   - `CppComputeMandelbrot` —— 纯算法示例，默认 2D 渲染器即可运行，展示 C++ 计算 Mandelbrot 并把像素传回 `PImage`；
   - `CppGLTriangle` —— OpenGL 示例，`size(..., P2D)` 下由 C++ 直接绘制三角形，需要 GPU 支持 OpenGL。

也可以用命令行直接运行示例（Linux / macOS）：

```bash
~/Public/Misc/Processing/bin/Processing cli \
  --sketch=~/sketchbook/libraries/cpplib/examples/CppComputeMandelbrot --run
```

验证是否装好：安装目录应包含以下内容（以 Linux `~/sketchbook/libraries/cpplib` 为例）：

```
cpplib/
├─ library.properties
├─ library/
│  ├─ cpplib.jar
│  └─ native/
│     ├─ linux-amd64/libcpplib.so
│     └─ windows-amd64/cpplib.dll
├─ examples/CppComputeMandelbrot/...
└─ examples/CppGLTriangle/...
```

重新安装库 = 重跑一键构建脚本（会自动先删除旧安装目录）。

---

## 4. 常见问题排查

| 现象 | 原因 / 解决 |
| --- | --- |
| `cmake not found on PATH` | 未安装 CMake 或没加入 PATH；`sudo apt install cmake`（Linux）或重装勾选 Add to PATH（Windows） |
| 提示找不到 C/C++ 编译器 | Linux 未装 `build-essential`；Windows 未装“使用 C++ 的桌面开发”工作负载 |
| `JDK tool missing: javac` | 用的 Processing 是精简版/没带 JDK；改用 `-JavaHome` / `--java-home` 指定独立 JDK |
| `Processing core jar not found` | 未找到 `core-*.jar`；确认 Processing 已安装或设置 `PROCESSING_HOME` |
| JNI 冒烟测试报 `UnsatisfiedLinkError` | 通常是 jar 旁的 `library/native/<os>-<arch>/libcpplib.so` 缺失或与 Java 签名不一致；重跑完整构建 |
| 修改了 `NativeCore.java` 的 native 方法签名 | 必须重跑完整构建脚本（含 native 步骤），让 `javac -h` 重新生成 JNI 头并用它重编 C++ |
| Processing 示例列表里没有 cpplib | 重启 Processing；确认安装目录完整（见第 3 节） |
| Linux 下 `libGL.so.1: cannot open shared object file` | 缺少 OpenGL 运行库；`sudo apt install libgl1-mesa-glx` 或安装显卡驱动 |
| CUDA 示例运行报找不到 `cudart` | CUDA 构建后依赖运行库；确认 native 目录下有 `libcudart.so.*`，且 GPU 驱动可用 |

---

## 5. 目录速览

```
cpplib-template/
├─ src/main/java/org/example/cpplib/   Java 侧：Compute / GLView / CPPLib / NativeCore
├─ cpp/                                 C++/CUDA 源码 + CMakeLists.txt
│  ├─ src/bridge.cpp                    JNI 导出层（Java_org_example_cpplib_*）
│  ├─ src/engine.cpp / mandelbrot.cpp   CPU 计算引擎
│  ├─ src/gl_loader.cpp / gl_view.cpp   OpenGL 动态加载与渲染
│  └─ mandelbrot_cuda.cu                CUDA 后端（--cuda 时编译）
├─ tools/build.ps1                      Windows 一键构建 + 冒烟测试 + 安装（推荐入口）
├─ tools/build.sh                       Linux/macOS 一键构建 + 冒烟测试 + 安装（推荐入口）
├─ tools/jnitest/                       命令行冒烟测试源码
├─ native/<os>-<arch>/                  CMake 产物输出目录（.so / .dll）
├─ examples/                            Processing 示例（含 osglib / osgverselib 的示例）
├─ library.properties / release.properties  发布元数据
├─ osg_lib/                             OpenSceneGraph 集成库（见第 6 节）
├─ osgverse_lib/                        osgVerse 集成库：PBR 延迟渲染 + 数字地球（见第 7 节）
└─ build.gradle.kts                     Gradle 发布脚本（可选）
```

---

## 6. osg_lib —— 在 Processing 窗口里跑 OpenSceneGraph

`osg_lib/` 是一个**独立的**库，把 OpenSceneGraph 当作底层渲染引擎嵌入 Processing 窗口。
和 cpplib 不同，它不自己写 OpenGL，而是：

- 在 C++ 侧创建一个 `osgViewer::Viewer` +
  `osgViewer::GraphicsWindowEmbedded`（**不管理 GL 上下文，不 makeCurrent**）；
- 每帧在 Processing 的 `draw()` 里调用 `osg.frame()`，OSG 直接使用 Processing 当前
  已经激活的 GL 上下文渲染（`viewer.frame()`）。

### 6.1 前置条件

- 一个 **GLCORE** 构建的 OpenSceneGraph SDK（包含 `include/` 与 `lib/`，含
  `libosgViewer.so`、`osgPlugins-*` 等）。默认探测 `~/Public/osgverse/build/sdk_core`，
  也可用 `--osg-sdk <path>` 或环境变量 `OSG_SDK_ROOT` 指定。
- 其余（Processing / JDK / CMake / 编译器）与第 1 节相同。

### 6.2 构建

```bash
# Linux / macOS
bash osg_lib/build.sh
# 指定 SDK 路径 / 只打包不安装
bash osg_lib/build.sh --osg-sdk /path/to/sdk_core --skip-install
```

脚本流程：`javac`（生成 JNI 头）→ 打包 `osg_lib.jar` → CMake 编译
`libosglib.so`（通过 rpath 指向 OSG SDK 的 `lib/`）→ 组装 `osg_lib/dist/osglib/`
→ 安装到 `<sketchbook>/libraries/osglib`（含示例）。

### 6.3 使用

```java
import org.example.osglib.*;

OsgView osg;

void setup() {
  size(900, 680, P2D);       // 必须用 OpenGL 渲染器（P2D / P3D）
  osg = new OsgView(this);   // 内部创建 GraphicsWindowEmbedded，并自动转发鼠标/键盘事件
  osg.loadModel(System.getProperty("user.home") + "/Public/Assets/osg-data/cessna.osg");
}

void draw() {
  osg.frame();               // 渲染一帧 OSG 场景
  // 之后可以继续用 Processing 画 2D 覆盖层（HUD / 交互提示）
}

void keyPressed() {
  if (key == 's') osg.saveScreenshot(sketchPath("shot.ppm"));
}
```

`OsgView` 的 API：

| 方法 | 说明 |
| --- | --- |
| `frame()` | 渲染一帧。放在 `draw()` 里，建议靠近末尾 |
| `loadModel(path)` | 载入模型（osg/ive/obj/3ds/gltf 等，取决于 OSG 插件），返回是否成功 |
| `home()` | 相机重置为包围整个场景 |
| `setViewpoint(x,y,z,h,p,r)` | 设置视点（眼位置 + heading/pitch/roll，度） |
| `saveScreenshot(path)` | 把当前帧缓冲存为 PPM（截图 / 自动化测试） |
| `mouseMove/mousePress/mouseRelease/mouseScroll` | 手动投递鼠标事件（坐标原点左上、Y 向下；按键 1=左 2=中 3=右） |
| `keyPress/keyRelease` | 手动投递键盘事件（参数为 `osgGA::GUIEventAdapter::KeySymbol`） |

示例：`examples/CppOsgViewer`（默认载入 `cessna.osg`，鼠标拖拽旋转、滚轮缩放、
方向键旋转、`h` 重置、`s` 截图、`l` 选择其它模型文件）。

### 6.4 输入事件与 OSG 漫游器

`new OsgView(this)` 默认会把 sketch 注册为 Processing 的 `mouseEvent` / `keyEvent`
监听器，把坐标与按键映射成 OSG 事件后推进 `GraphicsWindowEmbedded` 的事件队列；
`Viewer::frame()` 在 `eventTraversal()` 阶段消费这些事件并分发给 `CameraManipulator`
（默认 `osgGA::TrackballManipulator`）。因此拖拽旋转、滚轮缩放、方向键旋转开箱即用。

如果需要自己控制事件，用 `new OsgView(this, false)` 关闭自动转发，再手动调用
`osg.mousePress(...)` / `osg.mouseMove(...)` / `osg.keyPress(...)` 等方法。

坐标约定与 Processing 一致：原点在窗口**左上角**，Y 轴**向下**（OSG 的
`GUIEventAdapter` 默认就是 `Y_INCREASING_DOWNWARDS`，无需翻转）。

### 6.5 模型载入（GLCORE 兼容处理）

`loadModel()` 会先把这个 OSG SDK 的 **GLCORE 限制**处理掉，再交给 OSG 渲染：

- 该 SDK 里 `osg::Light` / `Material` / `ShadeModel` / `LightModel` / `TexGen` 都是空实现
  （`apply()` 只打印 "not supported"），且 `GL_LIGHTn` / `GL_TEXTURE_GEN_*` 在
  core profile 下是非法枚举。因此载入后会遍历整棵场景图，把这些光照 / 材质 /
  纹理生成相关的**模式与属性全部清掉**（普通属性 + 纹理属性都要清，`TexGen` 属于后者）。
- 很多老模型（如 `cessna.osg`）没有任何顶点色，只靠 `Material` 决定颜色。清掉材质后
  `osg_lib` 会把材质的漫反射色**逐顶点写成顶点色**（缺省浅灰），这样不依赖固定管线光照
  也能看见模型。
- 之后挂一个最小化的「仅顶点色」着色器（只声明 `osg_Vertex` / `osg_Color`），
  绕开 OSG 默认 GL3 着色器对 `osg_MultiTexCoord0` 等属性的要求。
- 最后用 `osgUtil::Optimizer`（`INDEX_MESH | MERGE_GEOMETRY | TESSELLATE_GEOMETRY`）
  整理几何体：`cessna.osg` 原本是 1117 个独立的 `DrawArrays TRIANGLE_STRIP`，索引合并后
  只剩一次索引绘制；`GL_QUADS` / `GL_POLYGON` 在 core profile 下是非法枚举，
  必须三角化（如 `morphing.osg`）才能画出来。

实测可正常显示：`cessna.osg`、`cow.osg`、`glider.osg`、`dumptruck.osg`、`axes.osgt`、
`morphing.osg`、`nathan.osg`、`avatar.osg`（在 `~/Public/Assets/osg-data/` 下）。
含蒙皮动画（`osgAnimation::RigGeometry`）或复杂纹理环境的模型，这个 GLCORE SDK 本身
也有少量固定管线残留报警（用官方 `osgviewer` 跑同样会报），但不影响显示。

### 6.6 实现要点（为什么能稳定共存）

Processing（P2D/P3D）和 OSG 共用同一个 GL 上下文，两者都会缓存自己的 GL 状态
（VAO / EBO / Program）。直接交替渲染会互相污染对方的 VAO，导致 OSG 绘制越界崩溃。
`osg_lib` 在 [osg_viewer.cpp](file:///home/wangrui/Public/Misc/Processing/cpplib-template/osg_lib/src/osg_viewer.cpp) 的 `frame()` 里做了四件事：

1. **进入时保存** Processing 当前的 `VAO / ARRAY_BUFFER / ELEMENT_ARRAY_BUFFER / Program` 绑定；
2. 解绑后 **`state->reset()`** 重置 OSG 状态缓存，再 `viewer.frame()`；
3. **作废 OSG 的绑定缓存**：`setCurrentVertexArrayObject(0)` +
   `resetBufferObjectPointers()`。OSG 对 VAO / VBO / EBO 的绑定有内部缓存，
   如果只改真实 GL 绑定而不通知 OSG，它下一帧会以为"还绑着"而跳过绑定，
   在 core profile 下就会用无效状态绘制——现象是**每帧一堆 `GL_INVALID_OPERATION`
   且模型完全画不出来**；
4. **退出时原样恢复** 第 1 步保存的绑定。

这样 Processing 的状态缓存始终与真实 GL 状态一致，不会把自己的 EBO 绑到 OSG 的 VAO 上，
从而消除崩溃。已实测：`background + osg.frame()`、`rect + osg.frame()`、
`text + osg.frame()`、P2D/P3D 混用、以及 OSG 之上再画 Processing HUD 均稳定运行。

> 注意：由于 OSG 直接渲染到当前 GL 上下文，`size()` 必须使用 `P2D` 或 `P3D`；
> 默认渲染器（无 OpenGL）无法承载 OSG。

---

## 7. osgverse_lib —— 在 Processing 窗口里跑 osgVerse（PBR 延迟渲染 / 数字地球）

`osgverse_lib/` 建立在 `osg_lib` 的同一套思路上（`GraphicsWindowEmbedded` + 每帧
`viewer.frame()`，GL 上下文完全交给 Processing），但把渲染管线换成了 **osgVerse**
的 `osgVerse::Pipeline`，因此可以直接复用 osgVerse 的 PBR 材质、延迟渲染、阴影、
SSAO、泛光、色调映射、HDR 天空盒，以及 TMS 数字地球插件。

### 7.1 前置条件

- **osgVerse 的 GLCORE SDK**：目录里同时有 osgVerse 和 OpenSceneGraph 的头文件与
  库（`include/osgVerse/`、`lib/libosgVerse*.so`、`lib/cmake/osgVerse/`、
  `shaders/`、`skyboxes/`、`misc/`、`models/`）。默认探测
  `~/Public/osgverse/build/sdk_core`，可用 `--osgverse-sdk <path>` 指定。
- 其余（Processing / JDK / CMake / 编译器）与第 1 节相同。

### 7.2 构建

```bash
bash osgverse_lib/build.sh
# 指定 SDK 路径 / 只打包不安装 / 只重装 Java
bash osgverse_lib/build.sh --osgverse-sdk /path/to/sdk_core --skip-install
bash osgverse_lib/build.sh --skip-native
```

脚本流程：`javac`（生成 JNI 头）→ 打包 `osgverselib.jar` → CMake 编译
`libosgverselib.so`（用 SDK 自带的 `find_package(osgVerse)` 找到
`osgVerse::Verse` / `osgVerse::OSG`，rpath 指向 `<sdk>/lib`）→ 组装
`osgverse_lib/dist/osgverselib/` → 安装到 `<sketchbook>/libraries/osgverselib`。

### 7.3 使用

```java
import org.example.osgverselib.*;

OsgVerseView view;

void setup() {
  size(1280, 720, P2D);            // 必须用 OpenGL 渲染器
  view = new OsgVerseView(this);   // 自动转发鼠标/键盘事件
  view.createPbrScene("");         // 空字符串 = SDK 自带的 models/Sponza.osgb
  // view.createEarthScene("", "");  // 或：高德影像瓦片 + EarthManipulator
}

void draw() {
  view.frame();
  // 之后可以照常画 Processing 的 2D HUD
}

void keyPressed() {
  if (key == 'h') view.home();
  if (key == 's') view.saveScreenshot(sketchPath("shot.ppm"));
}
```

`OsgVerseView` 的 API：

| 方法 | 说明 |
| --- | --- |
| `frame()` | 渲染一帧。放在 `draw()` 里，建议靠近末尾 |
| `createPbrScene(path)` | 建 PBR + 延迟渲染场景；`path` 为空时用 SDK 的 `models/Sponza.osgb` |
| `createEarthScene(orthoUrl, startFile)` | 建数字地球；`orthoUrl` 为空用高德影像瓦片，`startFile` 为空用 `0-0-0.verse_tms` |
| `setEarthViewpoint(lon, lat, height)` | 地球相机定位到经纬度（度）与距地面高度（米） |
| `home()` | 复位相机 |
| `saveScreenshot(path)` | 把当前帧缓冲存为 PPM（截图 / 自动化测试） |
| `getMode()` | 当前场景类型（`MODE_NONE` / `MODE_PBR` / `MODE_EARTH`） |
| `mouseMove/mousePress/mouseRelease/mouseScroll` | 手动投递鼠标事件（坐标原点左上、Y 向下；按键 1=左 2=中 3=右） |
| `keyPress/keyRelease` | 手动投递键盘事件（参数为 `osgGA::GUIEventAdapter::KeySymbol`） |

示例：

- `examples/CppOsgVerseViewer`：Sponza 大厅的 PBR + 延迟渲染（G-Buffer / SSAO /
  阴影 / 泛光 / 色调映射 / FXAA + HDR 天空盒）。可用环境变量
  `OSGVERSE_MODEL=<模型路径>` 换成自己的模型。
- `examples/CppOsgVerseEarth`：TMS 影像瓦片数字地球 + `EarthManipulator` 漫游
  （默认高德影像瓦片），按 `b` 飞到北京上空。

### 7.4 输入事件与漫游器

和 `osg_lib` 完全一致：`new OsgVerseView(this)` 会把 sketch 注册为 Processing 的
`mouseEvent` / `keyEvent` 监听器，把事件推进 `GraphicsWindowEmbedded` 的事件队列；
`Viewer::frame()` 在 `eventTraversal()` 阶段把它们分发给 `CameraManipulator`。
PBR 场景默认是 `osgGA::TrackballManipulator`，地球场景是
`osgVerse::EarthManipulator`。要自己控制事件就用 `new OsgVerseView(this, false)`。

### 7.5 实现要点

**（1）延迟管线接管绘制。** osgVerse 的管线会把主相机的 `GraphicsContext` 置空，
真正绘制由各 stage 相机完成，所以必须覆盖 `Viewer::createRenderer()` 返回
`pipeline->createRenderer(camera)`（`osgverse_viewer.cpp` 里的 `PipelineViewer`）。
也正因如此，`saveScreenshot()` 必须用我们自己的 `GraphicsWindowEmbedded` 取
`State`，而不能用 `camera->getGraphicsContext()`。

**（2）目录变量要自己补齐。** `osgVerse::globalInitialize()` 只会把 `BASE_DIR`
设成 SDK 根目录，`SHADER_DIR` / `SKYBOX_DIR` / `MISC_DIR` 仍是静态初始化时的
`"../xxx/"`（osgVerse 假定可执行文件在 `<BASE_DIR>/bin` 下）。嵌入式场景里我们不
改工作目录，所以初始化后要显式把它们指向 `<SDK>/shaders` 等目录，否则管线找不到
着色器。另外传入 `argc = 0`，避免 osgVerse 改动 Processing 的工作目录。

**（3）参数对象的构造顺序。** `StandardPipelineParameters(shaderDir, skyboxFile)`
的构造函数会加载天空盒贴图（HDR）和 IBL 数据，而这张贴图同时是 PBR 光照的
环境光来源。所以必须**先用它建好参数，再创建 `osgVerse::SkyBox`**；否则
`setEnvironmentMap(nullptr)`，天空盒会变成一片白，模型因为拿不到 IBL 而全黑。

**（4）窗口尺寸与 resize。** 参数里的 `originWidth/originHeight` 决定管线内部
缓冲尺寸和主相机视口，必须用当前窗口尺寸。窗口尺寸变化时，延迟管线的内部缓冲
尺寸在 setup 时就固定了，只能 `clearStagesFromView()` 后重新 `setupStandardPipeline()`
（`resize()` 里做的就是这个，会有一次短暂卡顿）。

**（5）和 Processing 共用一个 GL 上下文。** `frame()` 里处理 VAO / VBO / EBO /
Program 绑定隔离的方式与 `osg_lib` 的第 6.6 节完全相同（保存 → 解绑 →
`state->reset()` → 渲染 → 作废 OSG 的绑定缓存 → 恢复）。

**（6）退出时不崩溃。** `GraphicsWindowEmbedded` 在 OSG 内部被大量对象长期持有
（实测引用计数 17），因此 `viewer` 析构时 `~GraphicsContext` 并不会被触发，
contextID 对应的 `osg::ContextData` 会一直留在 libosg 的静态 map 里。而 libosg 里
静态对象的析构顺序是「`ContextData` 的 map 晚于 `GLExtensions` 的静态数组」，于是
进程退出时会走
`~ContextData → ~VertexArrayStateManager → ~VertexArrayState → ~GLExtensions`，
访问已析构的静态数组 —— 表现为 **JVM 在退出瞬间 SIGSEGV，栈顶是
`osg::GLExtensions::~GLExtensions()`**。`OsgVerseViewer` 的析构函数因此在释放完
所有 OSG 对象后，还会调用 `releaseIdleContextData()` 把空闲的
`osg::ContextData` 提前释放掉。`OsgVerseView` 通过
`parent.registerMethod("dispose", this)` 挂到 Processing 的退出流程上，正常关闭
窗口或用 `exit()` 结束 sketch 都会走到这里，无需手工调用。

> 日志里会出现 osgVerse 自带的
> `Warning: detected OpenGL error 'invalid enumerant' after applying attribute ...`。
> 这是 osgVerse 的 `LightDrawable` 在 GL Core 下仍会尝试应用固定管线的
> `GL_LIGHTING`（core profile 里的非法枚举）导致的，用官方 `osgVerse_Viewer`
> 跑同样会报，不影响显示。`osgverse_lib` 默认把 OSG 的 GL 错误检查设成
> 「每个属性只报一次」，避免逐帧刷屏。

