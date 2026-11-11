#pragma once

#include "voxel_grid.hpp"
#include <vector>
#include <string>
#include <filesystem>
#include <cmath>
#include <algorithm>
#include <iostream>

#if defined(MOD3D_HAS_ONNX) && MOD3D_HAS_ONNX
#if __has_include(<onnxruntime_cxx_api.h>)
#include <onnxruntime_cxx_api.h>
#elif __has_include(<onnxruntime/onnxruntime_cxx_api.h>)
#include <onnxruntime/onnxruntime_cxx_api.h>
#endif
#endif

namespace inv {

class NeuralPriorInference {
public:
    static bool is_available() noexcept {
#if defined(MOD3D_HAS_ONNX) && MOD3D_HAS_ONNX
        return true;
#else
        return false;
#endif
    }

    static std::string default_model_path() {
        // The model may be a single .onnx file or .onnx + external .onnx.data weights.
        std::error_code ec;
        auto cur = std::filesystem::current_path(ec);
        if (!ec) {
            for (int i = 0; i < 6; ++i) {
                for (const auto& candidate : {cur / ".inv" / "python" / "models" / "inv_swin3d.onnx",
                                              cur / "models" / "inv_swin3d.onnx"}) {
                    if (std::filesystem::exists(candidate, ec)) {
                        return std::filesystem::absolute(candidate).string();
                    }
                }
                if (!cur.has_parent_path() || cur == cur.parent_path()) break;
                cur = cur.parent_path();
            }
        }
        return "";
    }

    /// Training geometry of the network (see .inv/python/dataset.py).
    static constexpr double kTrainCell = 50.0;      ///< training cell size [m]
    static constexpr double kMaxReliefFrac = 0.25;  ///< max terrain relief / domain depth seen in training

    /**
     * @brief Evaluates the 3D transformer ONNX model on a 2D surface gravity grid.
     *
     * Model input  (1, 2, Hn, Wn): [gz (mGal), terrain depth below the grid top / domain depth].
     * Model output (1, 1, Dn, Hn, Wn): density contrast (g/cm^3) for kTrainCell training cells.
     * The output is rescaled for the real domain size (gravity of a geometrically similar model
     * scales with length) and set to zero in cells whose centre lies above the terrain.
     * Legacy 1-channel models (gz only) are still accepted.
     *
     * @param obs_cols Observation data arranged in columns, size nx * ny (index: ix * ny + iy)
     * @param nx Number of columns in X
     * @param ny Number of rows in Y
     * @param target_nz Target vertical layers in output VoxelGrid
     * @param x0, y0, z0 Origin coordinates (z down, z0 = grid top)
     * @param dx, dy, dz Spacings
     * @param out_grid Resulting 3D VoxelGrid with predicted density contrast (g/cm^3)
     * @param err_msg Output error message if inference fails
     * @param custom_model_path Optional user-specified path to the .onnx model
     * @param surface_depth_cols Optional terrain depth below the grid top per column (size nx*ny,
     *        index ix * ny + iy, metres, e.g. inv::column_surface_z - z0). Empty = flat terrain.
     * @return true on success, false on failure
     */
    static bool predict(const std::vector<double>& obs_cols,
                        std::size_t nx, std::size_t ny,
                        std::size_t target_nz,
                        double x0, double y0, double z0,
                        double dx, double dy, double dz,
                        VoxelGrid<double>& out_grid,
                        std::string& err_msg,
                        const std::string& custom_model_path = "",
                        const std::vector<double>& surface_depth_cols = {})
    {
#if !defined(MOD3D_HAS_ONNX) || !MOD3D_HAS_ONNX
        (void)obs_cols; (void)nx; (void)ny; (void)target_nz;
        (void)x0; (void)y0; (void)z0; (void)dx; (void)dy; (void)dz;
        (void)out_grid; (void)custom_model_path; (void)surface_depth_cols;
        err_msg = "ONNX Runtime support is not compiled into Mod3D.";
        return false;
#else
        std::string model_path = custom_model_path.empty() ? default_model_path() : custom_model_path;
        if (model_path.empty() || !std::filesystem::exists(model_path)) {
            err_msg = "ONNX model file not found: " + (model_path.empty() ? "inv_swin3d.onnx" : model_path);
            return false;
        }
        if (obs_cols.size() != nx * ny || nx == 0 || ny == 0 || target_nz == 0) {
            err_msg = "Invalid observation grid dimensions for neural inference.";
            return false;
        }
        const bool has_terrain = surface_depth_cols.size() == nx * ny;

        try {
            Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "Mod3D_NeuralPrior");
            Ort::SessionOptions session_options;
            session_options.SetIntraOpNumThreads(2);
            session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
            Ort::Session session(env, model_path.c_str(), session_options);

            // Network footprint from the model itself (static shapes): (1, C, Hn, Wn)
            const auto in_shape = session.GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
            const auto out_shape = session.GetOutputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
            if (in_shape.size() != 4 || out_shape.size() != 5) {
                err_msg = "Unexpected ONNX model input/output rank.";
                return false;
            }
            const std::size_t kC = in_shape[1] > 0 ? static_cast<std::size_t>(in_shape[1]) : 1;
            const std::size_t kNx = in_shape[2] > 0 ? static_cast<std::size_t>(in_shape[2]) : 64;
            const std::size_t kNy = in_shape[3] > 0 ? static_cast<std::size_t>(in_shape[3]) : 64;
            const std::size_t kNz = out_shape[2] > 0 ? static_cast<std::size_t>(out_shape[2]) : 32;

            // Bilinear resample of a column field (ix * ny + iy) onto the network grid
            auto resample2d = [&](const std::vector<double>& src, float* dst) {
                for (std::size_t ix = 0; ix < kNx; ++ix) {
                    const double sx = (kNx > 1 ? static_cast<double>(ix) / (kNx - 1) : 0.0) * (nx - 1);
                    const std::size_t a0 = std::min(static_cast<std::size_t>(sx), nx - 1);
                    const std::size_t a1 = std::min(a0 + 1, nx - 1);
                    const double fx = sx - a0;
                    for (std::size_t iy = 0; iy < kNy; ++iy) {
                        const double sy = (kNy > 1 ? static_cast<double>(iy) / (kNy - 1) : 0.0) * (ny - 1);
                        const std::size_t b0 = std::min(static_cast<std::size_t>(sy), ny - 1);
                        const std::size_t b1 = std::min(b0 + 1, ny - 1);
                        const double fy = sy - b0;
                        const double v = (1 - fx) * (1 - fy) * src[a0 * ny + b0] + fx * (1 - fy) * src[a1 * ny + b0] +
                                         (1 - fx) * fy * src[a0 * ny + b1] + fx * fy * src[a1 * ny + b1];
                        dst[ix * kNy + iy] = static_cast<float>(v);
                    }
                }
            };

            const double depth_total = static_cast<double>(target_nz) * dz;
            // Amplitude calibration: gravity of a geometrically similar model scales linearly with length.
            // Multiply input gz by scale = kTrainCell / L_eff so physical contrast units (-2.67 air,
            // -1.67 water, [-1.55,+1.85] lithology bands) match 50 m training units (backward-compatible with v1).
            double scale = 1.0;
            if (kC >= 2) {
                const double eff = (nx * dx / kNx) * (ny * dy / kNy) * (depth_total / kNz);
                scale = kTrainCell / std::cbrt(eff);
            }
            std::vector<float> in_buf(kC * kNx * kNy, 0.0f);
            resample2d(obs_cols, in_buf.data());
            if (scale != 1.0) {
                const float fscale = static_cast<float>(scale);
                for (std::size_t i = 0; i < kNx * kNy; ++i) {
                    in_buf[i] *= fscale;
                }
            }
            if (kC >= 2 && has_terrain) {
                std::vector<double> t_norm(nx * ny);
                for (std::size_t i = 0; i < nx * ny; ++i) {
                    t_norm[i] = std::clamp(surface_depth_cols[i] / depth_total, 0.0, kMaxReliefFrac);
                }
                resample2d(t_norm, in_buf.data() + kNx * kNy);
            }

            Ort::MemoryInfo mem_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
            std::vector<int64_t> in_dims = {1, static_cast<int64_t>(kC), static_cast<int64_t>(kNx),
                                            static_cast<int64_t>(kNy)};
            Ort::Value in_tensor = Ort::Value::CreateTensor<float>(
                mem_info, in_buf.data(), in_buf.size(), in_dims.data(), in_dims.size());
            const char* in_names[] = {"observation_field"};
            const char* out_names[] = {"density_volume"};
            auto out_tensors = session.Run(Ort::RunOptions{nullptr}, in_names, &in_tensor, 1, out_names, 1);
            if (out_tensors.empty()) {
                err_msg = "ONNX session returned no output tensor.";
                return false;
            }
            const float* out_data = out_tensors[0].GetTensorData<float>();   // [iz, ix, iy]

            out_grid = VoxelGrid<double>(nx, ny, target_nz, x0, y0, z0, dx, dy, dz, 0.0);
            auto sample = [&](std::size_t z, std::size_t x, std::size_t y) -> double {
                return static_cast<double>(out_data[(z * kNx + x) * kNy + y]);
            };
            for (std::size_t ix = 0; ix < nx; ++ix) {
                const double sx = (nx > 1 ? static_cast<double>(ix) / (nx - 1) : 0.0) * (kNx - 1);
                const std::size_t x0i = std::min(static_cast<std::size_t>(sx), kNx - 1);
                const std::size_t x1i = std::min(x0i + 1, kNx - 1);
                const double fx = sx - x0i;
                for (std::size_t iy = 0; iy < ny; ++iy) {
                    const double sy = (ny > 1 ? static_cast<double>(iy) / (ny - 1) : 0.0) * (kNy - 1);
                    const std::size_t y0i = std::min(static_cast<std::size_t>(sy), kNy - 1);
                    const std::size_t y1i = std::min(y0i + 1, kNy - 1);
                    const double fy = sy - y0i;
                    const double surf = has_terrain ? surface_depth_cols[ix * ny + iy] : 0.0;
                    for (std::size_t iz = 0; iz < target_nz; ++iz) {
                        if ((static_cast<double>(iz) + 0.5) * dz < surf) continue;   // above terrain
                        // cell-centre mapping between the target and network depth axes
                        const double sz = std::clamp((static_cast<double>(iz) + 0.5) * kNz / target_nz - 0.5,
                                                     0.0, static_cast<double>(kNz - 1));
                        const std::size_t z0i = std::min(static_cast<std::size_t>(sz), kNz - 1);
                        const std::size_t z1i = std::min(z0i + 1, kNz - 1);
                        const double fz = sz - z0i;
                        const double c00 = sample(z0i, x0i, y0i) * (1 - fx) + sample(z0i, x1i, y0i) * fx;
                        const double c10 = sample(z0i, x0i, y1i) * (1 - fx) + sample(z0i, x1i, y1i) * fx;
                        const double c01 = sample(z1i, x0i, y0i) * (1 - fx) + sample(z1i, x1i, y0i) * fx;
                        const double c11 = sample(z1i, x0i, y1i) * (1 - fx) + sample(z1i, x1i, y1i) * fx;
                        const double c0 = c00 * (1 - fy) + c10 * fy;
                        const double c1 = c01 * (1 - fy) + c11 * fy;
                        out_grid(ix, iy, iz) = c0 * (1 - fz) + c1 * fz;
                    }
                }
            }
            return true;
        } catch (const std::exception& e) {
            err_msg = std::string("ONNX Runtime exception: ") + e.what();
            return false;
        }
#endif
    }
};

} // namespace inv
