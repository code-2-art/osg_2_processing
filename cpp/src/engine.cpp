#include "engine.h"

#include "config.h"
#include "mandelbrot.h"

#include <sstream>
#include <thread>

#if defined(CPPLIB_USE_CUDA)
#include "mandelbrot_cuda.h"
#endif

namespace cpplib {

ComputeEngine::ComputeEngine() {
#if defined(CPPLIB_USE_CUDA)
    // CUDA 运行时是“懒初始化”的：这里只做能力探测，
    // 真正的 device 分配在第一次计算时发生。
    if (cuda::available()) {
        cuda_state_ = reinterpret_cast<void*>(1);  // 占位标记
    }
#endif
}

ComputeEngine::~ComputeEngine() = default;

void ComputeEngine::mandelbrot(int w, int h, int maxIter,
                               double x0, double y0, double x1, double y1,
                               std::uint32_t* out) const {
#if defined(CPPLIB_USE_CUDA)
    if (cuda_state_ != nullptr && cuda::mandelbrot(out, w, h, maxIter, x0, y0, x1, y1) == 0) {
        return;  // GPU 路径成功
    }
    // GPU 失败时回退 CPU，保证 demo 依然可用
#endif
    mandelbrotCpu(out, w, h, maxIter, x0, y0, x1, y1);
}

std::string ComputeEngine::info() const {
    std::ostringstream os;
    os << "cpplib " << CPPLIB_VERSION;
#if defined(CPPLIB_USE_CUDA)
    if (cuda_state_ != nullptr) {
        os << " | backend=CUDA | " << cuda::deviceName();
    } else {
        os << " | backend=CPU(fallback, CUDA unavailable)";
    }
#else
    os << " | backend=CPU";
#endif
    os << " | hw_threads=" << std::thread::hardware_concurrency();
    return os.str();
}

}  // namespace cpplib
