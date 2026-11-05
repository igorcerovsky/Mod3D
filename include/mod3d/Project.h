#pragma once

#include "mod3d/Model.h"
#include "mod3d/Observation.h"
#include <string>
#include <string_view>
#include <memory>
#include <vector>
#include <filesystem>

namespace mod3d {

struct ProjectMetadata {
    std::string title{"Untitled Project"};
    std::string author{""};
    std::string description{""};
    std::string created_at{""};
    std::string modified_at{""};
    std::string version{"2.5.5"};
};

struct InversionSettings3D {
    int nz{24};
    double dz{100.0};
    double obs_height{0.1};
    bool focusing_enabled{true};
    int coarse_to_fine_levels{3};
    bool adaptive_refinement{true};
    double ms_weight{1.0};
    double mgs_weight{1.0};
    double depth_weight_exponent{0.75};
    double min_density{-500.0}; // kg/m^3 (matching BodyPropertiesDialog)
    double max_density{600.0};  // kg/m^3 (matching BodyPropertiesDialog)
    int forward_operator{0};
    double noise_rel{2.0};
    int irls_iterations{10};
    int cg_max_iterations{80};
    bool use_neural_prior{false};
    std::string onnx_model_path{""};
    bool prior_as_reference{true};
    bool refine_previous{true};
    int background_trend_mode{1}; // 0: None, 1: ConstantShift, 2: PlanarRamp, 3: BorderPlanar
    bool auto_tune_params{false};
};

struct ProjectSettings {
    bool compute_real_time{false};
    bool remove_observed_mean{false};
    int background_trend_mode{0};
    int gravity_formula{0};
    int magnetic_formula{0};
    std::vector<std::string> active_components;
    InversionSettings3D inversion{};
};

/**
 * @brief High-level Mod3D Project container managing model, observations, and serialization.
 * 
 * Provides:
 * - Modern schema-validated JSON serialization (.mod3d)
 * - Legacy Windows MFC .m3d binary archive deserialization
 * - 3D polyhedral mesh exports (Wavefront OBJ, STL ASCII/Binary, ParaView VTK PolyData)
 */
class Project {
public:
    Project();
    ~Project() = default;

    // Non-copyable, movable
    Project(const Project &) = delete;
    Project &operator=(const Project &) = delete;
    Project(Project &&) noexcept = default;
    Project &operator=(Project &&) noexcept = default;

    // Component Access
    [[nodiscard]] Model &model() noexcept { return model_; }
    [[nodiscard]] const Model &model() const noexcept { return model_; }

    [[nodiscard]] ObservationSpace &observation() noexcept { return observation_; }
    [[nodiscard]] const ObservationSpace &observation() const noexcept { return observation_; }

    [[nodiscard]] ProjectMetadata &metadata() noexcept { return metadata_; }
    [[nodiscard]] const ProjectMetadata &metadata() const noexcept { return metadata_; }

    [[nodiscard]] ProjectSettings &settings() noexcept { return settings_; }
    [[nodiscard]] const ProjectSettings &settings() const noexcept { return settings_; }

    // Modern C++20 JSON Serialization (.mod3d)
    bool save_json(const std::filesystem::path &filePath, int indent = 2) const;
    bool load_json(const std::filesystem::path &filePath);

    [[nodiscard]] std::string to_json_string(int indent = 2) const;
    bool from_json_string(std::string_view jsonStr);

    // 3D Polyhedral Mesh & Attribute Exporters
    bool export_obj(const std::filesystem::path &filePath, int bodyId = -1) const;
    bool export_stl(const std::filesystem::path &filePath, int bodyId = -1, bool binary = true) const;
    bool export_vtk(const std::filesystem::path &filePath, int bodyId = -1) const;

    // Legacy Windows MFC .m3d Binary Archive Deserializer
    bool load_legacy_m3d(const std::filesystem::path &filePath);

    // 3D Inversion Result Serialization Cache
    [[nodiscard]] const std::string &inversion_result_json() const noexcept { return inversion_result_json_; }
    void set_inversion_result_json(std::string json) { inversion_result_json_ = std::move(json); }
    [[nodiscard]] bool has_inversion_result_json() const noexcept { return !inversion_result_json_.empty(); }

    // Clear project state
    void clear();

private:
    ProjectMetadata metadata_;
    ProjectSettings settings_;
    Model model_;
    ObservationSpace observation_;
    std::string inversion_result_json_;
};

} // namespace mod3d
