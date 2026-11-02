#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <thread>
#include <vector>

namespace inv {

/**
 * @brief Minimal dynamic-scheduling parallel_for over [0, n).
 * Falls back to a serial loop for tiny ranges or single-core hosts.
 * `f(i)` must be safe to call concurrently for distinct i.
 */
template <typename F>
void parallel_for(std::size_t n, F&& f) {
    const std::size_t hw = std::max<std::size_t>(1, std::thread::hardware_concurrency());
    const std::size_t nt = std::min(hw, n);
    if (nt <= 1) {
        for (std::size_t i = 0; i < n; ++i) f(i);
        return;
    }
    std::atomic<std::size_t> next{0};
    auto worker = [&]() {
        for (;;) {
            const std::size_t i = next.fetch_add(1, std::memory_order_relaxed);
            if (i >= n) break;
            f(i);
        }
    };
    std::vector<std::thread> pool;
    pool.reserve(nt - 1);
    for (std::size_t t = 1; t < nt; ++t) pool.emplace_back(worker);
    worker();
    for (auto& th : pool) th.join();
}

} // namespace inv
