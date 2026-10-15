#pragma once

#include <cstdint>
#include <string>

namespace cpplib {

// ------------------------------------------------------------------
// ComputeEngine：在 C++ 侧长期存活的计算引擎。
// Java 侧 Compute 类持有它（long handle），引擎对象在整个 sketch 生命周期内
// 只创建/销毁一次 —— 这是接入 CUDA / AI 推理引擎（初始化上下文、加载模型等）
// 的推荐挂载点。
// ------------------------------------------------------------------
class ComputeEngine {
public:
    ComputeEngine();
    ~ComputeEngine();

    ComputeEngine(const ComputeEngine&) = delete;
    ComputeEngine& operator=(const ComputeEngine&) = delete;

    // 计算 Mandelbrot 集合。CPU 后端用多线程，CUDA 后端用 GPU kernel。
    // out 长度必须 >= w*h。
    void mandelbrot(int w, int h, int maxIter,
                    double x0, double y0, double x1, double y1,
                    std::uint32_t* out) const;

    // 构建/运行信息，例如 "cpplib 1.0.0 | backend=CUDA | device=RTX 4090"
    std::string info() const;

private:
    void* cuda_state_ = nullptr;  // CUDA 后端专用状态（预留扩展点）
};

}  // namespace cpplib
