#pragma once

#include <cstdint>

namespace cpplib {

// 纯 CPU（多线程）Mandelbrot 计算
void mandelbrotCpu(std::uint32_t* out, int w, int h, int maxIter,
                   double x0, double y0, double x1, double y1);

}  // namespace cpplib
