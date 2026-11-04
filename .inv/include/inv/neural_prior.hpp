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
        std::error_code ec;
        auto cur = std::filesystem::current_path(ec);
        if (!ec) {
            for (int i = 0; i < 6; ++i) {
                auto candidate = cur / ".inv" / "python" / "models" / "inv_swin3d.onnx";
                if (std::filesystem::exists(candidate, ec) && std::filesystem::exists(candidate.string() + ".data", ec)) {
                    return std::filesystem::absolute(candidate).string();
                }
                auto candidate2 = cur / "models" / "inv_swin3d.onnx";
                if (std::filesystem::exists(candidate2, ec) && std::filesystem::exists(candidate2.string() + ".data", ec)) {
                    return std::filesystem::absolute(candidate2).string();
                }
                if (!cur.has_parent_path() || cur == cur.parent_path()) break;
                cur = cur.parent_path();
            }
        }

        const std::vector<std::string> candidates = {
            ".inv/python/models/inv_swin3d.onnx",
            "../.inv/python/models/inv_swin3d.onnx",
            "../../.inv/python/models/inv_swin3d.onnx",
            "../../../.inv/python/models/inv_swin3d.onnx",
            "models/inv_swin3d.onnx",
            "../models/inv_swin3d.onnx"
        };
        for (const auto& p : candidates) {
            if (std::filesystem::exists(p, ec) && std::filesystem::exists(p + ".data", ec)) {
                return std::filesystem::absolute(p).string();
            }
        }
        return "";
    }

    /**
     * @brief Evaluates the 3D Swin Transformer ONNX model on a 2D surface gravity grid.
     *
     * @param obs_cols Observation data arranged in columns, size nx * ny (index: ix * ny + iy)
     * @param nx Number of columns in X
     * @param ny Number of rows in Y
     * @param target_nz Target vertical layers in output VoxelGrid
     * @param x0, y0, z0 Origin coordinates
     * @param dx, dy, dz Spacings
     * @param out_grid Resulting 3D VoxelGrid with predicted density contrast (g/cm^3)
     * @param err_msg Output error message if inference fails
     * @param custom_model_path Optional user-specified path to the .onnx model
     * @return true on success, false on failure
     */
    static bool predict(const std::vector<double>& obs_cols,
                        std::size_t nx, std::size_t ny,
                        std::size_t target_nz,
                        double x0, double y0, double z0,
                        double dx, double dy, double dz,
                        VoxelGrid<double>& out_grid,
                        std::string& err_msg,
                        const std::string& custom_model_path = "")
    {
#if !defined(MOD3D_HAS_ONNX) || !MOD3D_HAS_ONNX
        (void)obs_cols;
        (void)nx;
        (void)ny;
        (void)target_nz;
        (void)x0;
        (void)y0;
        (void)z0;
        (void)dx;
        (void)dy;
        (void)dz;
        (void)out_grid;
        (void)custom_model_path;
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

        try {
            // 1. Initialize ONNX Runtime session
            Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "Mod3D_NeuralPrior");
            Ort::SessionOptions session_options;
            session_options.SetIntraOpNumThreads(2);
            session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

            Ort::Session session(env, model_path.c_str(), session_options);

            // 2. Prepare 64x64 input tensor matching Swin3D footprint
            constexpr std::size_t kTrainNx = 64;
            constexpr std::size_t kTrainNy = 64;
            constexpr std::size_t kTrainNz = 32;

            std::vector<float> in_buf(kTrainNx * kTrainNy, 0.0f);
            for (std::size_t ix = 0; ix < kTrainNx; ++ix) {
                double u = (kTrainNx > 1) ? static_cast<double>(ix) / (kTrainNx - 1) : 0.0;
                double src_x = u * (nx - 1);
                std::size_t x0_idx = std::clamp(static_cast<std::size_t>(src_x), std::size_t{0}, nx - 1);
                std::size_t x1_idx = std::min(x0_idx + 1, nx - 1);
                double fx = src_x - x0_idx;

                for (std::size_t iy = 0; iy < kTrainNy; ++iy) {
                    double v = (kTrainNy > 1) ? static_cast<double>(iy) / (kTrainNy - 1) : 0.0;
                    double src_y = v * (ny - 1);
                    std::size_t y0_idx = std::clamp(static_cast<std::size_t>(src_y), std::size_t{0}, ny - 1);
                    std::size_t y1_idx = std::min(y0_idx + 1, ny - 1);
                    double fy = src_y - y0_idx;

                    // Bilinear interpolation from obs_cols [src_ix * ny + src_iy]
                    double v00 = obs_cols[x0_idx * ny + y0_idx];
                    double v10 = obs_cols[x1_idx * ny + y0_idx];
                    double v01 = obs_cols[x0_idx * ny + y1_idx];
                    double v11 = obs_cols[x1_idx * ny + y1_idx];

                    double val = (1.0 - fx) * (1.0 - fy) * v00 +
                                 fx * (1.0 - fy) * v10 +
                                 (1.0 - fx) * fy * v01 +
                                 fx * fy * v11;

                    in_buf[ix * kTrainNy + iy] = static_cast<float>(val);
                }
            }

            // 3. Create input tensor [1, 1, 64, 64]
            Ort::MemoryInfo mem_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
            std::vector<int64_t> in_dims = {1, 1, static_cast<int64_t>(kTrainNx), static_cast<int64_t>(kTrainNy)};

            Ort::Value in_tensor = Ort::Value::CreateTensor<float>(
                mem_info, in_buf.data(), in_buf.size(), in_dims.data(), in_dims.size());

            const char* in_names[] = {"observation_field"};
            const char* out_names[] = {"density_volume"};

            // 4. Run inference
            auto out_tensors = session.Run(Ort::RunOptions{nullptr}, in_names, &in_tensor, 1, out_names, 1);
            if (out_tensors.empty()) {
                err_msg = "ONNX session returned no output tensor.";
                return false;
            }

            const float* out_data = out_tensors[0].GetTensorData<float>();
            // Output shape is [1, 1, 32, 64, 64], i.e., [batch, channel, iz, ix, iy]

            // 5. Initialize output VoxelGrid (nx, ny, target_nz)
            out_grid = VoxelGrid<double>(nx, ny, target_nz, x0, y0, z0, dx, dy, dz, 0.0);

            // 6. Trilinear resample from (32, 64, 64) [iz, ix, iy] to target (nx, ny, target_nz)
            for (std::size_t ix = 0; ix < nx; ++ix) {
                double u = (nx > 1) ? static_cast<double>(ix) / (nx - 1) : 0.0;
                double src_x = u * (kTrainNx - 1);
                std::size_t sx0 = std::clamp(static_cast<std::size_t>(src_x), std::size_t{0}, kTrainNx - 1);
                std::size_t sx1 = std::min(sx0 + 1, kTrainNx - 1);
                double fx = src_x - sx0;

                for (std::size_t iy = 0; iy < ny; ++iy) {
                    double v = (ny > 1) ? static_cast<double>(iy) / (ny - 1) : 0.0;
                    double src_y = v * (kTrainNy - 1);
                    std::size_t sy0 = std::clamp(static_cast<std::size_t>(src_y), std::size_t{0}, kTrainNy - 1);
                    std::size_t sy1 = std::min(sy0 + 1, kTrainNy - 1);
                    double fy = src_y - sy0;

                    for (std::size_t iz = 0; iz < target_nz; ++iz) {
                        double w = (target_nz > 1) ? static_cast<double>(iz) / (target_nz - 1) : 0.0;
                        double src_z = w * (kTrainNz - 1);
                        std::size_t sz0 = std::clamp(static_cast<std::size_t>(src_z), std::size_t{0}, kTrainNz - 1);
                        std::size_t sz1 = std::min(sz0 + 1, kTrainNz - 1);
                        double fz = src_z - sz0;

                        // Tensor index in [iz, ix, iy]: (sz * kTrainNx + sx) * kTrainNy + sy
                        auto sample = [&](std::size_t z, std::size_t x, std::size_t y) -> double {
                            return static_cast<double>(out_data[(z * kTrainNx + x) * kTrainNy + y]);
                        };

                        double c000 = sample(sz0, sx0, sy0);
                        double c100 = sample(sz0, sx1, sy0);
                        double c010 = sample(sz0, sx0, sy1);
                        double c110 = sample(sz0, sx1, sy1);
                        double c001 = sample(sz1, sx0, sy0);
                        double c101 = sample(sz1, sx1, sy0);
                        double c011 = sample(sz1, sx0, sy1);
                        double c111 = sample(sz1, sx1, sy1);

                        double c00 = c000 * (1.0 - fx) + c100 * fx;
                        double c10 = c010 * (1.0 - fx) + c110 * fx;
                        double c01 = c001 * (1.0 - fx) + c101 * fx;
                        double c11 = c011 * (1.0 - fx) + c111 * fx;

                        double c0 = c00 * (1.0 - fy) + c10 * fy;
                        double c1 = c01 * (1.0 - fy) + c11 * fy;

                        double density = c0 * (1.0 - fz) + c1 * fz;

                        out_grid(ix, iy, iz) = density;
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
