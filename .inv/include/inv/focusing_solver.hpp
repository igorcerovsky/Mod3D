#pragma once

#include "focusing_metrics.hpp"
#include "gravity_operator.hpp"
#include "voxel_grid.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <vector>

namespace inv {

enum class BackgroundTrendMode : int {
    None = 0,          ///< Raw fit: G m ≈ d (no datum shift or regional trend estimation)
    ConstantShift = 1, ///< Jointly fit constant datum shift: G m + c0 ≈ d
    PlanarRamp = 2,    ///< Jointly fit planar regional trend: G m + c0 + cx*x + cy*y ≈ d
    BorderPlanar = 3   ///< Fit planar regional tilt on perimeter border + joint constant datum shift c0
};

/// Perimeter-weighted polynomial projector and full-grid datum aligner for background removal.
/// Using corner-weighted perimeter stations for the projector P_perim guarantees:
///  1. Exact shift/ramp invariance: P_perim(d + c0 + cx*x + cy*y) == P_perim(d) + c0 + cx*x + cy*y.
///  2. No Bouguer-slab nullspace inside PCG (unlike full-grid mean projection inside G^T P G, which
///     forces horizontal layers to zero mean density and creates artificial negative halos).
///  3. As m^(k) focuses onto a compact support, P_perim(d - G m^(k)) accounts for the body's far-field
///     gravity at the boundary and converges to the true regional datum/trend.
struct BackgroundBasis {
    GridGeometry geom{};
    int mode{0};
    std::vector<double> w_perim; ///< Normalized corner-weighted perimeter weights (sum == 1)
    double xm{0.0};
    double ym{0.0};
    double sx2_perim{0.0};
    double sy2_perim{0.0};

    BackgroundBasis() = default;
    BackgroundBasis(const GridGeometry& g, int trend_mode) { build(g, trend_mode); }
    BackgroundBasis(const GridGeometry& g, BackgroundTrendMode trend_mode) { build(g, static_cast<int>(trend_mode)); }

    void build(const GridGeometry& g, int trend_mode) {
        geom = g;
        mode = trend_mode;
        const std::size_t rows = g.columns();
        w_perim.assign(rows, 0.0);
        xm = g.x0 + 0.5 * static_cast<double>(g.nx) * g.dx;
        ym = g.y0 + 0.5 * static_cast<double>(g.ny) * g.dy;
        sx2_perim = 0.0;
        sy2_perim = 0.0;
        if (rows == 0 || trend_mode <= 0) return;

        double w_sum = 0.0;
        for (std::size_t ix = 0; ix < g.nx; ++ix) {
            for (std::size_t iy = 0; iy < g.ny; ++iy) {
                const bool is_border = (g.nx <= 2 || g.ny <= 2) ||
                                       (ix == 0 || ix + 1 == g.nx || iy == 0 || iy + 1 == g.ny);
                if (!is_border) continue;
                const double dx_i = g.xc(ix) - xm;
                const double dy_i = g.yc(iy) - ym;
                // Weight perimeter stations by squared distance from center (emphasizes corners where
                // compact interior anomalies are weakest, while preserving exact symmetry around (xm, ym)).
                const double w = std::max(1e-12, dx_i * dx_i + dy_i * dy_i);
                w_perim[ix * g.ny + iy] = w;
                w_sum += w;
            }
        }
        if (w_sum <= 0.0) {
            const double u = 1.0 / static_cast<double>(rows);
            std::fill(w_perim.begin(), w_perim.end(), u);
        } else {
            for (double& w : w_perim) w /= w_sum;
        }

        for (std::size_t ix = 0; ix < g.nx; ++ix) {
            const double dx_i = g.xc(ix) - xm;
            for (std::size_t iy = 0; iy < g.ny; ++iy) {
                const double dy_i = g.yc(iy) - ym;
                const double w = w_perim[ix * g.ny + iy];
                sx2_perim += w * dx_i * dx_i;
                sy2_perim += w * dy_i * dy_i;
            }
        }
    }

    /// Evaluates the perimeter-projected background field on `v` (usually d - G m).
    void evaluate_perimeter(const std::vector<double>& v,
                            std::vector<double>& bg_out,
                            double& c0_mgal,
                            double& cx_mgal_per_km,
                            double& cy_mgal_per_km) const noexcept
    {
        const std::size_t rows = v.size();
        bg_out.assign(rows, 0.0);
        c0_mgal = 0.0;
        cx_mgal_per_km = 0.0;
        cy_mgal_per_km = 0.0;
        if (rows == 0 || mode <= 0 || w_perim.size() != rows) return;

        double c0 = 0.0, sxb = 0.0, syb = 0.0;
        for (std::size_t ix = 0; ix < geom.nx; ++ix) {
            const double dx_i = geom.xc(ix) - xm;
            for (std::size_t iy = 0; iy < geom.ny; ++iy) {
                const std::size_t idx = ix * geom.ny + iy;
                const double w = w_perim[idx];
                if (w <= 0.0) continue;
                const double val = v[idx];
                c0 += w * val;
                sxb += w * dx_i * val;
                syb += w * (geom.yc(iy) - ym) * val;
            }
        }
        c0_mgal = c0;
        double cx = 0.0, cy = 0.0;
        if (mode == static_cast<int>(BackgroundTrendMode::PlanarRamp) ||
            mode == static_cast<int>(BackgroundTrendMode::BorderPlanar)) {
            if (sx2_perim > 1e-12) cx = sxb / sx2_perim;
            if (sy2_perim > 1e-12) cy = syb / sy2_perim;
            cx_mgal_per_km = cx * 1000.0;
            cy_mgal_per_km = cy * 1000.0;
        }

        for (std::size_t ix = 0; ix < geom.nx; ++ix) {
            const double dx_i = geom.xc(ix) - xm;
            for (std::size_t iy = 0; iy < geom.ny; ++iy) {
                const double dy_i = geom.yc(iy) - ym;
                bg_out[ix * geom.ny + iy] = c0 + cx * dx_i + cy * dy_i;
            }
        }
    }

    /// Projects out the perimeter-estimated background from `v` in-place: v <- v - P_perim(v).
    void project_out(std::vector<double>& v) const noexcept {
        if (mode <= 0 || v.empty()) return;
        std::vector<double> bg;
        double c0 = 0.0, cx = 0.0, cy = 0.0;
        evaluate_perimeter(v, bg, c0, cx, cy);
        for (std::size_t i = 0; i < v.size(); ++i) v[i] -= bg[i];
    }

    /// Evaluates the final background field on `d_minus_gm` (d - G m) with exact full-grid zero-mean
    /// residual alignment so that mean((G m + bg_out) - d) == 0 to machine precision.
    void evaluate(const std::vector<double>& d_minus_gm,
                  std::vector<double>& bg_out,
                  double& c0_mgal,
                  double& cx_mgal_per_km,
                  double& cy_mgal_per_km) const noexcept
    {
        const std::size_t rows = d_minus_gm.size();
        bg_out.assign(rows, 0.0);
        c0_mgal = 0.0;
        cx_mgal_per_km = 0.0;
        cy_mgal_per_km = 0.0;
        if (rows == 0 || mode <= 0) return;

        evaluate_perimeter(d_minus_gm, bg_out, c0_mgal, cx_mgal_per_km, cy_mgal_per_km);

        // Enforce exact full-grid mean alignment so computed field has zero constant shift vs measured field
        double mean_rem = 0.0;
        for (std::size_t i = 0; i < rows; ++i) mean_rem += (d_minus_gm[i] - bg_out[i]);
        mean_rem /= static_cast<double>(rows);
        for (std::size_t i = 0; i < rows; ++i) bg_out[i] += mean_rem;
        c0_mgal += mean_rem;
    }

    /// Fits a zero-mean planar tilt (cx*(x-xm) + cy*(y-ym)) using the corner-weighted perimeter columns.
    static void fit_border_tilt(const GridGeometry& g,
                                const std::vector<double>& d,
                                std::vector<double>& tilt_out,
                                double& cx_mgal_per_km,
                                double& cy_mgal_per_km) noexcept
    {
        BackgroundBasis basis(g, BackgroundTrendMode::BorderPlanar);
        double c0 = 0.0;
        basis.evaluate_perimeter(d, tilt_out, c0, cx_mgal_per_km, cy_mgal_per_km);
        for (double& v : tilt_out) v -= c0;
    }
};

/// Per-level diagnostics reported by the solver.
struct LevelStats {
    int level{0};
    std::size_t nx{0}, ny{0}, nz{0};
    std::size_t active_cells{0};
    int irls_iterations{0};
    int cg_iterations{0};
    double chi_rms{0.0};          ///< sqrt(mean(((Gm-d)/sigma)^2)); 1.0 == fit at noise level
    double rms_mgal{0.0};         ///< unweighted data RMS [mGal]
    double rho_end{0.0};          ///< final dimensionless regularisation ratio
    double e_end{0.0};            ///< final focusing threshold [g/cm^3]
    double ms_volume_fraction{0.0};  ///< S_MS / volume of topography-active cells (set by the engine)
    double mgs_area_fraction{0.0};   ///< S_MGS / interior face area of topography-active cells (set by the engine)
    double baseline_shift_mgal{0.0}; ///< Fitted constant datum shift c0 [mGal]
    double time_ms{0.0};
};

struct SolverParams {
    int irls_iterations{8};
    int cg_max_iterations{80};
    double cg_tolerance{1e-4};

    // Focusing threshold schedule for THIS level, relative to the current model amplitude.
    double e_start_rel{1.0};
    double e_end_rel{0.02};
    double e_abs_min{1e-4};       ///< absolute floor [g/cm^3]

    double rho_start{1.0};        ///< initial dimensionless regularisation strength
    double sigma{1.0};            ///< data noise std-dev [mGal]
    double target_chi{1.0};       ///< discrepancy-principle target (rms of weighted residual)

    bool focusing{true};          ///< false: plain depth-weighted Tikhonov L2 (baseline)
    double ms_weight{1.0};
    double mgs_weight{1.0};
    double depth_exponent{1.0};   ///< w_j = (S_j / S_max)^gamma, S_j = column norm of G
    int background_trend_mode{0}; ///< 0=None, 1=ConstantShift, 2=PlanarRamp, 3=BorderPlanar

    double min_density{-0.5};
    double max_density{0.6};
    double density_scale{0.5};    ///< typical anomaly amplitude [g/cm^3]

    bool verbose{false};          ///< print one diagnostic line per IRLS iteration to stderr

    /// Optional progress callback: return false to abort/cancel early.
    /// Args: (int iter, int total_iters, double rms_mgal, double chi_rms)
    std::function<bool(int iter, int total_iters, double rms_mgal, double chi_rms)> progress_callback;

    /// Optional cancel callback: return true to abort immediately.
    std::function<bool()> cancel_check;
};

/**
 * @brief One level of Portniaguine-Zhdanov re-weighted focusing inversion (IRLS + PCG).
 *
 * Minimises, with delta = m - m_ref and W_d = 1/sigma,
 *
 *   phi = || W_d (G m - d) ||^2
 *       + alpha * [ mu_ms * sum_j V_j (w_j delta_j)^2 / ((w_j delta_j)^2 + e^2)   (S_MS, volume)
 *                 + mu_gs * sum_f A_f (w_f D_f)^2     / ((w_f D_f)^2     + e^2) ]   (S_MGS, face area)
 *
 * i.e. the stabilisers act on the sensitivity-WEIGHTED model m_w = W m (Zhdanov), so the cost of
 * "occupying" a cell is depth independent (1 per unit volume); the weight only rescales the
 * threshold e / w_j. (Multiplying the saturated ratio by w^2 instead would make deep occupancy
 * cheap and bias compact bodies downward - this was verified numerically.)
 *
 * It is solved by iteratively re-weighted least squares: at iteration k the ratios are replaced by
 * quadratic forms with weights w^2 / ((w delta_k)^2 + e_k^2) (Zhdanov's W_e), and the resulting
 * SPD normal equations are solved by Jacobi-preconditioned CG, warm-started from delta_k.
 *
 *  - w_j = (S_j/S_max)^gamma with S_j = sqrt(sum_i G_ij^2) is the integrated-sensitivity depth weighting
 *    (Li & Oldenburg 1996; Portniaguine & Zhdanov 2002); it replaces ad-hoc 1/(z+z0) factors and
 *    automatically accounts for cell size, observation height and grid extent.
 *  - e_k is annealed geometrically from e_start_rel to e_end_rel times the current max |w delta|,
 *    advancing only while the data misfit is acceptable (chi <= 1.2 * target)
 *    (a RELATIVE threshold, so the schedule is independent of the density units / amplitude).
 *  - alpha is cooled by the discrepancy principle: after every iteration
 *    alpha *= clamp((target/chi)^1.5, 0.1, 2.5), steering chi toward the noise level.
 *  - Bounds [min_density, max_density] are enforced by projection after each solve.
 *  - Inactive cells (above topography or outside the adaptive support) are held at zero.
 */
class FocusingSolver {
public:
    static bool solve(const GridGeometry& g, LinearOperator& op, const std::vector<double>& d,
                      std::vector<double>& m, const std::vector<char>& active,
                      const std::vector<double>* m_ref, const SolverParams& p, LevelStats& st)
    {
        const auto t0 = std::chrono::high_resolution_clock::now();
        const std::size_t n = g.cells();
        const std::size_t rows = op.rows();
        const double inv_sigma2 = 1.0 / (p.sigma * p.sigma);

        op.set_active(active);

        std::size_t n_active = 0;
        for (char a : active) n_active += a ? 1 : 0;
        st.active_cells = n_active;

        // ---- integrated-sensitivity depth weights ------------------------------------------------
        std::vector<double> sens2;
        op.column_norm_squared(sens2);
        double smax2 = 0.0;
        for (std::size_t j = 0; j < n; ++j) {
            if (active[j]) smax2 = std::max(smax2, sens2[j]);
        }
        if (smax2 <= 0.0) smax2 = 1.0;
        std::vector<double> w2(n, 0.0);
        for (std::size_t j = 0; j < n; ++j) {
            if (!active[j]) continue;
            w2[j] = std::max(1e-8, std::pow(sens2[j] / smax2, p.depth_exponent));
        }

        const double vcell = g.dx * g.dy * g.dz;
        const double aref = std::pow(vcell, 2.0 / 3.0);
        const double fa[3] = {g.dy * g.dz / aref, g.dx * g.dz / aref, g.dx * g.dy / aref};
        const std::size_t stride[3] = {g.ny * g.nz, g.nz, 1};

        const double s_ref = 0.25 * p.density_scale;
        const double a_scale = smax2 * s_ref * s_ref * inv_sigma2; // alpha = rho * a_scale

        // ---- reference model and initial state ---------------------------------------------------
        std::vector<double> ref(n, 0.0);
        if (m_ref) {
            for (std::size_t j = 0; j < n; ++j) ref[j] = active[j] ? (*m_ref)[j] : 0.0;
        }
        std::vector<double> delta(n, 0.0);
        for (std::size_t j = 0; j < n; ++j) delta[j] = active[j] ? m[j] - ref[j] : 0.0;

        // residual of the reference model: r0_base = d - G m_ref
        BackgroundBasis bg;
        bg.build(g, p.background_trend_mode);

        std::vector<double> tmp(rows);
        op.apply(ref, tmp);
        std::vector<double> r0_base(rows), r0(rows), bg_cur(rows, 0.0);
        for (std::size_t i = 0; i < rows; ++i) r0_base[i] = d[i] - tmp[i];

        // scratch
        std::vector<double> gx(rows), r_proj(rows), gt(n);
        std::vector<double> ce(n), cf[3] = {std::vector<double>(n), std::vector<double>(n), std::vector<double>(n)};
        std::vector<double> prec_inv(n), rhs(n), x(n);

        double rho = p.rho_start;
        int total_cg = 0;
        int iters_done = 0;
        double chi = 0.0, e_k = p.e_abs_min;
        std::vector<double> m_cur(n, 0.0);
        for (std::size_t j = 0; j < n; ++j) m_cur[j] = active[j] ? ref[j] + delta[j] : 0.0;
        if (p.background_trend_mode > 0) {
            op.apply(m_cur, tmp);
            for (std::size_t i = 0; i < rows; ++i) tmp[i] = d[i] - tmp[i];
            double c0_init = 0.0, cx_init = 0.0, cy_init = 0.0;
            bg.evaluate_perimeter(tmp, bg_cur, c0_init, cx_init, cy_init);
        }
        for (std::size_t i = 0; i < rows; ++i) r0[i] = r0_base[i] - bg_cur[i];

        const int iters = std::max(1, p.irls_iterations);
        // e is annealed geometrically ONLY while the data are fitted (chi <= 1.2 target): tightening the
        // focusing while the misfit is still too large makes the penalty explode faster than alpha can
        // be cooled and the iteration diverges (verified numerically).
        double e_rel = p.e_start_rel;
        const double e_decay = (iters > 1) ? std::pow(p.e_end_rel / p.e_start_rel, 1.0 / (iters - 1)) : 1.0;
        for (int k = 0; k < iters; ++k) {
            if (p.cancel_check && p.cancel_check()) {
                st.irls_iterations = iters_done;
                st.cg_iterations = total_cg;
                st.chi_rms = chi;
                st.rho_end = rho;
                st.e_end = e_k;
                st.time_ms = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - t0).count();
                m = m_cur;
                return false;
            }

            // ---- focusing threshold --------------------------------------------------------------
            double amp = 0.0;
            for (std::size_t j = 0; j < n; ++j) amp = std::max(amp, std::sqrt(w2[j]) * std::abs(delta[j]));
            const double s_k = (k == 0 && amp <= 0.0) ? s_ref : std::max(amp, 0.02 * p.density_scale);
            e_k = std::max(p.e_abs_min, e_rel * s_k);
            const double e2 = e_k * e_k;
            // On k == 0 of a prolongated level, piecewise-constant prolongation has zero internal
            // gradients (delta[jb] - delta[j] == 0) inside each coarse block. Using a relaxed MGS
            // threshold on k == 0 lets the fine gravity kernel break coarse-block symmetry first.
            const double e_grad_k = (k == 0) ? std::max(e_k, 0.35 * s_k) : e_k;
            const double e_grad2 = e_grad_k * e_grad_k;
            const double alpha = rho * a_scale;

            // ---- IRLS weights (reg coefficients incl. alpha) -------------------------------------
            const double uni = 1.0 / (s_ref * s_ref);
            for (std::size_t j = 0; j < n; ++j) {
                if (!active[j]) { ce[j] = 0.0; continue; }
                const double den = p.focusing ? 1.0 / (w2[j] * delta[j] * delta[j] + e2) : uni;
                ce[j] = alpha * p.ms_weight * w2[j] * den;
            }
            for (int a = 0; a < 3; ++a) std::fill(cf[a].begin(), cf[a].end(), 0.0);
            std::fill(prec_inv.begin(), prec_inv.end(), 1.0);
            for (std::size_t ix = 0; ix < g.nx; ++ix) {
                for (std::size_t iy = 0; iy < g.ny; ++iy) {
                    for (std::size_t iz = 0; iz < g.nz; ++iz) {
                        const std::size_t j = g.cell_index(ix, iy, iz);
                        if (!active[j]) continue;
                        const bool has[3] = {ix + 1 < g.nx, iy + 1 < g.ny, iz + 1 < g.nz};
                        for (int a = 0; a < 3; ++a) {
                            if (!has[a]) continue;
                            const std::size_t jb = j + stride[a];
                            if (!active[jb]) continue;
                            const double dj = delta[jb] - delta[j];
                            const double wf2 = 0.5 * (w2[j] + w2[jb]);
                            const double den = (p.focusing && k > 0) ? 1.0 / (wf2 * dj * dj + e_grad2) : uni;
                            cf[a][j] = alpha * p.mgs_weight * fa[a] * wf2 * den;
                        }
                    }
                }
            }
            // Jacobi preconditioner diag(A)
            std::vector<double> diag(n, 1.0);
            for (std::size_t j = 0; j < n; ++j) {
                if (!active[j]) continue;
                double dd = sens2[j] * inv_sigma2 + ce[j];
                for (int a = 0; a < 3; ++a) {
                    dd += cf[a][j];
                    if (j >= stride[a]) dd += cf[a][j - stride[a]];
                }
                diag[j] = std::max(dd, 1e-300);
            }
            for (std::size_t j = 0; j < n; ++j) prec_inv[j] = active[j] ? 1.0 / diag[j] : 1.0;

            // ---- rhs = G^T P_perp W^2 r0 ---------------------------------------------------------
            for (std::size_t i = 0; i < rows; ++i) gx[i] = r0[i] * inv_sigma2;
            op.apply_transpose(gx, rhs);
            for (std::size_t j = 0; j < n; ++j) if (!active[j]) rhs[j] = 0.0;

            // ---- A x = G^T W^2 G x + Reg x -------------------------------------------------------
            auto matvec = [&](const std::vector<double>& v, std::vector<double>& out) {
                op.apply(v, gx);
                for (std::size_t i = 0; i < rows; ++i) gx[i] *= inv_sigma2;
                op.apply_transpose(gx, out);
                for (std::size_t j = 0; j < n; ++j) {
                    out[j] = active[j] ? out[j] + ce[j] * v[j] : 0.0;
                }
                for (std::size_t j = 0; j < n; ++j) {
                    if (!active[j]) continue;
                    for (int a = 0; a < 3; ++a) {
                        const double c = cf[a][j];
                        if (c == 0.0) continue;
                        const std::size_t jb = j + stride[a];
                        const double df = c * (v[j] - v[jb]);
                        out[j] += df;
                        out[jb] -= df;
                    }
                }
            };

            x = delta;
            total_cg += pcg(matvec, prec_inv, rhs, x, p.cg_max_iterations, p.cg_tolerance, n, p.cancel_check);

            // ---- projection onto density bounds ----------------------------------------------------
            double dnorm = 0.0, dchg = 0.0;
            for (std::size_t j = 0; j < n; ++j) {
                if (!active[j]) { x[j] = 0.0; continue; }
                const double mv = std::clamp(ref[j] + x[j], p.min_density, p.max_density);
                x[j] = mv - ref[j];
                dnorm += x[j] * x[j];
                dchg += (x[j] - delta[j]) * (x[j] - delta[j]);
            }
            delta = x;
            ++iters_done;

            // ---- data misfit, background update, and discrepancy-principle alpha update -----------
            for (std::size_t j = 0; j < n; ++j) m_cur[j] = active[j] ? ref[j] + delta[j] : 0.0;
            op.apply(m_cur, gx);
            if (p.background_trend_mode > 0) {
                for (std::size_t i = 0; i < rows; ++i) r_proj[i] = d[i] - gx[i];
                double c0_k = 0.0, cx_k = 0.0, cy_k = 0.0;
                bg.evaluate_perimeter(r_proj, bg_cur, c0_k, cx_k, cy_k);
                for (std::size_t i = 0; i < rows; ++i) r0[i] = r0_base[i] - bg_cur[i];
            }
            double sum_shift = 0.0;
            for (std::size_t i = 0; i < rows; ++i) {
                r_proj[i] = (gx[i] + bg_cur[i]) - d[i];
                sum_shift += (d[i] - gx[i]);
            }
            if (p.background_trend_mode > 0 && rows > 0) {
                double rem_mean = 0.0;
                for (double v : r_proj) rem_mean += v;
                rem_mean /= static_cast<double>(rows);
                for (double& v : r_proj) v -= rem_mean;
                st.baseline_shift_mgal = sum_shift / static_cast<double>(rows);
            }
            double ss = 0.0;
            for (std::size_t i = 0; i < rows; ++i) {
                ss += r_proj[i] * r_proj[i];
            }
            chi = std::sqrt(ss * inv_sigma2 / static_cast<double>(rows));
            st.rms_mgal = std::sqrt(ss / static_cast<double>(rows));

            if (p.verbose) {
                std::fprintf(stderr, "  [irls %2d] e=%.4f rho=%.3e chi=%.3f cg=%d amp_w=%.3f\n",
                             k, e_k, rho, chi, total_cg, amp);
            }

            if (p.progress_callback) {
                if (!p.progress_callback(k + 1, iters, st.rms_mgal, chi)) {
                    st.irls_iterations = iters_done;
                    st.cg_iterations = total_cg;
                    st.chi_rms = chi;
                    st.rho_end = rho;
                    st.e_end = e_k;
                    st.time_ms = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - t0).count();
                    m = m_cur;
                    return false;
                }
            }

            const bool in_band = (chi > 0.85 * p.target_chi && chi < 1.15 * p.target_chi);
            const double prev_e_rel = e_rel;
            const int rem = std::max(1, iters - 1 - k);
            if (chi <= 1.6 * p.target_chi) {
                const double dyn_decay = std::pow(std::min(1.0, p.e_end_rel / std::max(e_rel, 1e-12)), 1.0 / static_cast<double>(rem));
                e_rel = std::max(p.e_end_rel, e_rel * dyn_decay);
            } else if (k >= 1) {
                e_rel = std::max(p.e_end_rel, e_rel * std::sqrt(e_decay));
            }
            const double e_step_ratio = (prev_e_rel > 0.0) ? (e_rel / prev_e_rel) : 1.0;
            const double change = std::sqrt(dchg) / (std::sqrt(dnorm) + 1e-30);
            if (k >= std::max(2, iters / 2) && in_band && e_rel <= 1.05 * p.e_end_rel && change < 1e-2) break;
            if (!in_band) {
                const double f = std::clamp(std::pow(p.target_chi / std::max(chi, 1e-12), 1.5), 0.08, 2.5);
                rho *= f * std::sqrt(e_step_ratio);
            } else if (e_step_ratio < 0.99) {
                rho *= std::sqrt(e_step_ratio);
            }
        }

        m = m_cur;
        st.irls_iterations = iters_done;
        st.cg_iterations = total_cg;
        st.chi_rms = chi;
        st.rho_end = rho;
        st.e_end = e_k;

        st.time_ms = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - t0).count();
        return true;
    }

private:
    template <typename MatVec>
    static int pcg(MatVec&& A, const std::vector<double>& minv, const std::vector<double>& b,
                   std::vector<double>& x, int max_iter, double tol, std::size_t n,
                   const std::function<bool()>& cancel_check = nullptr)
    {
        std::vector<double> r(n), z(n), p(n), Ap(n);
        A(x, Ap);
        double bnorm = 0.0;
        for (std::size_t j = 0; j < n; ++j) { r[j] = b[j] - Ap[j]; bnorm += b[j] * b[j]; }
        bnorm = std::sqrt(bnorm);
        if (bnorm <= 0.0) { std::fill(x.begin(), x.end(), 0.0); return 0; }
        for (std::size_t j = 0; j < n; ++j) { z[j] = minv[j] * r[j]; p[j] = z[j]; }
        double rz = 0.0;
        for (std::size_t j = 0; j < n; ++j) rz += r[j] * z[j];
        int it = 0;
        for (; it < max_iter; ++it) {
            if (cancel_check && cancel_check()) break;
            double rn = 0.0;
            for (std::size_t j = 0; j < n; ++j) rn += r[j] * r[j];
            if (std::sqrt(rn) <= tol * bnorm) break;
            A(p, Ap);
            double pAp = 0.0;
            for (std::size_t j = 0; j < n; ++j) pAp += p[j] * Ap[j];
            if (pAp <= 0.0) break;
            const double a = rz / pAp;
            for (std::size_t j = 0; j < n; ++j) { x[j] += a * p[j]; r[j] -= a * Ap[j]; }
            for (std::size_t j = 0; j < n; ++j) z[j] = minv[j] * r[j];
            double rz_new = 0.0;
            for (std::size_t j = 0; j < n; ++j) rz_new += r[j] * z[j];
            const double beta = rz_new / rz;
            rz = rz_new;
            for (std::size_t j = 0; j < n; ++j) p[j] = z[j] + beta * p[j];
        }
        return it;
    }
};

} // namespace inv
