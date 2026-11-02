#pragma once

#include "fft.hpp"
#include "forward_kernel.hpp"
#include "parallel.hpp"
#include "point.hpp"
#include "pfld/interface_mesh.hpp"

#include <algorithm>
#include <complex>
#include <cstddef>
#include <memory>
#include <vector>

namespace inv {

/**
 * @brief Regular Cartesian grid geometry shared by operators and solvers.
 * Cell storage order matches VoxelGrid: index = (ix * ny + iy) * nz + iz.
 * z points DOWN (depth). The observation plane lies above the top of the grid (z < z0).
 */
struct GridGeometry {
    std::size_t nx{0}, ny{0}, nz{0};
    double x0{0}, y0{0}, z0{0};
    double dx{1}, dy{1}, dz{1};

    [[nodiscard]] std::size_t cells() const noexcept { return nx * ny * nz; }
    [[nodiscard]] std::size_t columns() const noexcept { return nx * ny; }
    [[nodiscard]] std::size_t cell_index(std::size_t ix, std::size_t iy, std::size_t iz) const noexcept {
        return (ix * ny + iy) * nz + iz;
    }
    [[nodiscard]] double xc(std::size_t ix) const noexcept { return x0 + (static_cast<double>(ix) + 0.5) * dx; }
    [[nodiscard]] double yc(std::size_t iy) const noexcept { return y0 + (static_cast<double>(iy) + 0.5) * dy; }
    [[nodiscard]] double zc(std::size_t iz) const noexcept { return z0 + (static_cast<double>(iz) + 0.5) * dz; }
};

/**
 * @brief Linear forward operator d = G m (gravity gz in mGal per g/cm^3).
 * Observations are indexed by column (ix * ny + iy), one station per cell column.
 */
class LinearOperator {
public:
    virtual ~LinearOperator() = default;
    [[nodiscard]] virtual std::size_t rows() const noexcept = 0;
    [[nodiscard]] virtual std::size_t cols() const noexcept = 0;
    /// d = G m
    virtual void apply(const std::vector<double>& m, std::vector<double>& d) const = 0;
    /// g = G^T r
    virtual void apply_transpose(const std::vector<double>& r, std::vector<double>& g) const = 0;
    /// out[j] = sum_i G_ij^2 = diag(G^T G)
    virtual void column_norm_squared(std::vector<double>& out) const = 0;
    /// Optional hint: only cells flagged active will be non-zero in `m` (enables skipping work).
    virtual void set_active(const std::vector<char>& /*active*/) {}
};

/**
 * @brief Primary 3D potential field forward operator based on pfld::InterfaceMesh.
 *
 * Evaluates gravity gz using:
 *  1. Triangulated DEM topography facets (conforming to surface relief).
 *  2. Horizontal rectangular quad interfaces (shared between adjacent depth layers).
 *  3. Bottom boundary horizontal quad interfaces (z = z_max).
 *  4. Planar vertical facet elimination (n_z = 0 facets contribute identically zero to gz
 *     in Cartesian coordinates; skipping them avoids 66.7% of line integrals with exact precision).
 *
 * Represents G = K * D where D maps cell densities m to interface contrasts Delta_rho,
 * and K evaluates the Pohanka/Vlado line integrals for each facet at unit contrast.
 *
 * For moderate grid sizes (rows * cols <= max_dense_entries), G is assembled in parallel
 * once and stored as a dense matrix for maximum CG/IRLS efficiency.
 * When rows * cols > max_dense_entries, operates matrix-free directly on the pfld facets.
 */
class PfldInterfaceMeshOperator final : public LinearOperator {
public:
    PfldInterfaceMeshOperator(const GridGeometry& g,
                              const std::vector<Point3D<double>>& obs,
                              const std::vector<double>& topo_elevations = {},
                              double max_dense_entries = 30e6)
        : g_(g), rows_(obs.size()), cols_(g.cells()), obs_(obs)
    {
        // 1. Build interface mesh geometry with pfld
        mesh_.build_geometry(g.nx, g.ny, g.nz,
                             g.x0, g.y0, g.dx, g.dy, g.dz,
                             topo_elevations, g.z0);

        const std::size_t total_dense_entries = rows_ * cols_;

        if (static_cast<double>(total_dense_entries) <= max_dense_entries) {
            is_dense_ = true;
            g_mat_.assign(rows_ * cols_, 0.0);

            // Parallel assembly over observations
            parallel_for(rows_, [&](std::size_t i) {
                const auto& obs_pt = obs_[i];
                const pfld::Point3Dd p_obs(obs_pt.x, obs_pt.y, obs_pt.z);
                double* row = g_mat_.data() + i * cols_;

                for (const auto& facet : mesh_.interfaces()) {
                    const double u = facet.evaluate_unit_gz(p_obs);
                    if (facet.cell_above >= 0) {
                        row[facet.cell_above] += u;
                    }
                    if (facet.cell_below >= 0) {
                        row[facet.cell_below] -= u;
                    }
                }
            });

            // Column norms squared: sum_i G_ij^2
            sens2_.assign(cols_, 0.0);
            for (std::size_t i = 0; i < rows_; ++i) {
                const double* row = g_mat_.data() + i * cols_;
                for (std::size_t j = 0; j < cols_; ++j) {
                    sens2_[j] += row[j] * row[j];
                }
            }
        } else {
            // Matrix-free fallback for massive grids
            is_dense_ = false;
            compute_matrix_free_sens2();
        }
    }

    [[nodiscard]] std::size_t rows() const noexcept override { return rows_; }
    [[nodiscard]] std::size_t cols() const noexcept override { return cols_; }
    [[nodiscard]] bool is_dense() const noexcept { return is_dense_; }
    [[nodiscard]] const pfld::InterfaceMesh<double>& mesh() const noexcept { return mesh_; }

    void set_active(const std::vector<char>& active) override {
        active_cols_.clear();
        for (std::size_t j = 0; j < active.size(); ++j) {
            if (active[j]) active_cols_.push_back(j);
        }
        use_active_ = true;
    }

    void apply(const std::vector<double>& m, std::vector<double>& d) const override {
        d.assign(rows_, 0.0);
        if (is_dense_) {
            parallel_for(rows_, [&](std::size_t i) {
                const double* row = g_mat_.data() + i * cols_;
                double s = 0.0;
                if (use_active_) {
                    for (const std::size_t j : active_cols_) s += row[j] * m[j];
                } else {
                    for (std::size_t j = 0; j < cols_; ++j) s += row[j] * m[j];
                }
                d[i] = s;
            });
        } else {
            // Matrix-free: evaluate d = K (D m) directly with pfld interfaces
            const auto& facets = mesh_.interfaces();
            std::vector<double> delta_rho(facets.size(), 0.0);
            for (std::size_t f = 0; f < facets.size(); ++f) {
                const auto& facet = facets[f];
                double val = 0.0;
                if (facet.cell_above >= 0) val += m[facet.cell_above];
                if (facet.cell_below >= 0) val -= m[facet.cell_below];
                delta_rho[f] = val;
            }

            parallel_for(rows_, [&](std::size_t i) {
                const pfld::Point3Dd p_obs(obs_[i].x, obs_[i].y, obs_[i].z);
                double s = 0.0;
                for (std::size_t f = 0; f < facets.size(); ++f) {
                    if (std::abs(delta_rho[f]) > 1e-12) {
                        s += delta_rho[f] * facets[f].evaluate_unit_gz(p_obs);
                    }
                }
                d[i] = s;
            });
        }
    }

    void apply_transpose(const std::vector<double>& r, std::vector<double>& g) const override {
        g.assign(cols_, 0.0);
        if (is_dense_) {
            if (use_active_) {
                parallel_for(active_cols_.size(), [&](std::size_t k) {
                    const std::size_t j = active_cols_[k];
                    double s = 0.0;
                    for (std::size_t i = 0; i < rows_; ++i) s += g_mat_[i * cols_ + j] * r[i];
                    g[j] = s;
                });
            } else {
                for (std::size_t i = 0; i < rows_; ++i) {
                    const double* row = g_mat_.data() + i * cols_;
                    const double ri = r[i];
                    for (std::size_t j = 0; j < cols_; ++j) g[j] += row[j] * ri;
                }
            }
        } else {
            // Matrix-free: g = D^T (K^T r)
            const auto& facets = mesh_.interfaces();
            std::vector<double> w(facets.size(), 0.0);
            parallel_for(facets.size(), [&](std::size_t f) {
                double wf = 0.0;
                for (std::size_t i = 0; i < rows_; ++i) {
                    const pfld::Point3Dd p_obs(obs_[i].x, obs_[i].y, obs_[i].z);
                    wf += r[i] * facets[f].evaluate_unit_gz(p_obs);
                }
                w[f] = wf;
            });

            // Map back: g = D^T w
            for (std::size_t f = 0; f < facets.size(); ++f) {
                const auto& facet = facets[f];
                if (facet.cell_above >= 0) g[facet.cell_above] += w[f];
                if (facet.cell_below >= 0) g[facet.cell_below] -= w[f];
            }
            if (use_active_) {
                for (std::size_t j = 0; j < cols_; ++j) {
                    if (std::find(active_cols_.begin(), active_cols_.end(), j) == active_cols_.end()) {
                        g[j] = 0.0;
                    }
                }
            }
        }
    }

    void column_norm_squared(std::vector<double>& out) const override {
        out = sens2_;
    }

    [[nodiscard]] double element(std::size_t i, std::size_t j) const noexcept {
        if (is_dense_) {
            return g_mat_[i * cols_ + j];
        }
        const pfld::Point3Dd p_obs(obs_[i].x, obs_[i].y, obs_[i].z);
        double s = 0.0;
        for (const auto& facet : mesh_.interfaces()) {
            if (facet.cell_above == static_cast<int64_t>(j)) s += facet.evaluate_unit_gz(p_obs);
            if (facet.cell_below == static_cast<int64_t>(j)) s -= facet.evaluate_unit_gz(p_obs);
        }
        return s;
    }

private:
    void compute_matrix_free_sens2() {
        sens2_.assign(cols_, 0.0);
        const auto& facets = mesh_.interfaces();
        parallel_for(cols_, [&](std::size_t j) {
            double sum_sq = 0.0;
            for (std::size_t i = 0; i < rows_; ++i) {
                const pfld::Point3Dd p_obs(obs_[i].x, obs_[i].y, obs_[i].z);
                double g_ij = 0.0;
                for (const auto& facet : facets) {
                    if (facet.cell_above == static_cast<int64_t>(j)) g_ij += facet.evaluate_unit_gz(p_obs);
                    if (facet.cell_below == static_cast<int64_t>(j)) g_ij -= facet.evaluate_unit_gz(p_obs);
                }
                sum_sq += g_ij * g_ij;
            }
            sens2_[j] = sum_sq;
        });
    }

    GridGeometry g_;
    std::size_t rows_, cols_;
    std::vector<Point3D<double>> obs_;
    pfld::InterfaceMesh<double> mesh_;
    bool is_dense_{false};
    std::vector<double> g_mat_;
    std::vector<double> sens2_;
    std::vector<std::size_t> active_cols_;
    bool use_active_{false};
};

/**
 * @brief Explicit dense sensitivity matrix for arbitrary station positions (e.g. draped over topography).
 * Memory: rows * cols doubles. Built in parallel with the exact Nagy (1966) prism kernel.
 */
class DenseGravityOperator final : public LinearOperator {
public:
    DenseGravityOperator(const GridGeometry& g, const std::vector<Point3D<double>>& obs)
        : rows_(obs.size()), cols_(g.cells()), g_(rows_ * cols_, 0.0)
    {
        parallel_for(rows_, [&](std::size_t i) {
            double* row = g_.data() + i * cols_;
            Point3D<double> pmin, pmax;
            for (std::size_t ix = 0; ix < g.nx; ++ix) {
                pmin.x = g.x0 + static_cast<double>(ix) * g.dx;
                pmax.x = pmin.x + g.dx;
                for (std::size_t iy = 0; iy < g.ny; ++iy) {
                    pmin.y = g.y0 + static_cast<double>(iy) * g.dy;
                    pmax.y = pmin.y + g.dy;
                    for (std::size_t iz = 0; iz < g.nz; ++iz) {
                        pmin.z = g.z0 + static_cast<double>(iz) * g.dz;
                        pmax.z = pmin.z + g.dz;
                        row[g.cell_index(ix, iy, iz)] =
                            ForwardKernel<double>::prism_gravity_gz(obs[i], pmin, pmax, 1.0);
                    }
                }
            }
        });
    }

    [[nodiscard]] std::size_t rows() const noexcept override { return rows_; }
    [[nodiscard]] std::size_t cols() const noexcept override { return cols_; }

    void set_active(const std::vector<char>& active) override {
        active_cols_.clear();
        for (std::size_t j = 0; j < active.size(); ++j) {
            if (active[j]) active_cols_.push_back(j);
        }
        use_active_ = true;
    }

    void apply(const std::vector<double>& m, std::vector<double>& d) const override {
        d.assign(rows_, 0.0);
        parallel_for(rows_, [&](std::size_t i) {
            const double* row = g_.data() + i * cols_;
            double s = 0.0;
            if (use_active_) {
                for (const std::size_t j : active_cols_) s += row[j] * m[j];
            } else {
                for (std::size_t j = 0; j < cols_; ++j) s += row[j] * m[j];
            }
            d[i] = s;
        });
    }

    void apply_transpose(const std::vector<double>& r, std::vector<double>& g) const override {
        g.assign(cols_, 0.0);
        if (use_active_) {
            parallel_for(active_cols_.size(), [&](std::size_t k) {
                const std::size_t j = active_cols_[k];
                double s = 0.0;
                for (std::size_t i = 0; i < rows_; ++i) s += g_[i * cols_ + j] * r[i];
                g[j] = s;
            });
        } else {
            for (std::size_t i = 0; i < rows_; ++i) {
                const double* row = g_.data() + i * cols_;
                const double ri = r[i];
                for (std::size_t j = 0; j < cols_; ++j) g[j] += row[j] * ri;
            }
        }
    }

    void column_norm_squared(std::vector<double>& out) const override {
        out.assign(cols_, 0.0);
        for (std::size_t i = 0; i < rows_; ++i) {
            const double* row = g_.data() + i * cols_;
            for (std::size_t j = 0; j < cols_; ++j) out[j] += row[j] * row[j];
        }
    }

    /// Matrix element access (tests / diagnostics).
    [[nodiscard]] double element(std::size_t i, std::size_t j) const noexcept { return g_[i * cols_ + j]; }

private:
    std::size_t rows_, cols_;
    std::vector<double> g_;
    std::vector<std::size_t> active_cols_;
    bool use_active_{false};
};

/**
 * @brief Matrix-free sensitivity operator for a FLAT observation plane over a regular grid.
 *
 * One station sits above the centre of every cell column. For each depth layer the
 * response depends only on the horizontal offset, so G is block-Toeplitz and
 * G m is evaluated with 2D FFTs: O(nz * N log N) per application instead of
 * O(nx*ny * nx*ny*nz) for the dense matrix, and O(nz * N) memory.
 *
 * Because the prism response is symmetric in +/-dx and +/-dy the transpose is the same
 * convolution applied to the residual.
 */
class ToeplitzGravityOperator final : public LinearOperator {
public:
    using cd = std::complex<double>;

    ToeplitzGravityOperator(const GridGeometry& g, double z_obs)
        : g_(g),
          px_(next_pow2(2 * g.nx - 1)),
          py_(next_pow2(2 * g.ny - 1)),
          fft_(px_, py_),
          khat_(g.nz),
          layer_active_(g.nz, 1),
          sens2_(g.cells(), 0.0)
    {
        const std::size_t kx = 2 * g.nx - 1;
        const std::size_t ky = 2 * g.ny - 1;
        std::vector<std::vector<double>> sat(g.nz);

        parallel_for(g.nz, [&](std::size_t iz) {
            std::vector<cd> k(px_ * py_, cd(0.0, 0.0));
            std::vector<double> sq(kx * ky, 0.0);
            const Point3D<double> obs(0.0, 0.0, z_obs);
            for (std::size_t a = 0; a < kx; ++a) {
                const long di = static_cast<long>(a) - static_cast<long>(g.nx - 1);
                for (std::size_t b = 0; b < ky; ++b) {
                    const long dj = static_cast<long>(b) - static_cast<long>(g.ny - 1);
                    const Point3D<double> pmin(di * g.dx - 0.5 * g.dx, dj * g.dy - 0.5 * g.dy,
                                               g.z0 + static_cast<double>(iz) * g.dz);
                    const Point3D<double> pmax(pmin.x + g.dx, pmin.y + g.dy, pmin.z + g.dz);
                    const double v = ForwardKernel<double>::prism_gravity_gz(obs, pmin, pmax, 1.0);
                    const std::size_t ia = static_cast<std::size_t>((di + static_cast<long>(px_)) % static_cast<long>(px_));
                    const std::size_t ib = static_cast<std::size_t>((dj + static_cast<long>(py_)) % static_cast<long>(py_));
                    k[ia * py_ + ib] = cd(v, 0.0);
                    sq[a * ky + b] = v * v;
                }
            }
            fft_.forward(k);
            khat_[iz] = std::move(k);

            // Summed-area table of K^2 for exact column norms.
            std::vector<double> s((kx + 1) * (ky + 1), 0.0);
            for (std::size_t a = 0; a < kx; ++a) {
                double row_sum = 0.0;
                for (std::size_t b = 0; b < ky; ++b) {
                    row_sum += sq[a * ky + b];
                    s[(a + 1) * (ky + 1) + (b + 1)] = s[a * (ky + 1) + (b + 1)] + row_sum;
                }
            }
            sat[iz] = std::move(s);
        });

        for (std::size_t iz = 0; iz < g.nz; ++iz) {
            const auto& s = sat[iz];
            for (std::size_t ip = 0; ip < g.nx; ++ip) {
                for (std::size_t jp = 0; jp < g.ny; ++jp) {
                    // observation i in [0,nx): di = ip - i spans [ip-nx+1, ip] -> table index a in [ip, ip+nx-1]
                    const std::size_t a0 = ip, a1 = ip + g.nx;
                    const std::size_t b0 = jp, b1 = jp + g.ny;
                    const std::size_t w = ky + 1;
                    const double v = s[a1 * w + b1] - s[a0 * w + b1] - s[a1 * w + b0] + s[a0 * w + b0];
                    sens2_[g.cell_index(ip, jp, iz)] = v;
                }
            }
        }
    }

    [[nodiscard]] std::size_t rows() const noexcept override { return g_.columns(); }
    [[nodiscard]] std::size_t cols() const noexcept override { return g_.cells(); }

    void set_active(const std::vector<char>& active) override {
        std::fill(layer_active_.begin(), layer_active_.end(), 0);
        for (std::size_t ic = 0; ic < g_.columns(); ++ic) {
            for (std::size_t iz = 0; iz < g_.nz; ++iz) {
                if (active[ic * g_.nz + iz]) layer_active_[iz] = 1;
            }
        }
    }

    void apply(const std::vector<double>& m, std::vector<double>& d) const override {
        std::vector<cd> acc(px_ * py_, cd(0.0, 0.0));
        std::vector<cd> buf(px_ * py_);
        for (std::size_t iz = 0; iz < g_.nz; ++iz) {
            if (!layer_active_[iz]) continue;
            std::fill(buf.begin(), buf.end(), cd(0.0, 0.0));
            for (std::size_t ix = 0; ix < g_.nx; ++ix) {
                for (std::size_t iy = 0; iy < g_.ny; ++iy) {
                    buf[ix * py_ + iy] = cd(m[g_.cell_index(ix, iy, iz)], 0.0);
                }
            }
            fft_.forward(buf);
            const auto& kh = khat_[iz];
            for (std::size_t q = 0; q < buf.size(); ++q) acc[q] += buf[q] * kh[q];
        }
        fft_.inverse(acc);
        d.assign(g_.columns(), 0.0);
        for (std::size_t ix = 0; ix < g_.nx; ++ix) {
            for (std::size_t iy = 0; iy < g_.ny; ++iy) d[ix * g_.ny + iy] = acc[ix * py_ + iy].real();
        }
    }

    void apply_transpose(const std::vector<double>& r, std::vector<double>& g) const override {
        std::vector<cd> rf(px_ * py_, cd(0.0, 0.0));
        for (std::size_t ix = 0; ix < g_.nx; ++ix) {
            for (std::size_t iy = 0; iy < g_.ny; ++iy) rf[ix * py_ + iy] = cd(r[ix * g_.ny + iy], 0.0);
        }
        fft_.forward(rf);
        g.assign(g_.cells(), 0.0);
        std::vector<cd> buf(px_ * py_);
        for (std::size_t iz = 0; iz < g_.nz; ++iz) {
            if (!layer_active_[iz]) continue;
            const auto& kh = khat_[iz];
            for (std::size_t q = 0; q < buf.size(); ++q) buf[q] = rf[q] * kh[q];
            fft_.inverse(buf);
            for (std::size_t ix = 0; ix < g_.nx; ++ix) {
                for (std::size_t iy = 0; iy < g_.ny; ++iy) {
                    g[g_.cell_index(ix, iy, iz)] = buf[ix * py_ + iy].real();
                }
            }
        }
    }

    void column_norm_squared(std::vector<double>& out) const override { out = sens2_; }

private:
    GridGeometry g_;
    std::size_t px_, py_;
    Fft2D fft_;
    std::vector<std::vector<cd>> khat_;
    std::vector<char> layer_active_;
    std::vector<double> sens2_;
};

/// Mean elevation (z, down-positive) of the four DEM nodes surrounding column (ix, iy).
/// `topo_nodes` has (nx+1)*(ny+1) entries indexed [iy*(nx+1)+ix] (same layout as pfld::InterfaceMesh).
[[nodiscard]] inline double column_surface_z(const std::vector<double>& topo_nodes,
                                            std::size_t nx, std::size_t ny,
                                            std::size_t ix, std::size_t iy, double z_default) noexcept {
    if (topo_nodes.size() != (nx + 1) * (ny + 1)) return z_default;
    const std::size_t w = nx + 1;
    (void)ny;
    return 0.25 * (topo_nodes[iy * w + ix] + topo_nodes[iy * w + ix + 1] +
                   topo_nodes[(iy + 1) * w + ix] + topo_nodes[(iy + 1) * w + ix + 1]);
}

} // namespace inv
