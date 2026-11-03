#pragma once

#include "voxel_grid.hpp"
#include "forward_kernel.hpp"
#include "focusing_metrics.hpp"
#include "gravity_operator.hpp"
#include "observation.hpp"
#include "focusing_solver.hpp"
#include "pfld/interface_mesh.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <functional>
#include <iostream>
#include <fstream>
#include <filesystem>

namespace inv {

struct InversionConfig {
    std::size_t nx{64};
    std::size_t ny{64};
    std::size_t nz{32};
    double dx{50.0}; // meters
    double dy{50.0};
    double dz{50.0};
    double x0{0.0};
    double y0{0.0};
    double z0{0.0};

    // Coarse-to-fine multi-scale levels (1..3, default 3: factors 4 -> 2 -> 1)
    int coarse_to_fine_levels{3};

    // ---- Zhdanov focusing -------------------------------------------------------------
    // focusing_e / focusing_e_grad are ABSOLUTE floors [g/cm^3] for the focusing thresholds.
    // The thresholds themselves are RELATIVE to the current model amplitude and annealed
    // geometrically from focusing_e_start_rel (level 0, first iteration) to focusing_e_end_rel
    // (finest level, last iteration) so the schedule is independent of density units.
    double focusing_e{1e-4};
    double focusing_e_grad{1e-4};
    double focusing_e_start_rel{1.0};
    double focusing_e_end_rel{0.005};
    bool focusing_enabled{true};     ///< false: plain depth-weighted Tikhonov L2 baseline
    double ms_weight{1.0};
    double mgs_weight{1.0};
    double depth_weight_exponent{0.75}; ///< w = (S_j/S_max)^gamma (Li-Oldenburg theory: 1; 0.75 tuned on the prism benchmark)

    // alpha_reg: initial DIMENSIONLESS regularisation ratio (regularisation vs data Hessian).
    // The absolute alpha is cooled by the discrepancy principle (see FocusingSolver).
    double alpha_reg{1.0};

    // ---- IRLS / CG ---------------------------------------------------------------------
    int irls_iterations{10};         ///< iterations on the finest level (+3 per coarser level)
    int cg_max_iterations{80};
    double cg_tolerance{1e-4};

    // ---- Data --------------------------------------------------------------------------
    double noise_sigma{0.0};         ///< [mGal]; <= 0 -> noise_rel * max|d|
    double noise_rel{0.02};
    double obs_height{10.0};         ///< stations are this far above the (local) surface [m]

    // ---- Adaptive refinement / prior -------------------------------------------------------
    bool adaptive_refinement{true};  ///< refine only the focused support (+1 cell halo)
    double refine_threshold{0.1};    ///< support = |m| >= threshold * max|m|
    bool prior_as_reference{true};   ///< apply focusing relative to the (scaled) prior model
    bool scale_prior{true};          ///< least-squares rescale of the prior to the data

    // Physical bounds for density contrast (g/cm^3)
    double min_density{-0.5};
    double max_density{0.6};

    // Max entries of the explicit sensitivity matrix used when stations are not on a flat plane.
    double dense_max_entries{30e6};

    // When false (default), pfld::InterfaceMesh is the primary forward operator across all
    // levels and topography configurations (triangular DEM relief + shared horizontal quad interfaces).
    // When true, uses ToeplitzGravityOperator (FFT) solely as a benchmark baseline on flat grids.
    bool use_fft_solver_benchmark{false};

    bool verbose{false};             ///< per-iteration solver trace on stderr (not serialised)

    // Reserved: a prior is supplied through InversionSetup::prior (see python/infer_prior.py).
    std::string onnx_model_path{""};

    /// Optional progress callback: return false to abort/cancel early.
    /// Args: (int level, int total_levels, int iter, int total_iters, double rms_mgal, double chi_rms)
    std::function<bool(int level, int total_levels, int iter, int total_iters, double rms_mgal, double chi_rms)> progress_callback;

    /// Optional cancel callback: return true to abort immediately.
    std::function<bool()> cancel_check;
};

struct InversionStats {
    double elapsed_time_ms{0.0};
    int levels_completed{0};
    double initial_data_rms{0.0};    ///< rms(d), i.e. misfit of the zero model [mGal]
    double final_data_rms{0.0};      ///< rms(G m - d) on the finest level [mGal]
    double final_chi_rms{0.0};       ///< weighted misfit, 1.0 == noise level
    double minimum_support{0.0};     ///< S_MS volume fraction of the final model
    double minimum_gradient_support{0.0}; ///< S_MGS face-area fraction of the final model
    bool used_neural_transformer{false};
    std::string message;
    std::vector<LevelStats> levels;
};

/**
 * @brief Inversion Setup Data exchanged between Mod3D and .inv via JSON.
 */
struct InversionSetup {
    std::string version{"1.0"};
    std::string mode{"planar"}; // "planar" or "spherical"
    InversionConfig config{};

    // Topography elevations: size (nx + 1) * (ny + 1) in meters, indexed [iy*(nx+1)+ix].
    // z points down (same axis as the grid); empty -> flat surface at z0.
    std::vector<double> topo_elevations{};

    // Surface observation stations
    std::vector<double> obs_x{};
    std::vector<double> obs_y{};
    std::vector<double> obs_z{};
    std::vector<double> obs_gz{}; // Measured vertical gravity anomaly (mGal)

    // Optional prior density model (e.g. neural network prediction), VoxelGrid order, size nx*ny*nz.
    std::vector<double> prior{};

    // Serialization
    [[nodiscard]] nlohmann::json to_json() const {
        nlohmann::json j;
        j["version"] = version;
        j["mode"] = mode;
        j["coarse_to_fine_levels"] = config.coarse_to_fine_levels;

        j["voxel_domain"] = {
            {"nx", config.nx}, {"ny", config.ny}, {"nz", config.nz},
            {"dx", config.dx}, {"dy", config.dy}, {"dz", config.dz},
            {"x0", config.x0}, {"y0", config.y0}, {"z0", config.z0}
        };

        j["parameters"] = {
            {"focusing_e", config.focusing_e},
            {"focusing_e_grad", config.focusing_e_grad},
            {"focusing_e_start_rel", config.focusing_e_start_rel},
            {"focusing_e_end_rel", config.focusing_e_end_rel},
            {"focusing_enabled", config.focusing_enabled},
            {"ms_weight", config.ms_weight},
            {"mgs_weight", config.mgs_weight},
            {"depth_weight_exponent", config.depth_weight_exponent},
            {"alpha_reg", config.alpha_reg},
            {"irls_iterations", config.irls_iterations},
            {"cg_max_iterations", config.cg_max_iterations},
            {"cg_tolerance", config.cg_tolerance},
            {"noise_sigma", config.noise_sigma},
            {"noise_rel", config.noise_rel},
            {"obs_height", config.obs_height},
            {"adaptive_refinement", config.adaptive_refinement},
            {"refine_threshold", config.refine_threshold},
            {"prior_as_reference", config.prior_as_reference},
            {"scale_prior", config.scale_prior},
            {"min_density", config.min_density},
            {"max_density", config.max_density},
            {"use_fft_solver_benchmark", config.use_fft_solver_benchmark},
            {"onnx_model_path", config.onnx_model_path}
        };

        j["topography"] = topo_elevations;
        j["observations"] = {
            {"x", obs_x},
            {"y", obs_y},
            {"z", obs_z},
            {"gz", obs_gz}
        };
        if (!prior.empty()) j["prior"] = {{"values", prior}};
        return j;
    }

    static InversionSetup from_json(const nlohmann::json& j) {
        InversionSetup s;
        if (j.contains("version")) s.version = j["version"].get<std::string>();
        if (j.contains("mode")) s.mode = j["mode"].get<std::string>();

        if (j.contains("voxel_domain")) {
            const auto& vd = j["voxel_domain"];
            s.config.nx = vd.value("nx", 64UL);
            s.config.ny = vd.value("ny", 64UL);
            s.config.nz = vd.value("nz", 32UL);
            s.config.dx = vd.value("dx", 50.0);
            s.config.dy = vd.value("dy", 50.0);
            s.config.dz = vd.value("dz", 50.0);
            s.config.x0 = vd.value("x0", 0.0);
            s.config.y0 = vd.value("y0", 0.0);
            s.config.z0 = vd.value("z0", 0.0);
        }

        if (j.contains("coarse_to_fine_levels")) {
            s.config.coarse_to_fine_levels = j["coarse_to_fine_levels"].get<int>();
        }

        if (j.contains("parameters")) {
            const auto& p = j["parameters"];
            InversionConfig& c = s.config;
            c.focusing_e = p.value("focusing_e", c.focusing_e);
            c.focusing_e_grad = p.value("focusing_e_grad", c.focusing_e_grad);
            c.focusing_e_start_rel = p.value("focusing_e_start_rel", c.focusing_e_start_rel);
            c.focusing_e_end_rel = p.value("focusing_e_end_rel", c.focusing_e_end_rel);
            c.focusing_enabled = p.value("focusing_enabled", c.focusing_enabled);
            c.ms_weight = p.value("ms_weight", c.ms_weight);
            c.mgs_weight = p.value("mgs_weight", c.mgs_weight);
            c.depth_weight_exponent = p.value("depth_weight_exponent", c.depth_weight_exponent);
            c.alpha_reg = p.value("alpha_reg", c.alpha_reg);
            c.irls_iterations = p.value("irls_iterations", c.irls_iterations);
            c.cg_max_iterations = p.value("cg_max_iterations", c.cg_max_iterations);
            c.cg_tolerance = p.value("cg_tolerance", c.cg_tolerance);
            c.noise_sigma = p.value("noise_sigma", c.noise_sigma);
            c.noise_rel = p.value("noise_rel", c.noise_rel);
            c.obs_height = p.value("obs_height", c.obs_height);
            c.adaptive_refinement = p.value("adaptive_refinement", c.adaptive_refinement);
            c.refine_threshold = p.value("refine_threshold", c.refine_threshold);
            c.prior_as_reference = p.value("prior_as_reference", c.prior_as_reference);
            c.scale_prior = p.value("scale_prior", c.scale_prior);
            c.min_density = p.value("min_density", c.min_density);
            c.max_density = p.value("max_density", c.max_density);
            c.use_fft_solver_benchmark = p.value("use_fft_solver_benchmark", c.use_fft_solver_benchmark);
            c.onnx_model_path = p.value("onnx_model_path", c.onnx_model_path);
        }

        if (j.contains("topography") && j["topography"].is_array()) {
            s.topo_elevations = j["topography"].get<std::vector<double>>();
        }

        if (j.contains("observations")) {
            const auto& obs = j["observations"];
            if (obs.contains("x")) s.obs_x = obs["x"].get<std::vector<double>>();
            if (obs.contains("y")) s.obs_y = obs["y"].get<std::vector<double>>();
            if (obs.contains("z")) s.obs_z = obs["z"].get<std::vector<double>>();
            if (obs.contains("gz")) s.obs_gz = obs["gz"].get<std::vector<double>>();
        }

        if (j.contains("prior") && j["prior"].contains("values") && j["prior"]["values"].is_array()) {
            s.prior = j["prior"]["values"].get<std::vector<double>>();
        }
        return s;
    }
};

/**
 * @brief Inversion Result Data serialized for Mod3D import.
 */
struct InversionResult {
    std::string version{"1.0"};
    InversionStats stats{};
    VoxelGrid<double> density_grid{};
    std::vector<double> modeled_gz{}; ///< Modeled vertical gravity anomaly (mGal) at column stations

    [[nodiscard]] nlohmann::json to_json() const {
        nlohmann::json j;
        j["version"] = version;
        j["elapsed_ms"] = stats.elapsed_time_ms;
        j["levels_completed"] = stats.levels_completed;
        j["initial_rms"] = stats.initial_data_rms;
        j["final_rms"] = stats.final_data_rms;
        j["final_chi_rms"] = stats.final_chi_rms;
        j["minimum_support"] = stats.minimum_support;
        j["minimum_gradient_support"] = stats.minimum_gradient_support;
        j["used_prior"] = stats.used_neural_transformer;
        j["message"] = stats.message;
        j["modeled_gz"] = modeled_gz;

        nlohmann::json lv = nlohmann::json::array();
        for (const auto& l : stats.levels) {
            lv.push_back({
                {"level", l.level}, {"nx", l.nx}, {"ny", l.ny}, {"nz", l.nz},
                {"active_cells", l.active_cells}, {"irls_iterations", l.irls_iterations},
                {"cg_iterations", l.cg_iterations}, {"chi_rms", l.chi_rms}, {"rms_mgal", l.rms_mgal},
                {"rho_end", l.rho_end}, {"e_end", l.e_end},
                {"ms_volume_fraction", l.ms_volume_fraction},
                {"mgs_area_fraction", l.mgs_area_fraction}, {"time_ms", l.time_ms}
            });
        }
        j["levels"] = lv;

        j["grid"] = {
            {"nx", density_grid.nx()},
            {"ny", density_grid.ny()},
            {"nz", density_grid.nz()},
            {"dx", density_grid.dx()},
            {"dy", density_grid.dy()},
            {"dz", density_grid.dz()},
            {"x0", density_grid.x0()},
            {"y0", density_grid.y0()},
            {"z0", density_grid.z0()},
            {"values", density_grid.data()}
        };
        return j;
    }

    static InversionResult from_json(const nlohmann::json& j) {
        InversionResult r;
        if (j.contains("version")) r.version = j["version"].get<std::string>();
        if (j.contains("elapsed_ms")) r.stats.elapsed_time_ms = j["elapsed_ms"].get<double>();
        if (j.contains("levels_completed")) r.stats.levels_completed = j["levels_completed"].get<int>();
        if (j.contains("initial_rms")) r.stats.initial_data_rms = j["initial_rms"].get<double>();
        if (j.contains("final_rms")) r.stats.final_data_rms = j["final_rms"].get<double>();
        if (j.contains("final_chi_rms")) r.stats.final_chi_rms = j["final_chi_rms"].get<double>();
        if (j.contains("minimum_support")) r.stats.minimum_support = j["minimum_support"].get<double>();
        if (j.contains("minimum_gradient_support")) r.stats.minimum_gradient_support = j["minimum_gradient_support"].get<double>();
        if (j.contains("used_prior")) r.stats.used_neural_transformer = j["used_prior"].get<bool>();
        if (j.contains("message")) r.stats.message = j["message"].get<std::string>();

        if (j.contains("levels") && j["levels"].is_array()) {
            for (const auto& l : j["levels"]) {
                LevelStats s;
                s.level = l.value("level", 0);
                s.nx = l.value("nx", 0UL);
                s.ny = l.value("ny", 0UL);
                s.nz = l.value("nz", 0UL);
                s.active_cells = l.value("active_cells", 0UL);
                s.irls_iterations = l.value("irls_iterations", 0);
                s.cg_iterations = l.value("cg_iterations", 0);
                s.chi_rms = l.value("chi_rms", 0.0);
                s.rms_mgal = l.value("rms_mgal", 0.0);
                s.rho_end = l.value("rho_end", 0.0);
                s.e_end = l.value("e_end", 0.0);
                s.ms_volume_fraction = l.value("ms_volume_fraction", 0.0);
                s.mgs_area_fraction = l.value("mgs_area_fraction", 0.0);
                s.time_ms = l.value("time_ms", 0.0);
                r.stats.levels.push_back(s);
            }
        }

        if (j.contains("grid")) {
            const auto& g = j["grid"];
            const size_t nx = g.value("nx", 0UL);
            const size_t ny = g.value("ny", 0UL);
            const size_t nz = g.value("nz", 0UL);
            const double dx = g.value("dx", 50.0);
            const double dy = g.value("dy", 50.0);
            const double dz = g.value("dz", 50.0);
            const double x0 = g.value("x0", 0.0);
            const double y0 = g.value("y0", 0.0);
            const double z0 = g.value("z0", 0.0);
            r.density_grid = VoxelGrid<double>(nx, ny, nz, x0, y0, z0, dx, dy, dz, 0.0);
            if (g.contains("values") && g["values"].is_array()) {
                r.density_grid.data() = g["values"].get<std::vector<double>>();
            }
        }

        if (j.contains("modeled_gz") && j["modeled_gz"].is_array()) {
            r.modeled_gz = j["modeled_gz"].get<std::vector<double>>();
        }
        return r;
    }
};

/**
 * @brief High-Level 3D Potential Field Focusing Inversion Engine.
 *
 * Pipeline (per call):
 *  1. Observations are reduced to column data (one station per fine cell column).
 *  2. Up to 3 levels (cell factors 4 -> 2 -> 1) are solved coarse-to-fine. Each level
 *     block-averages the data and (optionally) the prior, builds its own forward operator
 *     (matrix-free FFT block-Toeplitz for a flat observation plane, explicit matrix for draped
 *     stations), and runs the Portniaguine-Zhdanov IRLS focusing solver (FocusingSolver).
 *  3. The solution of level l is prolongated (piecewise-constant => G_coarse = G_fine P) and warm
 *     starts level l+1. With adaptive_refinement, finer levels only update the focused support
 *     (+1 cell halo) of the previous level; everything else stays at the background.
 *  4. An optional prior (neural-network prediction, via InversionSetup::prior) is rescaled to the data
 *     and used as start model and as the reference of the focusing stabilisers.
 *
 * Topography: cells whose centre lies above the DEM are inactive; stations are draped at
 * `obs_height` above the local surface. The FFT fast path assumes a FLAT observation plane; for
 * non-flat topography an explicit sensitivity matrix is used while it fits `dense_max_entries`,
 * otherwise the mean surface is used (reported in InversionStats::message).
 *
 * PLANAR MODELLING ONLY: vertical-facet elimination (and hence the Toeplitz/prism kernels here,
 * which assume a uniform plumb line) is valid for planar Cartesian grids, not for spherical Earth
 * modelling. See pfld::InterfaceMesh.
 */
class InversionEngine {
public:
    explicit InversionEngine(InversionConfig config = {})
        : config_(config) {}

    const InversionConfig& config() const noexcept { return config_; }
    void set_config(const InversionConfig& cfg) noexcept { config_ = cfg; }

    /**
     * @brief Inverts a regular observation raster covering the whole domain.
     * @param obs_field raster indexed [iy * obs_nx + ix] (x fastest) in mGal.
     */
    bool invert_gravity_field(const std::vector<double>& obs_field,
                              std::size_t obs_nx, std::size_t obs_ny,
                              VoxelGrid<double>& out_model,
                              InversionStats& stats,
                              const std::vector<double>& topo_elevations = {},
                              const std::vector<double>* prior = nullptr)
    {
        if (obs_field.size() != obs_nx * obs_ny || obs_nx == 0 || obs_ny == 0) {
            stats.message = "invalid observation raster";
            return false;
        }
        std::vector<double> cols;
        observation::raster_to_columns(obs_field, obs_nx, obs_ny, config_.nx, config_.ny, cols);
        return invert_columns(cols, out_model, stats, topo_elevations, {}, prior);
    }

    /**
     * @brief Core entry point: data given as one value per fine cell column (index ix*ny+iy).
     * @param obs_z_cols optional per-column station elevations (z down); empty -> obs_height above surface.
     * @param prior optional density prior, size nx*ny*nz in VoxelGrid order.
     */
    bool invert_columns(const std::vector<double>& d_fine,
                        VoxelGrid<double>& out_model,
                        InversionStats& stats,
                        const std::vector<double>& topo_nodes = {},
                        const std::vector<double>& obs_z_cols = {},
                        const std::vector<double>* prior = nullptr,
                        std::vector<double>* out_modeled_gz = nullptr)
    {
        const auto t_start = std::chrono::high_resolution_clock::now();
        const InversionConfig& c = config_;
        if (c.nx < 2 || c.ny < 2 || c.nz < 1 || d_fine.size() != c.nx * c.ny) {
            stats.message = "grid/data size mismatch";
            return false;
        }
        const bool has_prior = prior && prior->size() == c.nx * c.ny * c.nz;

        GridGeometry gf{c.nx, c.ny, c.nz, c.x0, c.y0, c.z0, c.dx, c.dy, c.dz};
        const int n_levels = std::clamp(c.coarse_to_fine_levels, 1, 3);
        const std::vector<std::size_t> targets =
            (n_levels == 1) ? std::vector<std::size_t>{1}
          : (n_levels == 2) ? std::vector<std::size_t>{2, 1}
                            : std::vector<std::size_t>{4, 2, 1};

        // noise level
        double dmax = 0.0, dsum2 = 0.0;
        for (double v : d_fine) { dmax = std::max(dmax, std::abs(v)); dsum2 += v * v; }
        const double sigma = (c.noise_sigma > 0.0) ? c.noise_sigma : std::max(c.noise_rel * dmax, 1e-9);

        stats = InversionStats{};
        stats.initial_data_rms = std::sqrt(dsum2 / static_cast<double>(d_fine.size()));
        stats.used_neural_transformer = has_prior;

        const bool has_topo = topo_nodes.size() == (c.nx + 1) * (c.ny + 1);
        std::vector<double> prior_fine;
        if (has_prior) prior_fine = *prior;

        std::vector<double> m_prev;
        GridGeometry g_prev{};
        double rho = c.alpha_reg;
        std::string note;
        std::vector<double> m_level;

        for (int l = 0; l < n_levels; ++l) {
            if (c.cancel_check && c.cancel_check()) {
                stats.message = "Inversion cancelled by user";
                return false;
            }
            const std::size_t fx = choose_factor(c.nx, targets[l]);
            const std::size_t fy = choose_factor(c.ny, targets[l]);
            const std::size_t fz = choose_factor(c.nz, targets[l]);
            GridGeometry g{c.nx / fx, c.ny / fy, c.nz / fz, c.x0, c.y0, c.z0,
                           c.dx * static_cast<double>(fx), c.dy * static_cast<double>(fy), c.dz * static_cast<double>(fz)};

            // ---- level data ------------------------------------------------------------------
            std::vector<double> d_level;
            observation::block_average_columns(d_fine, c.nx, c.ny, fx, fy, d_level);
            std::vector<double> oz_level;
            if (obs_z_cols.size() == c.nx * c.ny) {
                observation::block_average_columns(obs_z_cols, c.nx, c.ny, fx, fy, oz_level);
            }

            // ---- level topography nodes + active mask ----------------------------------------------
            std::vector<double> topo_level;
            if (has_topo) {
                topo_level.resize((g.nx + 1) * (g.ny + 1));
                for (std::size_t iy = 0; iy <= g.ny; ++iy) {
                    for (std::size_t ix = 0; ix <= g.nx; ++ix) {
                        topo_level[iy * (g.nx + 1) + ix] = topo_nodes[(iy * fy) * (c.nx + 1) + ix * fx];
                    }
                }
            }
            std::vector<double> col_surface(g.columns(), c.z0);
            double zmin = c.z0, zmax = c.z0;
            if (has_topo) {
                zmin = 1e300; zmax = -1e300;
                for (std::size_t ix = 0; ix < g.nx; ++ix) {
                    for (std::size_t iy = 0; iy < g.ny; ++iy) {
                        const double z = column_surface_z(topo_level, g.nx, g.ny, ix, iy, c.z0);
                        col_surface[ix * g.ny + iy] = z;
                        zmin = std::min(zmin, z);
                        zmax = std::max(zmax, z);
                    }
                }
            }
            const bool flat = (zmax - zmin) < 1e-6;

            std::vector<char> active(g.cells(), 1);
            if (has_topo) {
                for (std::size_t ix = 0; ix < g.nx; ++ix) {
                    for (std::size_t iy = 0; iy < g.ny; ++iy) {
                        const double zs = col_surface[ix * g.ny + iy];
                        for (std::size_t iz = 0; iz < g.nz; ++iz) {
                            if (g.zc(iz) < zs) active[g.cell_index(ix, iy, iz)] = 0;
                        }
                    }
                }
            }

            const std::vector<char> topo_active = active;

            // ---- adaptive refinement: only the focused support of the previous level -----------------
            if (l > 0 && c.adaptive_refinement) {
                adapt_active(m_prev, g_prev, g, active);
            }

            // ---- forward operator ------------------------------------------------------------------
            std::unique_ptr<LinearOperator> op;
            if (c.use_fft_solver_benchmark && flat) {
                double z_obs = zmin - c.obs_height;
                if (!oz_level.empty()) {
                    double s = 0.0;
                    for (double v : oz_level) s += v;
                    z_obs = s / static_cast<double>(oz_level.size());
                }
                op = std::make_unique<ToeplitzGravityOperator>(g, z_obs);
            } else {
                std::vector<Point3D<double>> obs;
                obs.reserve(g.columns());
                for (std::size_t ix = 0; ix < g.nx; ++ix) {
                    for (std::size_t iy = 0; iy < g.ny; ++iy) {
                        const std::size_t col = ix * g.ny + iy;
                        const double z = oz_level.empty() ? col_surface[col] - c.obs_height : oz_level[col];
                        obs.emplace_back(g.xc(ix), g.yc(iy), z);
                    }
                }
                op = std::make_unique<PfldInterfaceMeshOperator>(g, obs, topo_level, c.dense_max_entries);
            }

            // ---- prior (restricted to this level) -----------------------------------------------
            std::vector<double> prior_level;
            if (has_prior) {
                observation::block_average_cells(prior_fine, gf, fx, fy, fz, prior_level);
                for (std::size_t j = 0; j < prior_level.size(); ++j) if (!active[j]) prior_level[j] = 0.0;
                if (l == 0 && c.scale_prior) {
                    // 1-D least-squares amplitude of the prior against the data.
                    op->set_active(active);
                    std::vector<double> gp;
                    op->apply(prior_level, gp);
                    double num = 0.0, den = 0.0;
                    for (std::size_t i = 0; i < gp.size(); ++i) { num += gp[i] * d_level[i]; den += gp[i] * gp[i]; }
                    if (den > 0.0) {
                        const double s = std::clamp(num / den, 0.05, 20.0);
                        for (auto& v : prior_fine) v *= s;
                        for (auto& v : prior_level) v *= s;
                    }
                }
                for (auto& v : prior_level) v = std::clamp(v, c.min_density, c.max_density);
            }

            // ---- warm start ----------------------------------------------------------------------
            if (l == 0) {
                m_level = has_prior ? prior_level : std::vector<double>(g.cells(), 0.0);
            } else {
                observation::prolongate_cells(m_prev, g_prev, g, m_level);
                for (std::size_t j = 0; j < m_level.size(); ++j) if (!active[j]) m_level[j] = 0.0;
            }

            // ---- solve ------------------------------------------------------------------------------
            SolverParams sp;
            sp.irls_iterations = c.irls_iterations + 3 * (n_levels - 1 - l);
            sp.cg_max_iterations = c.cg_max_iterations;
            sp.cg_tolerance = c.cg_tolerance;
            const double es = std::max(1e-6, c.focusing_e_start_rel);
            const double ef = std::max(1e-6, c.focusing_e_end_rel);
            sp.e_start_rel = es * std::pow(ef / es, static_cast<double>(l) / n_levels);
            sp.e_end_rel = es * std::pow(ef / es, static_cast<double>(l + 1) / n_levels);
            sp.e_abs_min = std::max(c.focusing_e, 1e-12);
            sp.rho_start = rho;
            sp.sigma = sigma;
            sp.focusing = c.focusing_enabled;
            sp.ms_weight = c.ms_weight;
            sp.mgs_weight = c.mgs_weight;
            sp.depth_exponent = c.depth_weight_exponent;
            sp.min_density = c.min_density;
            sp.max_density = c.max_density;
            sp.density_scale = 0.5 * (c.max_density - c.min_density);
            sp.verbose = c.verbose;

            sp.progress_callback = [&](int iter, int total_iters, double rms, double chi) -> bool {
                if (c.progress_callback) {
                    return c.progress_callback(l + 1, n_levels, iter, total_iters, rms, chi);
                }
                return true;
            };
            sp.cancel_check = c.cancel_check;

            LevelStats ls;
            ls.level = l;
            ls.nx = g.nx; ls.ny = g.ny; ls.nz = g.nz;
            const std::vector<double>* ref = (has_prior && c.prior_as_reference) ? &prior_level : nullptr;
            if (!FocusingSolver::solve(g, *op, d_level, m_level, active, ref, sp, ls)) {
                stats.message = "Inversion cancelled by user";
                return false;
            }
            rho = ls.rho_end;

            // Compute modeled gravity on finest level if requested
            if (l == n_levels - 1 && out_modeled_gz) {
                out_modeled_gz->resize(g.columns());
                op->apply(m_level, *out_modeled_gz);
            }

            // Focusing diagnostics (physical units), normalised by the topography-active domain so
            // that adaptive refinement does not change the denominator.
            {
                VoxelGrid<double> gm(g.nx, g.ny, g.nz, g.x0, g.y0, g.z0, g.dx, g.dy, g.dz, 0.0);
                gm.data() = m_level;
                using FE = FocusingEvaluator<double>;
                const double vol = FE::active_volume(gm, &topo_active);
                const double area = FE::interface_area(gm, &topo_active);
                ls.ms_volume_fraction = vol > 0 ? FE::compute_minimum_support_volume(gm, ls.e_end, 0.0, &topo_active) / vol : 0.0;
                ls.mgs_area_fraction = area > 0 ? FE::compute_minimum_gradient_support_area(gm, ls.e_end, &topo_active) / area : 0.0;
            }
            stats.levels.push_back(ls);

            m_prev = m_level;
            g_prev = g;
        }

        out_model = VoxelGrid<double>(c.nx, c.ny, c.nz, c.x0, c.y0, c.z0, c.dx, c.dy, c.dz, 0.0);
        out_model.data() = m_prev;

        stats.levels_completed = n_levels;
        stats.final_data_rms = stats.levels.back().rms_mgal;
        stats.final_chi_rms = stats.levels.back().chi_rms;
        stats.minimum_support = stats.levels.back().ms_volume_fraction;
        stats.minimum_gradient_support = stats.levels.back().mgs_area_fraction;
        stats.message = std::string("Completed ") + std::to_string(n_levels) + "-level coarse-to-fine " +
                        (c.focusing_enabled ? "focusing (IRLS MS+MGS)" : "Tikhonov L2 baseline") + " inversion using " +
                        (c.use_fft_solver_benchmark ? "FFT Toeplitz benchmark operator" : "pfld::InterfaceMesh operator") + note;
        stats.elapsed_time_ms = std::chrono::duration<double, std::milli>(
            std::chrono::high_resolution_clock::now() - t_start).count();
        return true;
    }

    /**
     * @brief Executes inversion directly from a Mod3D InversionSetup.
     * Scattered stations are bin-averaged into the finest cell columns (see observation.hpp).
     */
    bool run_setup(const InversionSetup& setup, InversionResult& result) {
        config_ = setup.config;
        const GridGeometry gf{config_.nx, config_.ny, config_.nz, config_.x0, config_.y0, config_.z0,
                              config_.dx, config_.dy, config_.dz};
        std::vector<double> cols;
        const std::size_t n_used = observation::scattered_to_columns(setup.obs_x, setup.obs_y, setup.obs_gz, gf, cols);
        if (n_used == 0) {
            result.stats.message = "no observation inside the model domain";
            return false;
        }
        std::vector<double> oz;
        if (setup.obs_z.size() == setup.obs_gz.size() && !setup.obs_z.empty()) {
            observation::scattered_to_columns(setup.obs_x, setup.obs_y, setup.obs_z, gf, oz);
        }
        const std::vector<double>* prior = setup.prior.empty() ? nullptr : &setup.prior;
        return invert_columns(cols, result.density_grid, result.stats, setup.topo_elevations, oz, prior, &result.modeled_gz);
    }

    // JSON file helpers for Mod3D export/import
    static bool save_setup_to_json(const std::filesystem::path& path, const InversionSetup& setup) {
        std::ofstream out(path);
        if (!out) return false;
        out << setup.to_json().dump(2);
        return true;
    }

    static bool load_setup_from_json(const std::filesystem::path& path, InversionSetup& setup) {
        std::ifstream in(path);
        if (!in) return false;
        nlohmann::json j;
        in >> j;
        setup = InversionSetup::from_json(j);
        return true;
    }

    static bool save_result_to_json(const std::filesystem::path& path, const InversionResult& result) {
        std::ofstream out(path);
        if (!out) return false;
        out << result.to_json().dump(2);
        return true;
    }

    static bool load_result_from_json(const std::filesystem::path& path, InversionResult& result) {
        std::ifstream in(path);
        if (!in) return false;
        nlohmann::json j;
        in >> j;
        result = InversionResult::from_json(j);
        return true;
    }

private:
    InversionConfig config_;

    /// Largest f in {target, target/2, ..., 1} that divides n and leaves at least min(n,2) cells.
    static std::size_t choose_factor(std::size_t n, std::size_t target) {
        for (std::size_t f = target; f >= 1; f /= 2) {
            if (n % f == 0 && n / f >= std::min<std::size_t>(n, 2)) return f;
            if (f == 1) break;
        }
        return 1;
    }

    /// Restricts `active` (on the finer grid) to the focused support of the previous (coarser) level
    /// dilated by one coarse cell. Skipped when the support is not sparse (> 70 %) or empty.
    void adapt_active(const std::vector<double>& m_coarse, const GridGeometry& gc,
                      const GridGeometry& gf, std::vector<char>& active) const
    {
        double mx = 0.0;
        for (double v : m_coarse) mx = std::max(mx, std::abs(v));
        if (mx <= 0.0) return;
        const double thr = config_.refine_threshold * mx;

        std::vector<char> sup(gc.cells(), 0);
        for (std::size_t ix = 0; ix < gc.nx; ++ix)
            for (std::size_t iy = 0; iy < gc.ny; ++iy)
                for (std::size_t iz = 0; iz < gc.nz; ++iz)
                    if (std::abs(m_coarse[gc.cell_index(ix, iy, iz)]) >= thr) sup[gc.cell_index(ix, iy, iz)] = 1;

        std::vector<char> dil(gc.cells(), 0);
        std::size_t count = 0;
        for (long ix = 0; ix < static_cast<long>(gc.nx); ++ix)
            for (long iy = 0; iy < static_cast<long>(gc.ny); ++iy)
                for (long iz = 0; iz < static_cast<long>(gc.nz); ++iz) {
                    bool on = false;
                    for (long a = -1; a <= 1 && !on; ++a)
                        for (long b = -1; b <= 1 && !on; ++b)
                            for (long e = -1; e <= 1 && !on; ++e) {
                                const long jx = ix + a, jy = iy + b, jz = iz + e;
                                if (jx < 0 || jy < 0 || jz < 0 || jx >= static_cast<long>(gc.nx) ||
                                    jy >= static_cast<long>(gc.ny) || jz >= static_cast<long>(gc.nz)) continue;
                                if (sup[gc.cell_index(jx, jy, jz)]) on = true;
                            }
                    if (on) { dil[gc.cell_index(ix, iy, iz)] = 1; ++count; }
                }
        if (count == 0 || static_cast<double>(count) > 0.7 * static_cast<double>(gc.cells())) return;

        const std::size_t fx = gf.nx / gc.nx, fy = gf.ny / gc.ny, fz = gf.nz / gc.nz;
        for (std::size_t ix = 0; ix < gf.nx; ++ix)
            for (std::size_t iy = 0; iy < gf.ny; ++iy)
                for (std::size_t iz = 0; iz < gf.nz; ++iz)
                    if (!dil[gc.cell_index(ix / fx, iy / fy, iz / fz)]) active[gf.cell_index(ix, iy, iz)] = 0;
    }
};

} // namespace inv
