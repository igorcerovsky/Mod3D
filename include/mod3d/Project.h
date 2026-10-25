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
    std::string version{"2.4.5"};
};

struct ProjectSettings {
    bool compute_real_time{false};
    bool remove_observed_mean{false};
    int gravity_formula{0};
    int magnetic_formula{0};
    std::vector<std::string> active_components;
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

    // Clear project state
    void clear();

private:
    ProjectMetadata metadata_;
    ProjectSettings settings_;
    Model model_;
    ObservationSpace observation_;
};

} // namespace mod3d
