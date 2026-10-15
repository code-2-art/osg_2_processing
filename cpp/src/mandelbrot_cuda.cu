// ============================================================
// CUDA 后端：Mandelbrot 计算 kernel。
// 本文件仅在 cmake 时加 -DCPPLIB_USE_CUDA=ON 才参与编译。
// 它演示了把一段“热循环”搬到 GPU 的标准姿势：
//   1. 写 __global__ kernel
//   2. host 函数里 cudaMalloc / 拷入参数 / 启动 / 拷回结果
// 你的真实算法（矩阵乘、FFT、AI 推理前处理等）都可以套同样的骨架。
// ============================================================

#include "mandelbrot_cuda.h"

#include <cstdio>
#include <cuda_runtime.h>

namespace cpplib {
namespace cuda {

namespace {

// 与 CPU 版保持一致的调色板（device 版本）
__device__ inline unsigned int iterColor(int it, int maxIter) {
    if (it >= maxIter) {
        return 0xFF000000u;
    }
    const double t = static_cast<double>(it) / static_cast<double>(maxIter);
    const double twoPi = 6.28318530717958647692;
    auto chan = [](double v) -> unsigned {
        return (static_cast<unsigned>(v * 255.0 + 0.5)) & 0xFFu;
    };
    unsigned r = chan(0.5 + 0.5 * sin(twoPi * t + 0.0));
    unsigned g = chan(0.5 + 0.5 * sin(twoPi * t + 2.0943951023931953));
    unsigned b = chan(0.5 + 0.5 * sin(twoPi * t + 4.1887902047863905));
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

__global__ void mandelKernel(unsigned int* out, int w, int h, int maxIter,
                             double x0, double y0, double x1, double y1) {
    const int idx = blockIdx.x * blockDim.x + threadIdx.x;
    const int n = w * h;
    if (idx >= n) {
        return;
    }
    const int y = idx / w;
    const int x = idx % w;

    const double cr = x0 + (x + 0.5) / static_cast<double>(w) * (x1 - x0);
    const double ci = y0 + (y + 0.5) / static_cast<double>(h) * (y1 - y0);
    double zr = cr;
    double zi = ci;
    int it = 0;
    while (it < maxIter) {
        const double zr2 = zr * zr;
        const double zi2 = zi * zi;
        if (zr2 + zi2 > 4.0) {
            break;
        }
        zi = 2.0 * zr * zi + ci;
        zr = zr2 - zi2 + cr;
        ++it;
    }
    out[idx] = iterColor(it, maxIter);
}

}  // namespace

bool available() {
    int deviceCount = 0;
    if (cudaGetDeviceCount(&deviceCount) != cudaSuccess) {
        return false;
    }
    return deviceCount > 0;
}

std::string deviceName() {
    cudaDeviceProp prop;
    int dev = 0;
    if (cudaGetDevice(&dev) != cudaSuccess || cudaGetDeviceProperties(&prop, dev) != cudaSuccess) {
        return "NVIDIA GPU (device query failed)";
    }
    return std::string(prop.name);
}

int mandelbrot(std::uint32_t* out, int w, int h, int maxIter,
               double x0, double y0, double x1, double y1) {
    if (out == nullptr || w <= 0 || h <= 0) {
        return -1;
    }

    const int n = w * h;
    unsigned int* dev = nullptr;
    cudaError_t err = cudaMalloc(&dev, sizeof(unsigned int) * static_cast<std::size_t>(n));
    if (err != cudaSuccess) {
        std::fprintf(stderr, "cpplib[cu]: cudaMalloc failed: %s\n", cudaGetErrorString(err));
        return -1;
    }

    const int threads = 256;
    const int blocks = (n + threads - 1) / threads;
    mandelKernel<<<blocks, threads>>>(dev, w, h, maxIter, x0, y0, x1, y1);
    err = cudaGetLastError();
    if (err != cudaSuccess) {
        std::fprintf(stderr, "cpplib[cu]: kernel launch failed: %s\n", cudaGetErrorString(err));
        cudaFree(dev);
        return -1;
    }

    err = cudaMemcpy(out, dev, sizeof(unsigned int) * static_cast<std::size_t>(n),
                     cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        std::fprintf(stderr, "cpplib[cu]: cudaMemcpy failed: %s\n", cudaGetErrorString(err));
        cudaFree(dev);
        return -1;
    }

    cudaFree(dev);
    return 0;
}

}  // namespace cuda
}  // namespace cpplib
