#pragma once

#include "voxel_grid.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

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
     * Supports:
     *   - v5 (5 channels): [gz (scaled to 50 m equivalent), terr/depth, valid_mask, log(aspect), clearance/dxy]
     *     returning "density_volume" and optional "log_var_volume" (-> *out_sigma_grid = exp(0.5 * log_var))
     *   - v2..v4 (2 channels): [gz (mGal scaled), terr/depth] -> "density_volume"
     *   - legacy v1 (1 channel): [gz (mGal)] -> "density_volume"
     *
     * Uses cached ONNX Runtime sessions with CoreML MLProgram EP on macOS (#ifdef __APPLE__) and
     * graceful fallback to CPUExecutionProvider.
     */
    static bool predict(const std::vector<double>& obs_cols,
                        std::size_t nx, std::size_t ny,
                        std::size_t target_nz,
                        double x0, double y0, double z0,
                        double dx, double dy, double dz,
                        VoxelGrid<double>& out_grid,
                        std::string& err_msg,
                        const std::string& custom_model_path = "",
                        const std::vector<double>& surface_depth_cols = {},
                        VoxelGrid<double>* out_sigma_grid = nullptr,
                        double obs_height_m = 0.35,
                        const std::vector<char>& obs_valid_cols = {})
    {
#if !defined(MOD3D_HAS_ONNX) || !MOD3D_HAS_ONNX
        (void)obs_cols; (void)nx; (void)ny; (void)target_nz;
        (void)x0; (void)y0; (void)z0; (void)dx; (void)dy; (void)dz;
        (void)out_grid; (void)custom_model_path; (void)surface_depth_cols;
        (void)out_sigma_grid; (void)obs_height_m; (void)obs_valid_cols;
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
        const bool has_valid = obs_valid_cols.size() == nx * ny;

        try {
            struct SessionEntry {
                std::uintmax_t file_size{0};
                long long mtime_ticks{0};
                std::shared_ptr<Ort::Session> session;
            };
            struct SessionRegistry {
                Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "Mod3D_NeuralPrior"};
                std::mutex mu;
                std::unordered_map<std::string, SessionEntry> cache;
            };
            static SessionRegistry registry;

            std::error_code ec;
            const std::string canon_path = std::filesystem::weakly_canonical(model_path, ec).string();
            const std::string cache_key_path = canon_path.empty() ? model_path : canon_path;
            const std::uintmax_t fsize = std::filesystem::file_size(model_path, ec);
            const auto ftime = std::filesystem::last_write_time(model_path, ec);
            const long long mticks = ec ? 0LL : static_cast<long long>(ftime.time_since_epoch().count());

            std::shared_ptr<Ort::Session> session;
            {
                std::lock_guard<std::mutex> lk(registry.mu);
                auto it = registry.cache.find(cache_key_path);
                if (it != registry.cache.end() && it->second.file_size == fsize && it->second.mtime_ticks == mticks) {
                    session = it->second.session;
                }
            }
            if (!session) {
                auto create_cpu_session = [&]() -> std::shared_ptr<Ort::Session> {
                    Ort::SessionOptions opts;
                    opts.SetIntraOpNumThreads(2);
                    opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
                    return std::make_shared<Ort::Session>(registry.env, model_path.c_str(), opts);
                };
#ifdef __APPLE__
                try {
                    Ort::SessionOptions opts;
                    opts.SetIntraOpNumThreads(2);
                    opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
                    std::unordered_map<std::string, std::string> coreml_opts;
                    coreml_opts["ModelFormat"] = "MLProgram";
                    coreml_opts["MLComputeUnits"] = "ALL";
                    coreml_opts["RequireStaticInputShapes"] = "1";
                    auto cache_root = std::filesystem::temp_directory_path(ec) / "mod3d_coreml_cache";
                    if (!ec) {
                        std::string fname = std::filesystem::path(model_path).filename().string();
                        auto sub = cache_root / (fname + "_" + std::to_string(fsize) + "_" + std::to_string(mticks));
                        std::filesystem::create_directories(sub, ec);
                        if (!ec) coreml_opts["ModelCacheDirectory"] = sub.string();
                    }
                    opts.AppendExecutionProvider("CoreML", coreml_opts);
                    session = std::make_shared<Ort::Session>(registry.env, model_path.c_str(), opts);
                } catch (const std::exception& e) {
                    std::cerr << "[NeuralPrior] CoreML MLProgram EP unavailable/fallback to CPU (" << e.what() << ")\n";
                    session = create_cpu_session();
                }
#else
                session = create_cpu_session();
#endif
                std::lock_guard<std::mutex> lk(registry.mu);
                registry.cache[cache_key_path] = SessionEntry{fsize, mticks, session};
            }

            // Network footprint from the model itself (static shapes): (1, C, Hn, Wn)
            const auto in_shape = session->GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
            const auto out_shape = session->GetOutputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
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
            const double dxy_canon = std::sqrt((nx * dx / static_cast<double>(kNx)) * (ny * dy / static_cast<double>(kNy)));
            const double dz_canon = depth_total / static_cast<double>(kNz);
            double scale = 1.0;
            if (kC >= 5) {
                scale = kTrainCell / std::max(dxy_canon, 1e-6);
            } else if (kC >= 2) {
                const double eff = (nx * dx / kNx) * (ny * dy / kNy) * (depth_total / kNz);
                scale = kTrainCell / std::cbrt(eff);
            }

            const std::size_t plane_n = kNx * kNy;
            std::vector<float> in_buf(kC * plane_n, 0.0f);
            resample2d(obs_cols, in_buf.data());
            if (scale != 1.0) {
                const float fscale = static_cast<float>(scale);
                for (std::size_t i = 0; i < plane_n; ++i) {
                    in_buf[i] *= fscale;
                }
            }
            if (kC >= 2 && has_terrain) {
                std::vector<double> t_norm(nx * ny);
                for (std::size_t i = 0; i < nx * ny; ++i) {
                    t_norm[i] = std::clamp(surface_depth_cols[i] / depth_total, 0.0, kMaxReliefFrac);
                }
                resample2d(t_norm, in_buf.data() + plane_n);
            }
            if (kC >= 5) {
                if (has_valid) {
                    std::vector<double> vm_d(nx * ny);
                    for (std::size_t i = 0; i < nx * ny; ++i) vm_d[i] = obs_valid_cols[i] ? 1.0 : 0.0;
                    resample2d(vm_d, in_buf.data() + 2 * plane_n);
                    for (std::size_t i = 0; i < plane_n; ++i) {
                        in_buf[2 * plane_n + i] = std::clamp(in_buf[2 * plane_n + i], 0.0f, 1.0f);
                    }
                } else {
                    std::fill_n(in_buf.data() + 2 * plane_n, plane_n, 1.0f);
                }
                const float log_asp = static_cast<float>(
                    std::log(std::clamp(dz_canon / std::max(dxy_canon, 1e-6), 0.1, 10.0)));
                const float clr_norm = static_cast<float>(
                    std::clamp(std::max(0.0, obs_height_m) / std::max(dxy_canon, 1e-6), 0.0, 4.0));
                std::fill_n(in_buf.data() + 3 * plane_n, plane_n, log_asp);
                std::fill_n(in_buf.data() + 4 * plane_n, plane_n, clr_norm);
            }

            Ort::MemoryInfo mem_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
            std::vector<int64_t> in_dims = {1, static_cast<int64_t>(kC), static_cast<int64_t>(kNx),
                                            static_cast<int64_t>(kNy)};
            Ort::Value in_tensor = Ort::Value::CreateTensor<float>(
                mem_info, in_buf.data(), in_buf.size(), in_dims.data(), in_dims.size());

            bool has_log_var = false;
            if (out_sigma_grid != nullptr) {
                Ort::AllocatorWithDefaultOptions allocator;
                const std::size_t n_out = session->GetOutputCount();
                for (std::size_t oi = 0; oi < n_out; ++oi) {
                    auto name_ptr = session->GetOutputNameAllocated(oi, allocator);
                    if (name_ptr && std::string(name_ptr.get()) == "log_var_volume") {
                        has_log_var = true;
                        break;
                    }
                }
            }

            const char* in_names[] = {"observation_field"};
            const char* out_names_2[] = {"density_volume", "log_var_volume"};
            const char* out_names_1[] = {"density_volume"};
            auto out_tensors = session->Run(
                Ort::RunOptions{nullptr}, in_names, &in_tensor, 1,
                has_log_var ? out_names_2 : out_names_1, has_log_var ? 2 : 1);
            if (out_tensors.empty()) {
                err_msg = "ONNX session returned no output tensor.";
                return false;
            }
            const float* out_data = out_tensors[0].GetTensorData<float>();
            const float* lv_data = (has_log_var && out_tensors.size() > 1)
                ? out_tensors[1].GetTensorData<float>()
                : nullptr;

            out_grid = VoxelGrid<double>(nx, ny, target_nz, x0, y0, z0, dx, dy, dz, 0.0);
            if (out_sigma_grid) {
                if (lv_data) {
                    *out_sigma_grid = VoxelGrid<double>(nx, ny, target_nz, x0, y0, z0, dx, dy, dz, 0.0);
                } else {
                    *out_sigma_grid = VoxelGrid<double>{};
                }
            }

            auto sample = [&](const float* buf, std::size_t z, std::size_t x, std::size_t y) -> double {
                return static_cast<double>(buf[(z * kNx + x) * kNy + y]);
            };
            auto sample_var = [&](std::size_t z, std::size_t x, std::size_t y) -> double {
                const double lv = std::clamp(static_cast<double>(lv_data[(z * kNx + x) * kNy + y]), -8.0, 4.0);
                return std::exp(lv);
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
                        if ((static_cast<double>(iz) + 0.5) * dz < surf) continue;
                        const double sz = std::clamp((static_cast<double>(iz) + 0.5) * kNz / target_nz - 0.5,
                                                     0.0, static_cast<double>(kNz - 1));
                        const std::size_t z0i = std::min(static_cast<std::size_t>(sz), kNz - 1);
                        const std::size_t z1i = std::min(z0i + 1, kNz - 1);
                        const double fz = sz - z0i;
                        const double c00 = sample(out_data, z0i, x0i, y0i) * (1 - fx) + sample(out_data, z0i, x1i, y0i) * fx;
                        const double c10 = sample(out_data, z0i, x0i, y1i) * (1 - fx) + sample(out_data, z0i, x1i, y1i) * fx;
                        const double c01 = sample(out_data, z1i, x0i, y0i) * (1 - fx) + sample(out_data, z1i, x1i, y0i) * fx;
                        const double c11 = sample(out_data, z1i, x0i, y1i) * (1 - fx) + sample(out_data, z1i, x1i, y1i) * fx;
                        const double c0 = c00 * (1 - fy) + c10 * fy;
                        const double c1 = c01 * (1 - fy) + c11 * fy;
                        out_grid(ix, iy, iz) = c0 * (1 - fz) + c1 * fz;

                        if (out_sigma_grid && lv_data) {
                            const double v00 = sample_var(z0i, x0i, y0i) * (1 - fx) + sample_var(z0i, x1i, y0i) * fx;
                            const double v10 = sample_var(z0i, x0i, y1i) * (1 - fx) + sample_var(z0i, x1i, y1i) * fx;
                            const double v01 = sample_var(z1i, x0i, y0i) * (1 - fx) + sample_var(z1i, x1i, y0i) * fx;
                            const double v11 = sample_var(z1i, x0i, y1i) * (1 - fx) + sample_var(z1i, x1i, y1i) * fx;
                            const double v0 = v00 * (1 - fy) + v10 * fy;
                            const double v1 = v01 * (1 - fy) + v11 * fy;
                            const double var_i = v0 * (1 - fz) + v1 * fz;
                            (*out_sigma_grid)(ix, iy, iz) = std::sqrt(std::max(1e-8, var_i));
                        }
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
