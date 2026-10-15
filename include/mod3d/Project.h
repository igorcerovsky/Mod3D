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
    std::string version{"2.1.1"};

    // Legacy member access
    std::string createdAt{""};
    std::string modifiedAt{""};
};

struct ProjectSettings {
    bool compute_real_time{false};
    int gravity_formula{0};
    int magnetic_formula{0};
    std::vector<std::string> active_components;

    // Legacy member access
    bool computeRealTime{false};
    int gravityFormula{0};
    int magneticFormula{0};
    std::vector<std::string> activeComponents;
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
    [[nodiscard]] Model &model() noexcept { return m_model; }
    [[nodiscard]] const Model &model() const noexcept { return m_model; }

    [[nodiscard]] ObservationSpace &observation() noexcept { return m_observation; }
    [[nodiscard]] const ObservationSpace &observation() const noexcept { return m_observation; }

    [[nodiscard]] ProjectMetadata &metadata() noexcept { return m_metadata; }
    [[nodiscard]] const ProjectMetadata &metadata() const noexcept { return m_metadata; }

    [[nodiscard]] ProjectSettings &settings() noexcept { return m_settings; }
    [[nodiscard]] const ProjectSettings &settings() const noexcept { return m_settings; }

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

    // ========================================================================
    // Legacy API Wrappers
    // ========================================================================
    bool saveJson(const std::filesystem::path &filePath, int indent = 2) const {
        return save_json(filePath, indent);
    }
    bool loadJson(const std::filesystem::path &filePath) {
        return load_json(filePath);
    }
    [[nodiscard]] std::string toJsonString(int indent = 2) const {
        return to_json_string(indent);
    }
    bool fromJsonString(const std::string &jsonStr) {
        return from_json_string(jsonStr);
    }
    bool exportObj(const std::filesystem::path &filePath, int bodyId = -1) const {
        return export_obj(filePath, bodyId);
    }
    bool exportStl(const std::filesystem::path &filePath, int bodyId = -1, bool binary = true) const {
        return export_stl(filePath, bodyId, binary);
    }
    bool exportVtk(const std::filesystem::path &filePath, int bodyId = -1) const {
        return export_vtk(filePath, bodyId);
    }
    bool loadLegacyM3D(const std::filesystem::path &filePath) {
        return load_legacy_m3d(filePath);
    }

private:
    ProjectMetadata m_metadata;
    ProjectSettings m_settings;
    Model m_model;
    ObservationSpace m_observation;
};

} // namespace mod3d
