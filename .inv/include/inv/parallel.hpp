#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <thread>
#include <vector>

namespace inv {
namespace detail {

#if defined(__cpp_lib_jthread) && __cpp_lib_jthread >= 201911L
using jthread_type = std::jthread;
#else
// RAII thread wrapper for platforms/compilers where libc++ lacks std::jthread (e.g. Apple Clang on macOS)
class jthread_type {
    std::thread m_thread;
public:
    jthread_type() noexcept = default;
    template <typename Callable, typename... Args>
    explicit jthread_type(Callable&& f, Args&&... args)
        : m_thread(std::forward<Callable>(f), std::forward<Args>(args)...) {}
    ~jthread_type() {
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }
    jthread_type(const jthread_type&) = delete;
    jthread_type& operator=(const jthread_type&) = delete;
    jthread_type(jthread_type&& other) noexcept : m_thread(std::move(other.m_thread)) {}
    jthread_type& operator=(jthread_type&& other) noexcept {
        if (this != &other) {
            if (m_thread.joinable()) {
                m_thread.join();
            }
            m_thread = std::move(other.m_thread);
        }
        return *this;
    }
    bool joinable() const noexcept { return m_thread.joinable(); }
    void join() { m_thread.join(); }
    void request_stop() noexcept {}
};
#endif

} // namespace detail

using detail::jthread_type;
using jthread = detail::jthread_type;

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
    std::vector<jthread_type> pool;
    pool.reserve(nt - 1);
    for (std::size_t t = 1; t < nt; ++t) pool.emplace_back(worker);
    worker();
    for (auto& th : pool) th.join();
}

} // namespace inv
