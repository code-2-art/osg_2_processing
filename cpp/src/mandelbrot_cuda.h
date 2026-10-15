#pragma once

// 这个头文件只在 CPPLIB_USE_CUDA 时被包含。
// 声明由 mandelbrot_cuda.cu 实现的 CUDA 后端接口。

#include <cstdint>
#include <string>

namespace cpplib {
namespace cuda {

// CUDA runtime 是否可用（有可用的 NVIDIA 设备）
bool available();

// 例如 "NVIDIA GeForce RTX 4090"
std::string deviceName();

// GPU 计算 Mandelbrot。成功返回 0，失败返回非 0。
// out 为 host 内存，长度 >= w*h。
int mandelbrot(std::uint32_t* out, int w, int h, int maxIter,
               double x0, double y0, double x1, double y1);

}  // namespace cuda
}  // namespace cpplib
