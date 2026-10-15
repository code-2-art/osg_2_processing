# CPP_README —— 把 C/C++ 代码嵌入 Processing 的完整指南

本指南讲解如何在本模板的基础上，把你自己的 C/C++ 代码接入 Processing，实现**新建函数**和**互动内容**。读完后你应该能：

- 理解 Processing → Java → JNI → C++ 三层调用链；
- 照着模板新增一个自己的 native 函数；
- 在 Processing sketch 里调用它，并把鼠标/键盘等交互传进 C++；
- 知道两种典型模式（纯计算 / 原生 OpenGL 绘制）该选哪种。

---

## 0. 三层架构总览

```
┌─────────────────────────────────────────────┐
│  Processing sketch  (.pde)                  │   你写的 sketch：setup()/draw()/mouseX...
│    import org.example.cpplib.*;             │
│    Compute cpp = new Compute(this);         │
│    cpp.mandelbrot(pixels, ...);             │
└──────────────────┬──────────────────────────┘
                   │  Java 方法调用
┌──────────────────▼──────────────────────────┐
│  Java 库层 (src/main/java/...)              │   对外 API（Compute / GLView）
│    - 持有一个 long 句柄指向 C++ 对象         │
│    - registerMethod("dispose", ...) 自动释放│
│    - NativeCore.ensureLoaded() 加载 .so/.dll│
└──────────────────┬──────────────────────────┘
                   │  JNI (native 方法)
┌──────────────────▼──────────────────────────┐
│  C++ 原生层 (cpp/src/)                      │   真正干活的代码
│    bridge.cpp    JNI 导出函数                │
│    engine.cpp    计算引擎（持有状态）        │
│    mandelbrot.cpp  具体算法                  │
│    gl_view.cpp   OpenGL 绘制                 │
└─────────────────────────────────────────────┘
```

关键点：

1. **C++ 对象的生命周期在 C++ 侧管理**，Java 侧只存一个 `long` 句柄（指针转成整数）。创建时 `new`，销毁时 `delete`。
2. **Java 不直接调用 C++**，而是通过 `native` 方法声明，由 JVM 在运行时把调用转给同名的 C 函数（`Java_包名_类名_方法名`）。
3. **数据通过数组/基本类型传递**。大块数据（像素、顶点）用 `int[]` / `float[]`，JNI 侧用 `GetIntArrayElements` 拿到指针，算完 `ReleaseIntArrayElements` 写回。

---

## 1. 两种模式，先选对

本模板演示了两种把 C++ 接进 Processing 的方式，绝大多数需求都能归入其中之一：

### 模式 A：纯计算（Compute）

**适用场景**：C++ 负责算，结果以数据形式（像素数组、数值数组）拿回 Java，由 Processing 负责显示。

**例子**：`Compute` 类 + `CppComputeMandelbrot` 示例。

**流程**：

```
sketch: img.loadPixels()
  → cpp.mandelbrot(img.pixels, w, h, ...)     // Java 调 native
    → JNI: GetIntArrayElements(rgb)           // 拿到 C++ 可写的指针
      → C++ mandelbrotCpu(out, ...)           // 直接写像素
    → JNI: ReleaseIntArrayElements(rgb, ..., 0)  // 写回 Java
  → img.updatePixels()
  → image(img, 0, 0, width, height)
```

**优点**：和 Processing 渲染解耦，C++ 只管算，显示逻辑全在 sketch 里。适合图像处理、矩阵运算、FFT、AI 推理等。

**代价**：数据要从 Java 堆拷贝到 native（`GetIntArrayElements` 可能拷贝），超大数组会有开销。

### 模式 B：原生 OpenGL 绘制（GLView）

**适用场景**：C++ 直接在 Processing 的 OpenGL 上下文里画东西（着色器、VBO、纹理），结果直接上屏，不经过 Java 数据拷贝。

**例子**：`GLView` 类 + `CppGLTriangle` 示例。

**流程**：

```
sketch: size(800, 600, P2D)    // 必须用 OpenGL 渲染器
  → gl.render(millis()/1000.0)  // 在 draw() 最后调用
    → parent.g.flush()           // 先把 Processing 本帧的几何画完
    → nativeRenderGL(handle, ...)
      → C++ 侧用 dlopen/dlsym 拿到 GL 函数指针
      → 编译着色器、上传 VBO、glDrawArrays
```

**优点**：零数据拷贝，性能最高；可以做 CUDA-OpenGL 互操作（GPU 算完直接画）。

**代价**：必须用 `P2D`/`P3D` 渲染器；C++ 侧要自己处理 OpenGL 状态，容易和 Processing 的状态冲突。

> 怎么选？**90% 的情况选模式 A**，简单、好调试、不挑渲染器。只有当你确实需要最高性能（每帧百万级顶点、GPU 计算+渲染）时才上模式 B。

---

## 2. 手把手：新增一个 native 函数

下面以"给 Processing 加一个 C++ 版的快速高斯模糊"为例，走一遍完整流程。

### 步骤 2.1：在 NativeCore.java 声明 native 方法

打开 `src/main/java/org/example/cpplib/NativeCore.java`，在合适的位置加：

```java
/**
 * 对 rgb 数组做一次 3x3 高斯模糊（原地修改）。
 * @param rgb  ARGB 像素数组，长度 w*h
 */
static native void nativeGaussianBlur(long engine, int[] rgb, int w, int h);
```

> 命名规则：`native` + 功能名。参数里第一个 `long engine` 是 C++ 对象句柄（如果你不需要跨调用保持状态，可以不要这个参数，做成纯函数）。

### 步骤 2.2：重新生成 JNI 头

运行一键构建脚本（它会自动执行 `javac -h`）：

```bash
bash tools/build.sh           # Linux
# 或
powershell -ExecutionPolicy Bypass -File tools\build.ps1   # Windows
```

生成后，`native/include/org_example_cpplib_NativeCore.h` 里会多出一行：

```c
JNIEXPORT void JNICALL Java_org_example_cpplib_NativeCore_nativeGaussianBlur
  (JNIEnv *, jclass, jlong, jintArray, jint, jint);
```

### 步骤 2.3：在 bridge.cpp 实现 JNI 函数

打开 `cpp/src/bridge.cpp`，照着已有函数的样子加：

```cpp
extern "C" JNIEXPORT void JNICALL
Java_org_example_cpplib_NativeCore_nativeGaussianBlur(JNIEnv* env, jclass,
                                                      jlong engine,
                                                      jintArray rgb,
                                                      jint w, jint h) {
    if (rgb == nullptr || w <= 0 || h <= 0) return;

    jint* pixels = env->GetIntArrayElements(rgb, nullptr);
    if (pixels == nullptr) return;  // OutOfMemoryError pending

    // 调用你自己的 C++ 实现
    cpplib::gaussianBlur(
        reinterpret_cast<std::uint32_t*>(pixels), w, h);

    // 最后一个参数 0 = 把修改写回 Java 数组并释放 native 拷贝
    env->ReleaseIntArrayElements(rgb, pixels, 0);
}
```

### 步骤 2.4：在 C++ 侧写真正的算法

在 `cpp/src/` 下新建 `blur.h` / `blur.cpp`（或直接加到现有文件），并在 `CMakeLists.txt` 的 `add_library` 里加上新源文件：

```cpp
// cpp/src/blur.h
#pragma once
#include <cstdint>
namespace cpplib {
void gaussianBlur(std::uint32_t* rgb, int w, int h);
}
```

```cpp
// cpp/src/blur.cpp
#include "blur.h"
#include <vector>
namespace cpplib {
void gaussianBlur(std::uint32_t* rgb, int w, int h) {
    std::vector<std::uint32_t> tmp(w * h);
    // ... 3x3 高斯核卷积 ...
}
}
```

```cmake
# cpp/CMakeLists.txt 的 add_library 里追加 src/blur.cpp
add_library(cpplib SHARED
  src/bridge.cpp
  src/engine.cpp
  src/mandelbrot.cpp
  src/gl_loader.cpp
  src/gl_view.cpp
  src/blur.cpp          # ← 新增
)
```

### 步骤 2.5：在 Java 侧加一个对外方法

如果不需要跨帧状态，可以直接在 `Compute.java` 里加一个 public 方法转发到 native：

```java
public void gaussianBlur(int[] rgb, int w, int h) {
    NativeCore.nativeGaussianBlur(handle, rgb, w, h);
}
```

### 步骤 2.6：在 sketch 里用

```java
import org.example.cpplib.*;

Compute cpp;
PImage img;

void setup() {
  size(640, 480);
  cpp = new Compute(this);
  img = loadImage("some.png");
}

void draw() {
  img.loadPixels();
  cpp.gaussianBlur(img.pixels, img.width, img.height);   // ← C++ 模糊
  img.updatePixels();
  image(img, 0, 0);
}
```

完成。重新跑一次 `tools/build.sh`，重启 Processing，就能用了。

---

## 3. 数据类型速查（Java ↔ JNI ↔ C++）

| Java 类型 | JNI 类型 | C++ 侧获取方式 |
| --- | --- | --- |
| `int` | `jint` | 直接用 |
| `long` | `jlong` | 直接用（常用来存 C++ 指针） |
| `float` | `jfloat` | 直接用 |
| `double` | `jdouble` | 直接用 |
| `boolean` | `jboolean` | 直接用 |
| `String` | `jstring` | `env->GetStringUTFChars(str, nullptr)` → `ReleaseStringUTFChars` |
| `int[]` | `jintArray` | `env->GetIntArrayElements(arr, nullptr)` → `ReleaseIntArrayElements` |
| `float[]` | `jfloatArray` | `GetFloatArrayElements` / `ReleaseFloatArrayElements` |
| `byte[]` | `jbyteArray` | `GetByteArrayElements` |
| `Object[]` | `jobjectArray` | `GetObjectArrayElement`（逐个取，较慢） |

**数组操作要点**：

- `GetXxxArrayElements` 返回的指针**可能是拷贝**（取决于 JVM），用完必须 `ReleaseXxxArrayElements`。
- `ReleaseXxxArrayElements` 的最后一个参数：
  - `0` = 把 native 的修改写回 Java 并释放；
  - `JNI_ABORT` = 不写回，直接释放（只读场景用）；
  - `JNI_COMMIT` = 写回但不释放（很少用）。
- 如果只想读不想写，用 `JNI_ABORT` 可以避免一次拷贝。

---

## 4. 互动内容：把鼠标/键盘传进 C++

Processing 的交互（鼠标位置、按键）发生在 Java 侧，要让 C++ 响应，就把它们作为参数传进 native 方法。

### 例子：C++ 侧跟着鼠标画点

**Java 侧**（`Compute.java` 或新建类）：

```java
public void drawAt(int[] rgb, int w, int h, int mx, int my, int color) {
    NativeCore.nativeDrawAt(handle, rgb, w, h, mx, my, color);
}
```

**NativeCore.java**：

```java
static native void nativeDrawAt(long engine, int[] rgb, int w, int h,
                                int mx, int my, int color);
```

**bridge.cpp**：

```cpp
extern "C" JNIEXPORT void JNICALL
Java_org_example_cpplib_NativeCore_nativeDrawAt(JNIEnv* env, jclass, jlong,
    jintArray rgb, jint w, jint h, jint mx, jint my, jint color) {
    jint* p = env->GetIntArrayElements(rgb, nullptr);
    if (!p) return;
    // 在 (mx,my) 周围画一个 5x5 的色块
    for (int dy = -2; dy <= 2; ++dy)
      for (int dx = -2; dx <= 2; ++dx) {
        int x = mx + dx, y = my + dy;
        if (x >= 0 && x < w && y >= 0 && y < h)
            p[y * w + x] = color;
      }
    env->ReleaseIntArrayElements(rgb, p, 0);
}
```

**sketch**：

```java
void draw() {
  // 每帧把鼠标位置传给 C++ 画点
  img.loadPixels();
  cpp.drawAt(img.pixels, img.width, img.height, mouseX, mouseY, color(255, 0, 0));
  img.updatePixels();
  image(img, 0, 0);
}

void keyPressed() {
  if (key == 'c') {
    // 按 c 清屏 —— 也可以做成一个 nativeClear 方法
    java.util.Arrays.fill(img.pixels, color(0));
    img.updatePixels();
  }
}
```

> 对于连续交互（鼠标拖拽画线），通常让 C++ 持有一个画布缓冲区（`std::vector<uint32_t>`），sketch 每帧把它拷出来显示。这样历史轨迹不会丢。

---

## 5. 构建与测试工作流

日常开发的推荐循环：

1. **改 Java / C++ 代码**；
2. **跑一键构建**：`bash tools/build.sh`（会自动重编 Java、重生成 JNI 头、重编 C++、跑冒烟测试、重装到 sketchbook）；
3. **重启 Processing**（库改动后必须重启，否则加载的是旧 jar）；
4. **运行示例验证**。

如果你只改了 Java、没改 native 方法签名，可以跳过 C++ 编译加快循环：

```bash
bash tools/build.sh --skip-native
```

### 冒烟测试

`tools/jnitest/TestMain.java` 是一个不依赖 Processing 窗口的命令行测试，每次构建都会自动运行。它验证：

- 原生库能被加载；
- `nativeVersionString()` 返回正确版本；
- 计算引擎能创建、`info()` 有输出；
- Mandelbrot 计算产生合理数量的非黑像素。

你可以照着它的样子，把新写的 native 函数也加进测试，确保重构不破坏功能。

---

## 6. 常见坑

| 坑 | 现象 | 解决 |
| --- | --- | --- |
| 改了 native 方法签名没重编 C++ | `UnsatisfiedLinkError` | 重跑完整构建（`javac -h` 会更新头文件，C++ 要重编） |
| 忘了 `ReleaseXxxArrayElements` | 内存泄漏 / 数组不更新 | 每个 `Get` 必须配一个 `Release` |
| 在 C++ 里存了 Java 数组指针 | 下次调用时指针失效 / 崩溃 | 不要跨调用存 `GetXxxArrayElements` 返回的指针；要存就自己 `memcpy` 到 C++ 缓冲区 |
| GLView 没在 `draw()` 最后调用 | 原生内容被 Processing 背景盖住 | `gl.render(t)` 放在 `draw()` 最后一行 |
| 用了默认渲染器却调 `GLView` | 报错 "requires an OpenGL renderer" | `size(w, h, P2D)` 或 `P3D` |
| sketch 没重启就改了库 | 行为没变 / 旧代码 | 库更新后必须完全退出并重启 Processing |
| Linux 下 `libGL.so.1` 找不到 | GLView 示例报 `cannot open shared object file` | `sudo apt install libgl1-mesa-glx` 或装显卡驱动 |

---

## 7. 从模板改成你自己的库

要把这个模板变成你自己的库，需要改这些地方：

1. **Java 包名**：`src/main/java/org/example/cpplib/` → 你的包路径（如 `com/yourname/myplugin/`）；
2. **C++ JNI 函数名**：bridge.cpp 里所有 `Java_org_example_cpplib_NativeCore_*` → `Java_com_yourname_myplugin_NativeCore_*`（重生成头文件后自动对齐）；
3. **库名**：`build.gradle.kts` 里的 `libName`，`library.properties` / `release.properties` 里的 `name`；
4. **native 库文件名**：`NativeCore.java` 里 `System.mapLibraryName("cpplib")` 的 `"cpplib"` 改成你的库名（对应 `lib<name>.so` / `<name>.dll`）；
5. **示例**：`examples/` 下的 .pde 里 `import org.example.cpplib.*;` 改成你的包名。

改完跑一次完整构建，确认冒烟测试通过即可。

---

## 8. 下一步可以做什么

- **接 CUDA**：`cmake -DCPPLIB_USE_CUDA=ON`，在 `mandelbrot_cuda.cu` 的基础上加自己的 kernel；
- **接第三方 C++ 库**：在 `cpp/CMakeLists.txt` 里 `find_package(...)` + `target_link_libraries(cpplib PRIVATE ...)`（OpenCV、Eigen、ONNX Runtime…）；
- **CUDA-OpenGL 互操作**：在 GLView 的 C++ 侧用 `cudaGraphicsGLRegisterBuffer` 把 VBO 注册给 CUDA，GPU 算完直接画，零拷贝；
- **异步计算**：C++ 侧开 `std::thread` 或 `std::async`，Java 侧轮询结果，避免阻塞 draw()。

有任何扩展需求，先在 `ComputeEngine`（模式 A）或 `GLView`（模式 B）里加方法，再按第 2 节的流程暴露到 Java 即可。
