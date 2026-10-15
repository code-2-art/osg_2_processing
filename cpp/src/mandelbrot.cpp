#include "mandelbrot.h"

#include <algorithm>
#include <cmath>
#include <thread>
#include <vector>

namespace cpplib {

namespace {

// 迭代次数 -> 0xAARRGGBB 调色板。
// 注意：这个函数与 mandelbrot_cuda.cu 里的 device 版本保持同样的算法，
// 以便 CPU / GPU 结果观感一致。
inline std::uint32_t iterColor(int it, int maxIter) {
    if (it >= maxIter) {
        return 0xFF000000u;  // 集合内部：黑色
    }
    const double t = static_cast<double>(it) / static_cast<double>(maxIter);
    const double twoPi = 6.28318530717958647692;
    auto chan = [&](double phase) -> unsigned {
        const double v = 0.5 + 0.5 * std::sin(twoPi * t + phase);
        return static_cast<unsigned>(v * 255.0 + 0.5) & 0xFFu;
    };
    const unsigned r = chan(0.0);
    const unsigned g = chan(2.09439510239319549230);  // 2*pi/3
    const unsigned b = chan(4.18879020478639098460);  // 4*pi/3
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

// 计算 [row0, row1) 之间的行
void mandelRows(std::uint32_t* out, int w, int h, int maxIter,
                double x0, double y0, double x1, double y1,
                int row0, int row1) {
    for (int y = row0; y < row1; ++y) {
        const double ci = y0 + (y + 0.5) / static_cast<double>(h) * (y1 - y0);
        std::uint32_t* row = out + static_cast<std::size_t>(y) * w;
        for (int x = 0; x < w; ++x) {
            const double cr = x0 + (x + 0.5) / static_cast<double>(w) * (x1 - x0);
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
            row[x] = iterColor(it, maxIter);
        }
    }
}

}  // namespace

void mandelbrotCpu(std::uint32_t* out, int w, int h, int maxIter,
                   double x0, double y0, double x1, double y1) {
    if (out == nullptr || w <= 0 || h <= 0) {
        return;
    }

    const unsigned hw = std::max(1u, std::thread::hardware_concurrency());
    // 行数太少时没必要开那么多线程
    const unsigned threads = static_cast<unsigned>(std::min<std::size_t>(hw, h));

    std::vector<std::thread> pool;
    pool.reserve(threads);
    for (unsigned t = 0; t < threads; ++t) {
        const int row0 = static_cast<int>(static_cast<std::size_t>(h) * t / threads);
        const int row1 = static_cast<int>(static_cast<std::size_t>(h) * (t + 1) / threads);
        pool.emplace_back([&, row0, row1]() {
            mandelRows(out, w, h, maxIter, x0, y0, x1, y1, row0, row1);
        });
    }
    for (auto& th : pool) {
        th.join();
    }
}

}  // namespace cpplib
