#pragma once

#include "voxel_grid.hpp"
#include <vector>
#include <cmath>
#include <cstddef>
#include <numeric>

namespace inv {

/**
 * @brief Zhdanov & Portniaguine Focusing Stabilizers and Metrics.
 * Computes Minimum Support (MS), Minimum Gradient Support (MGS),
 * and dynamic IRLS focusing weights.
 *
 * NOTE: compute_minimum_support / compute_minimum_gradient_support are the LEGACY
 * lattice-dependent (cell-count, central-difference) forms kept for compatibility.
 * The inversion engine uses compute_minimum_support_volume /
 * compute_minimum_gradient_support_area (physical units, face based).
 *
 * References:
 * - Portniaguine, O. & Zhdanov, M. S. (1999). Focusing geophysical inversion images. Geophysics.
 * - Zhdanov, M. S. (2002). Geophysical Inverse Theory and Regularization Problems. Elsevier.
 */
template <typename T = double>
class FocusingEvaluator {
public:
    /**
     * @brief Computes Zhdanov Minimum Support (MS) functional.
     * S_MS(m) = sum_j (m_j - m0_j)^2 / ((m_j - m0_j)^2 + e^2)
     * Measures the compact volume occupied by the anomalous body.
     *
     * @param grid The 3D voxel grid containing property values.
     * @param e Focusing parameter (regularization threshold, default 1e-3).
     * @param prior_val Baseline background value (m0, default 0).
     */
    static T compute_minimum_support(const VoxelGrid<T>& grid,
                                     T e = static_cast<T>(1e-3),
                                     T prior_val = T{0}) noexcept
    {
        const T e2 = e * e;
        T sum = T{0};
        const std::size_t n = grid.total_cells();

        for (std::size_t i = 0; i < n; ++i) {
            const T diff = grid[i] - prior_val;
            const T diff2 = diff * diff;
            sum += diff2 / (diff2 + e2);
        }

        return sum;
    }

    /**
     * @brief Computes Zhdanov Minimum Gradient Support (MGS) functional.
     * S_MGS(m) = sum_j ||grad m_j||^2 / (||grad m_j||^2 + e_grad^2)
     * Measures the spatial area/extent of property discontinuities (sharp boundaries).
     *
     * @param grid The 3D voxel grid.
     * @param e_grad Gradient focusing parameter (default 1e-3).
     */
    static T compute_minimum_gradient_support(const VoxelGrid<T>& grid,
                                              T e_grad = static_cast<T>(1e-3)) noexcept
    {
        const T e2 = e_grad * e_grad;
        T sum = T{0};

        const std::size_t nx = grid.nx();
        const std::size_t ny = grid.ny();
        const std::size_t nz = grid.nz();

        const T inv_2dx = static_cast<T>(1.0) / (static_cast<T>(2.0) * grid.dx());
        const T inv_2dy = static_cast<T>(1.0) / (static_cast<T>(2.0) * grid.dy());
        const T inv_2dz = static_cast<T>(1.0) / (static_cast<T>(2.0) * grid.dz());

        for (std::size_t ix = 0; ix < nx; ++ix) {
            const std::size_t ix_prev = (ix == 0) ? 0 : ix - 1;
            const std::size_t ix_next = (ix + 1 >= nx) ? nx - 1 : ix + 1;

            for (std::size_t iy = 0; iy < ny; ++iy) {
                const std::size_t iy_prev = (iy == 0) ? 0 : iy - 1;
                const std::size_t iy_next = (iy + 1 >= ny) ? ny - 1 : iy + 1;

                for (std::size_t iz = 0; iz < nz; ++iz) {
                    const std::size_t iz_prev = (iz == 0) ? 0 : iz - 1;
                    const std::size_t iz_next = (iz + 1 >= nz) ? nz - 1 : iz + 1;

                    const T gx = (grid(ix_next, iy, iz) - grid(ix_prev, iy, iz)) * inv_2dx;
                    const T gy = (grid(ix, iy_next, iz) - grid(ix, iy_prev, iz)) * inv_2dy;
                    const T gz = (grid(ix, iy, iz_next) - grid(ix, iy, iz_prev)) * inv_2dz;

                    const T grad2 = gx * gx + gy * gy + gz * gz;
                    sum += grad2 / (grad2 + e2);
                }
            }
        }

        return sum;
    }

    // ------------------------------------------------------------------
    // Resolution-invariant (physical) forms used by the inversion solver.
    //
    // The legacy forms above count CELLS and use central differences, so their value
    // depends on the lattice (a 4x coarser level "sees" a different number). The forms
    // below are expressed in physical units and are comparable across coarse-to-fine levels
    // and between C++ and Python:
    //
    //   S_MS_vol  = sum_j  V_j * x_j^2 / (x_j^2 + e^2),            x_j = m_j - m0_j      [m^3]
    //   S_MGS_area= sum_f  A_f * D_f^2 / (D_f^2 + e_jump^2),       D_f = m_b - m_a       [m^2]
    //
    // where f runs over interior cell FACES (a, b are the two cells sharing the face).
    // The face form is the discrete analogue of "area of the surface where m is discontinuous"
    // (a one-cell sharp contact counts its area once; an n-cell smooth ramp counts ~n times).
    // It equals Zhdanov's e_grad form with e_grad = e_jump / h. An optional active mask
    // restricts both sums to active cells (faces need both neighbours active).
    // ------------------------------------------------------------------

    /// Domain volume of active cells [m^3].
    static T active_volume(const VoxelGrid<T>& grid, const std::vector<char>* active = nullptr) noexcept {
        if (!active) return grid.cell_volume() * static_cast<T>(grid.total_cells());
        std::size_t n = 0;
        for (char a : *active) n += a ? 1 : 0;
        return grid.cell_volume() * static_cast<T>(n);
    }

    /// Total interior face area between pairs of active cells [m^2].
    static T interface_area(const VoxelGrid<T>& grid, const std::vector<char>* active = nullptr) noexcept {
        T area = T{0};
        for_each_face(grid, active, [&](std::size_t, std::size_t, T a) { area += a; });
        return area;
    }

    /// Volume-weighted Minimum Support [m^3].
    static T compute_minimum_support_volume(const VoxelGrid<T>& grid, T e, T prior_val = T{0},
                                            const std::vector<char>* active = nullptr) noexcept {
        const T e2 = e * e;
        const T v = grid.cell_volume();
        T sum = T{0};
        for (std::size_t i = 0; i < grid.total_cells(); ++i) {
            if (active && !(*active)[i]) continue;
            const T x = grid[i] - prior_val;
            sum += v * (x * x) / (x * x + e2);
        }
        return sum;
    }

    /// Area-weighted face-based Minimum Gradient Support [m^2].
    static T compute_minimum_gradient_support_area(const VoxelGrid<T>& grid, T e_jump,
                                                   const std::vector<char>* active = nullptr) noexcept {
        const T e2 = e_jump * e_jump;
        T sum = T{0};
        for_each_face(grid, active, [&](std::size_t a, std::size_t b, T area) {
            const T d = grid[b] - grid[a];
            sum += area * (d * d) / (d * d + e2);
        });
        return sum;
    }

    /// Visits each interior face once: f(cell_a, cell_b, face_area).
    template <typename F>
    static void for_each_face(const VoxelGrid<T>& grid, const std::vector<char>* active, F&& f) noexcept {
        const std::size_t nx = grid.nx(), ny = grid.ny(), nz = grid.nz();
        const T ax = grid.dy() * grid.dz();
        const T ay = grid.dx() * grid.dz();
        const T az = grid.dx() * grid.dy();
        for (std::size_t ix = 0; ix < nx; ++ix) {
            for (std::size_t iy = 0; iy < ny; ++iy) {
                for (std::size_t iz = 0; iz < nz; ++iz) {
                    const std::size_t a = grid.index(ix, iy, iz);
                    if (active && !(*active)[a]) continue;
                    if (ix + 1 < nx) {
                        const std::size_t b = grid.index(ix + 1, iy, iz);
                        if (!active || (*active)[b]) f(a, b, ax);
                    }
                    if (iy + 1 < ny) {
                        const std::size_t b = grid.index(ix, iy + 1, iz);
                        if (!active || (*active)[b]) f(a, b, ay);
                    }
                    if (iz + 1 < nz) {
                        const std::size_t b = grid.index(ix, iy, iz + 1);
                        if (!active || (*active)[b]) f(a, b, az);
                    }
                }
            }
        }
    }

    /**
     * @brief Computes the diagonal IRLS focusing weights W_e:
     * w_j = 1 / sqrt((m_j - m0_j)^2 + e^2)
     */
    static void compute_irls_weights(const VoxelGrid<T>& grid,
                                     std::vector<T>& out_weights,
                                     T e = static_cast<T>(1e-3),
                                     T prior_val = T{0}) noexcept
    {
        const T e2 = e * e;
        const std::size_t n = grid.total_cells();
        out_weights.resize(n);

        for (std::size_t i = 0; i < n; ++i) {
            const T diff = grid[i] - prior_val;
            out_weights[i] = static_cast<T>(1.0) / std::sqrt(diff * diff + e2);
        }
    }
};

} // namespace inv
