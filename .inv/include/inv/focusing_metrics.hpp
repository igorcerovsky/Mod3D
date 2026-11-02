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
