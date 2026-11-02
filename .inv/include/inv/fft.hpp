#pragma once

#include <complex>
#include <cstddef>
#include <cmath>
#include <numbers>
#include <utility>
#include <vector>

namespace inv {

/// Smallest power of two >= n (n >= 1).
[[nodiscard]] inline std::size_t next_pow2(std::size_t n) noexcept {
    std::size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

/**
 * @brief In-place iterative radix-2 FFT of a fixed power-of-two length.
 * Unnormalised in both directions (callers apply 1/N after an inverse transform).
 */
class Fft1D {
public:
    using cd = std::complex<double>;

    explicit Fft1D(std::size_t n) : n_(n), rev_(n), tw_(n > 1 ? n / 2 : 1) {
        std::size_t bits = 0;
        while ((std::size_t{1} << bits) < n_) ++bits;
        for (std::size_t i = 0; i < n_; ++i) {
            std::size_t r = 0;
            for (std::size_t b = 0; b < bits; ++b) {
                if (i & (std::size_t{1} << b)) r |= std::size_t{1} << (bits - 1 - b);
            }
            rev_[i] = r;
        }
        for (std::size_t k = 0; k < tw_.size(); ++k) {
            const double a = -2.0 * std::numbers::pi * static_cast<double>(k) / static_cast<double>(n_);
            tw_[k] = cd(std::cos(a), std::sin(a));
        }
    }

    [[nodiscard]] std::size_t size() const noexcept { return n_; }

    void transform(cd* a, bool inverse) const {
        if (n_ <= 1) return;
        for (std::size_t i = 0; i < n_; ++i) {
            if (i < rev_[i]) std::swap(a[i], a[rev_[i]]);
        }
        for (std::size_t len = 2; len <= n_; len <<= 1) {
            const std::size_t half = len >> 1;
            const std::size_t step = n_ / len;
            for (std::size_t i = 0; i < n_; i += len) {
                for (std::size_t j = 0; j < half; ++j) {
                    cd w = tw_[j * step];
                    if (inverse) w = std::conj(w);
                    const cd u = a[i + j];
                    const cd v = a[i + j + half] * w;
                    a[i + j] = u + v;
                    a[i + j + half] = u - v;
                }
            }
        }
    }

private:
    std::size_t n_;
    std::vector<std::size_t> rev_;
    std::vector<cd> tw_;
};

/**
 * @brief 2D FFT on a row-major array indexed [ix * ny + iy] (both sizes powers of two).
 * inverse() includes the 1/(nx*ny) normalisation, so inverse(forward(a)) == a.
 */
class Fft2D {
public:
    using cd = std::complex<double>;

    Fft2D(std::size_t nx, std::size_t ny) : nx_(nx), ny_(ny), fx_(nx), fy_(ny) {}

    [[nodiscard]] std::size_t nx() const noexcept { return nx_; }
    [[nodiscard]] std::size_t ny() const noexcept { return ny_; }

    void forward(std::vector<cd>& a) const { run(a, false); }
    void inverse(std::vector<cd>& a) const { run(a, true); }

private:
    void run(std::vector<cd>& a, bool inverse) const {
        for (std::size_t ix = 0; ix < nx_; ++ix) {
            fy_.transform(a.data() + ix * ny_, inverse);
        }
        std::vector<cd> col(nx_);
        for (std::size_t iy = 0; iy < ny_; ++iy) {
            for (std::size_t ix = 0; ix < nx_; ++ix) col[ix] = a[ix * ny_ + iy];
            fx_.transform(col.data(), inverse);
            for (std::size_t ix = 0; ix < nx_; ++ix) a[ix * ny_ + iy] = col[ix];
        }
        if (inverse) {
            const double s = 1.0 / static_cast<double>(nx_ * ny_);
            for (auto& v : a) v *= s;
        }
    }

    std::size_t nx_, ny_;
    Fft1D fx_, fy_;
};

} // namespace inv
