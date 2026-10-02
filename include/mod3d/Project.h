#pragma once

#include "mod3d/Model.h"
#include "mod3d/Observation.h"
#include <string>
#include <memory>
#include <vector>

namespace mod3d {

struct ProjectMetadata {
    std::string title{"Untitled Project"};
    std::string author{""};
    std::string description{""};
    std::string createdAt{""};
    std::string modifiedAt{""};
    std::string version{"2.1.0"};
};

struct ProjectSettings {
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
    Model &model() noexcept { return m_model; }
    const Model &model() const noexcept { return m_model; }

    ObservationSpace &observation() noexcept { return m_observation; }
    const ObservationSpace &observation() const noexcept { return m_observation; }

    ProjectMetadata &metadata() noexcept { return m_metadata; }
    const ProjectMetadata &metadata() const noexcept { return m_metadata; }

    ProjectSettings &settings() noexcept { return m_settings; }
    const ProjectSettings &settings() const noexcept { return m_settings; }

    // Modern JSON Serialization (.mod3d)
    bool saveJson(const std::string &filePath, int indent = 2) const;
    bool loadJson(const std::string &filePath);

    std::string toJsonString(int indent = 2) const;
    bool fromJsonString(const std::string &jsonStr);

    // 3D Polyhedral Mesh & Attribute Exporters
    bool exportObj(const std::string &filePath, int bodyId = -1) const;
    bool exportStl(const std::string &filePath, int bodyId = -1, bool binary = true) const;
    bool exportVtk(const std::string &filePath, int bodyId = -1) const;

    // Legacy Windows MFC .m3d Binary Archive Deserializer
    bool loadLegacyM3D(const std::string &filePath);

    // Clear project state
    void clear();

private:
    ProjectMetadata m_metadata;
    ProjectSettings m_settings;
    Model m_model;
    ObservationSpace m_observation;
};

} // namespace mod3d
