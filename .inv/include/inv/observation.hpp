#pragma once

#include "gravity_operator.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace inv {

/**
 * @brief Observation resampling utilities.
 *
 * Internally the solver works with ONE station per cell column ("column data", index ix*ny+iy),
 * which is what makes the FFT forward operator possible. These helpers convert user data
 * (regular rasters or scattered stations) to that representation and between levels.
 *
 * Limitation (documented): scattered stations are bin-averaged into columns of the finest grid;
 * empty columns are filled by neighbour propagation, i.e. the data are effectively gridded
 * before inversion. Use a station spacing comparable to the cell size for best results.
 */
namespace observation {

/// Bilinear sample of a cell-centred raster src[iy*snx+ix] at normalised coords u,v in [0,1].
[[nodiscard]] inline double sample_raster(const std::vector<double>& src, std::size_t snx, std::size_t sny,
                                          double u, double v) noexcept {
    const double px = std::clamp(u * static_cast<double>(snx) - 0.5, 0.0, static_cast<double>(snx - 1));
    const double py = std::clamp(v * static_cast<double>(sny) - 0.5, 0.0, static_cast<double>(sny - 1));
    const std::size_t x0 = static_cast<std::size_t>(px);
    const std::size_t y0 = static_cast<std::size_t>(py);
    const std::size_t x1 = std::min(x0 + 1, snx - 1);
    const std::size_t y1 = std::min(y0 + 1, sny - 1);
    const double fx = px - static_cast<double>(x0);
    const double fy = py - static_cast<double>(y0);
    const double a = src[y0 * snx + x0] * (1 - fx) + src[y0 * snx + x1] * fx;
    const double b = src[y1 * snx + x0] * (1 - fx) + src[y1 * snx + x1] * fx;
    return a * (1 - fy) + b * fy;
}

/// Resamples a raster (index iy*snx+ix, covering the full domain) onto column data (index ix*ny+iy).
/// Each destination cell averages sub-samples of the bilinear interpolant (anti-aliased when coarsening).
inline void raster_to_columns(const std::vector<double>& src, std::size_t snx, std::size_t sny,
                              std::size_t nx, std::size_t ny, std::vector<double>& out) {
    out.assign(nx * ny, 0.0);
    const auto subs = [](std::size_t s, std::size_t d) {
        return std::clamp<std::size_t>(2 * ((s + d - 1) / d), 2, 16);
    };
    const std::size_t sx = subs(snx, nx), sy = subs(sny, ny);
    for (std::size_t ix = 0; ix < nx; ++ix) {
        for (std::size_t iy = 0; iy < ny; ++iy) {
            double acc = 0.0;
            for (std::size_t a = 0; a < sx; ++a) {
                const double u = (static_cast<double>(ix) + (static_cast<double>(a) + 0.5) / static_cast<double>(sx)) /
                                 static_cast<double>(nx);
                for (std::size_t b = 0; b < sy; ++b) {
                    const double v = (static_cast<double>(iy) + (static_cast<double>(b) + 0.5) / static_cast<double>(sy)) /
                                     static_cast<double>(ny);
                    acc += sample_raster(src, snx, sny, u, v);
                }
            }
            out[ix * ny + iy] = acc / static_cast<double>(sx * sy);
        }
    }
}

/// Bin-averages scattered stations into column data of grid `g`; fills empty columns by
/// neighbour propagation. Returns the number of columns that contained at least one station.
inline std::size_t scattered_to_columns(const std::vector<double>& x, const std::vector<double>& y,
                                        const std::vector<double>& v, const GridGeometry& g,
                                        std::vector<double>& out) {
    const std::size_t n = std::min({x.size(), y.size(), v.size()});
    std::vector<double> sum(g.columns(), 0.0);
    std::vector<std::size_t> cnt(g.columns(), 0);
    for (std::size_t i = 0; i < n; ++i) {
        const double fx = (x[i] - g.x0) / g.dx;
        const double fy = (y[i] - g.y0) / g.dy;
        if (fx < 0.0 || fy < 0.0 || fx >= static_cast<double>(g.nx) || fy >= static_cast<double>(g.ny)) continue;
        const std::size_t c = static_cast<std::size_t>(fx) * g.ny + static_cast<std::size_t>(fy);
        sum[c] += v[i];
        ++cnt[c];
    }
    out.assign(g.columns(), 0.0);
    std::vector<char> known(g.columns(), 0);
    std::size_t filled = 0;
    for (std::size_t c = 0; c < g.columns(); ++c) {
        if (cnt[c]) {
            out[c] = sum[c] / static_cast<double>(cnt[c]);
            known[c] = 1;
            ++filled;
        }
    }
    if (filled == 0) return 0;

    std::size_t remaining = g.columns() - filled;
    while (remaining > 0) {
        std::vector<std::size_t> newly;
        std::vector<double> newv;
        for (std::size_t ix = 0; ix < g.nx; ++ix) {
            for (std::size_t iy = 0; iy < g.ny; ++iy) {
                const std::size_t c = ix * g.ny + iy;
                if (known[c]) continue;
                double s = 0.0;
                int k = 0;
                if (ix > 0 && known[c - g.ny]) { s += out[c - g.ny]; ++k; }
                if (ix + 1 < g.nx && known[c + g.ny]) { s += out[c + g.ny]; ++k; }
                if (iy > 0 && known[c - 1]) { s += out[c - 1]; ++k; }
                if (iy + 1 < g.ny && known[c + 1]) { s += out[c + 1]; ++k; }
                if (k) { newly.push_back(c); newv.push_back(s / k); }
            }
        }
        if (newly.empty()) break;
        for (std::size_t i = 0; i < newly.size(); ++i) {
            out[newly[i]] = newv[i];
            known[newly[i]] = 1;
        }
        remaining -= newly.size();
    }
    return filled;
}

/// Block-average of column data (fine nx x ny) by integer factors -> (nx/fx x ny/fy).
inline void block_average_columns(const std::vector<double>& fine, std::size_t nx, std::size_t ny,
                                  std::size_t fx, std::size_t fy, std::vector<double>& out) {
    const std::size_t cx = nx / fx, cy = ny / fy;
    out.assign(cx * cy, 0.0);
    for (std::size_t ix = 0; ix < nx; ++ix) {
        for (std::size_t iy = 0; iy < ny; ++iy) {
            out[(ix / fx) * cy + (iy / fy)] += fine[ix * ny + iy];
        }
    }
    const double inv_n = 1.0 / static_cast<double>(fx * fy);
    for (auto& v : out) v *= inv_n;
}

/// Block-average of a 3D cell field by integer factors (restriction operator R).
inline void block_average_cells(const std::vector<double>& fine, const GridGeometry& gf,
                                std::size_t fx, std::size_t fy, std::size_t fz,
                                std::vector<double>& out) {
    const std::size_t cx = gf.nx / fx, cy = gf.ny / fy, cz = gf.nz / fz;
    out.assign(cx * cy * cz, 0.0);
    for (std::size_t ix = 0; ix < gf.nx; ++ix) {
        for (std::size_t iy = 0; iy < gf.ny; ++iy) {
            for (std::size_t iz = 0; iz < gf.nz; ++iz) {
                out[((ix / fx) * cy + (iy / fy)) * cz + (iz / fz)] += fine[gf.cell_index(ix, iy, iz)];
            }
        }
    }
    const double inv_n = 1.0 / static_cast<double>(fx * fy * fz);
    for (auto& v : out) v *= inv_n;
}

/// Piecewise-constant prolongation (injection) P: coarse -> fine, so that G_coarse = G_fine * P.
inline void prolongate_cells(const std::vector<double>& coarse, const GridGeometry& gc,
                             const GridGeometry& gf, std::vector<double>& out) {
    out.assign(gf.cells(), 0.0);
    const std::size_t fx = gf.nx / gc.nx, fy = gf.ny / gc.ny, fz = gf.nz / gc.nz;
    for (std::size_t ix = 0; ix < gf.nx; ++ix) {
        for (std::size_t iy = 0; iy < gf.ny; ++iy) {
            for (std::size_t iz = 0; iz < gf.nz; ++iz) {
                out[gf.cell_index(ix, iy, iz)] = coarse[gc.cell_index(ix / fx, iy / fy, iz / fz)];
            }
        }
    }
}

} // namespace observation
} // namespace inv
