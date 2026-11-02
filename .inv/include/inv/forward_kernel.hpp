#pragma once

#include "point.hpp"
#include "voxel_grid.hpp"
#include <vector>
#include <cmath>
#include <cstddef>
#include <algorithm>

namespace inv {

/**
 * @brief Gravitational & Potential Field Forward Operator.
 * Implements exact analytical right-rectangular prism equations (Nagy, 1966).
 */
template <typename T = double>
class ForwardKernel {
public:
    // Gravitational constant in SI: m^3 / (kg * s^2)
    static constexpr T G_SI = static_cast<T>(6.67430e-11);
    
    // Scale factor to produce gravity anomaly in mGal given coordinates in meters and density in g/cm^3
    // 1 g/cm^3 = 1000 kg/m^3. 1 mGal = 1e-5 m/s^2.
    // G_SI * 1000 / 1e-5 = 6.67430e-11 * 1e8 = 6.67430e-3
    static constexpr T G_MGAL = static_cast<T>(6.67430e-3);

    /**
     * @brief Computes vertical gravitational attraction gz (in mGal) of a single rectangular prism.
     * @param obs Observation station position (x, y, z) in meters.
     * @param p_min Prism minimum corner (x1, y1, z1) in meters.
     * @param p_max Prism maximum corner (x2, y2, z2) in meters.
     * @param density_contrast Density contrast in g/cm^3.
     */
    static T prism_gravity_gz(const Point3D<T>& obs,
                              const Point3D<T>& p_min,
                              const Point3D<T>& p_max,
                              T density_contrast) noexcept
    {
        if (std::abs(density_contrast) < static_cast<T>(1e-15)) {
            return T{0};
        }

        const T x[2] = {p_min.x - obs.x, p_max.x - obs.x};
        const T y[2] = {p_min.y - obs.y, p_max.y - obs.y};
        const T z[2] = {p_min.z - obs.z, p_max.z - obs.z};

        T sum = T{0};

        for (int i = 0; i < 2; ++i) {
            const T u = x[i];
            const T u2 = u * u;
            const T sign_i = (i == 0) ? static_cast<T>(-1) : static_cast<T>(1);

            for (int j = 0; j < 2; ++j) {
                const T v = y[j];
                const T v2 = v * v;
                const T sign_j = (j == 0) ? static_cast<T>(-1) : static_cast<T>(1);

                for (int k = 0; k < 2; ++k) {
                    const T w = z[k];
                    const T sign_k = (k == 0) ? static_cast<T>(-1) : static_cast<T>(1);
                    const T s = sign_i * sign_j * sign_k;

                    const T r = std::sqrt(u2 + v2 + w * w);
                    if (r < static_cast<T>(1e-12)) {
                        continue;
                    }

                    T term1 = T{0};
                    if (r + v > static_cast<T>(1e-14)) {
                        term1 = u * std::log(r + v);
                    } else if (r - v > static_cast<T>(1e-14)) {
                        term1 = -u * std::log(r - v);
                    }

                    T term2 = T{0};
                    if (r + u > static_cast<T>(1e-14)) {
                        term2 = v * std::log(r + u);
                    } else if (r - u > static_cast<T>(1e-14)) {
                        term2 = -v * std::log(r - u);
                    }

                    T term3 = T{0};
                    if (std::abs(w) > static_cast<T>(1e-14)) {
                        const T arg = (u * v) / (w * r);
                        term3 = -w * std::atan(arg);
                    }

                    sum += s * (term1 + term2 + term3);
                }
            }
        }

        // Nagy formula with downward-positive gravity anomaly convention:
        // A mass below the surface produces a positive downward gravitational anomaly gz.
        return -G_MGAL * density_contrast * sum;
    }

    /**
     * @brief Computes the full gravity anomaly gz (in mGal) across a grid of observation points.
     */
    static void compute_gravity_field(const VoxelGrid<T>& grid,
                                      const std::vector<Point3D<T>>& obs_pts,
                                      std::vector<T>& out_gz) noexcept
    {
        out_gz.assign(obs_pts.size(), T{0});
        const std::size_t n_cells = grid.total_cells();

        Point3D<T> p_min, p_max;

        for (std::size_t c = 0; c < n_cells; ++c) {
            const T rho = grid[c];
            if (std::abs(rho) < static_cast<T>(1e-12)) {
                continue;
            }

            std::size_t ix, iy, iz;
            grid.decode_index(c, ix, iy, iz);
            grid.cell_bounds(ix, iy, iz, p_min, p_max);

            for (std::size_t i = 0; i < obs_pts.size(); ++i) {
                out_gz[i] += prism_gravity_gz(obs_pts[i], p_min, p_max, rho);
            }
        }
    }

    /**
     * @brief Computes a row or column of the linear sensitivity matrix G.
     * G[i, j] represents gz at obs_i due to cell j with unit density contrast (1.0 g/cm^3).
     */
    static T compute_sensitivity_element(const Point3D<T>& obs,
                                         const Point3D<T>& p_min,
                                         const Point3D<T>& p_max) noexcept
    {
        return prism_gravity_gz(obs, p_min, p_max, static_cast<T>(1.0));
    }
};

} // namespace inv
