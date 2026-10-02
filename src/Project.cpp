#include "mod3d/Project.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <iostream>

namespace mod3d {

namespace {

std::string fieldComponentToString(FieldComponent comp) {
    switch (comp) {
        case FieldComponent::GX: return "gx";
        case FieldComponent::GY: return "gy";
        case FieldComponent::GZ: return "gz";
        case FieldComponent::G_TOT: return "g_tot";
        case FieldComponent::MX: return "mx";
        case FieldComponent::MY: return "my";
        case FieldComponent::MZ: return "mz";
        case FieldComponent::DELTA_T: return "delta_t";
        case FieldComponent::GXX: return "gxx";
        case FieldComponent::GYY: return "gyy";
        case FieldComponent::GZZ: return "gzz";
        case FieldComponent::GXY: return "gxy";
        case FieldComponent::GXZ: return "gxz";
        case FieldComponent::GYZ: return "gyz";
    }
    return "unknown";
}

FieldComponent stringToFieldComponent(const std::string &name) {
    if (name == "gx") return FieldComponent::GX;
    if (name == "gy") return FieldComponent::GY;
    if (name == "gz") return FieldComponent::GZ;
    if (name == "g_tot") return FieldComponent::G_TOT;
    if (name == "mx") return FieldComponent::MX;
    if (name == "my") return FieldComponent::MY;
    if (name == "mz") return FieldComponent::MZ;
    if (name == "delta_t") return FieldComponent::DELTA_T;
    if (name == "gxx") return FieldComponent::GXX;
    if (name == "gyy") return FieldComponent::GYY;
    if (name == "gzz") return FieldComponent::GZZ;
    if (name == "gxy") return FieldComponent::GXY;
    if (name == "gxz") return FieldComponent::GXZ;
    if (name == "gyz") return FieldComponent::GYZ;
    return FieldComponent::GZ;
}

std::string observationModeToString(ObservationMode mode) {
    switch (mode) {
        case ObservationMode::SensorHeight: return "SensorHeight";
        case ObservationMode::FlightElevation: return "FlightElevation";
        case ObservationMode::ElevationGrid: return "ElevationGrid";
    }
    return "SensorHeight";
}

ObservationMode stringToObservationMode(const std::string &str) {
    if (str == "FlightElevation") return ObservationMode::FlightElevation;
    if (str == "ElevationGrid") return ObservationMode::ElevationGrid;
    return ObservationMode::SensorHeight;
}

nlohmann::json gridToJson(const Grid &grid) {
    if (grid.empty()) {
        return nlohmann::json{{"empty", true}};
    }
    nlohmann::json j;
    j["empty"] = false;
    j["rows"] = grid.rows();
    j["cols"] = grid.cols();
    j["x0"] = grid.x0();
    j["y0"] = grid.y0();
    j["x_size"] = grid.xSize();
    j["y_size"] = grid.ySize();
    j["rotation_deg"] = grid.rotation();
    j["values"] = grid.data();
    return j;
}

Grid gridFromJson(const nlohmann::json &j) {
    if (!j.is_object() || j.value("empty", false)) {
        return Grid();
    }
    size_t rows = j.value("rows", 0);
    size_t cols = j.value("cols", 0);
    double x0 = j.value("x0", 0.0);
    double y0 = j.value("y0", 0.0);
    double xSize = j.value("x_size", 100.0);
    double ySize = j.value("y_size", 100.0);
    double rotDeg = j.value("rotation_deg", 0.0);
    Grid g(rows, cols, x0, y0, xSize, ySize, rotDeg);
    if (j.contains("values") && j["values"].is_array()) {
        const auto &arr = j["values"];
        size_t n = std::min(g.data().size(), arr.size());
        for (size_t i = 0; i < n; ++i) {
            g.data()[i] = arr[i].get<double>();
        }
    }
    return g;
}

nlohmann::json bodyToJson(const Body &b) {
    nlohmann::json j;
    j["id"] = b.GetID();
    j["index"] = b.GetIndex();
    j["name"] = b.GetName();
    j["description"] = b.GetDescription();
    j["active"] = b.IsActive();
    j["visible"] = b.IsVisible();
    j["locked"] = b.IsLocked();
    j["filled"] = b.IsFilled();
    j["density"] = b.GetRawDensity();
    j["density_gradient"] = { b.GetDensityGradient().x, b.GetDensityGradient().y, b.GetDensityGradient().z };
    j["density_origin"] = { b.GetDensityOrigo().x, b.GetDensityOrigo().y, b.GetDensityOrigo().z };
    j["susceptibility"] = b.GetSusceptibility();
    j["remanence"] = { b.GetRemanentMagnetization().x, b.GetRemanentMagnetization().y, b.GetRemanentMagnetization().z };
    j["magnetization_vector"] = { b.GetMagnetizationVector().x, b.GetMagnetizationVector().y, b.GetMagnetizationVector().z };
    j["color"] = { b.GetColor().r, b.GetColor().g, b.GetColor().b, b.GetColor().a };
    j["transparent"] = b.IsTransparent();
    j["alpha"] = b.GetTransparency();
    return j;
}

std::unique_ptr<Body> bodyFromJson(const nlohmann::json &j) {
    int id = j.value("id", 1);
    std::string name = j.value("name", "Body");
    double dens = j.value("density", 2670.0);
    auto b = std::make_unique<Body>(id, name, dens);
    b->SetIndex(j.value("index", 0));
    b->SetDescription(j.value("description", ""));
    b->SetActive(j.value("active", true));
    b->SetVisible(j.value("visible", true));
    b->SetLocked(j.value("locked", false));
    b->SetFilled(j.value("filled", true));

    if (j.contains("density_gradient") && j["density_gradient"].size() >= 3) {
        b->SetDensityGradient(Point3D(j["density_gradient"][0], j["density_gradient"][1], j["density_gradient"][2]));
    }
    if (j.contains("density_origin") && j["density_origin"].size() >= 3) {
        b->SetDensityOrigo(Point3D(j["density_origin"][0], j["density_origin"][1], j["density_origin"][2]));
    }
    b->SetSusceptibility(j.value("susceptibility", 0.0));
    if (j.contains("remanence") && j["remanence"].size() >= 3) {
        b->SetRemanentMagnetization(Point3D(j["remanence"][0], j["remanence"][1], j["remanence"][2]));
    }
    if (j.contains("magnetization_vector") && j["magnetization_vector"].size() >= 3) {
        b->SetMagnetizationVector(Point3D(j["magnetization_vector"][0], j["magnetization_vector"][1], j["magnetization_vector"][2]));
    }
    if (j.contains("color") && j["color"].size() >= 4) {
        BodyColor col;
        col.r = static_cast<uint8_t>(j["color"][0]);
        col.g = static_cast<uint8_t>(j["color"][1]);
        col.b = static_cast<uint8_t>(j["color"][2]);
        col.a = static_cast<uint8_t>(j["color"][3]);
        b->SetColor(col);
    }
    b->SetTransparent(j.value("transparent", false));
    b->SetTransparency(j.value("alpha", 1.0f));
    return b;
}

} // namespace

Project::Project() = default;

void Project::clear()
{
    m_metadata = ProjectMetadata();
    m_settings = ProjectSettings();
    m_model.clear();
    m_observation = ObservationSpace();
}

std::string Project::toJsonString(int indent) const
{
    nlohmann::json root;
    root["format"] = "Mod3D";
    root["version"] = m_metadata.version;

    // Metadata
    root["metadata"] = {
        {"title", m_metadata.title},
        {"author", m_metadata.author},
        {"description", m_metadata.description},
        {"created_at", m_metadata.createdAt},
        {"modified_at", m_metadata.modifiedAt}
    };

    // Settings
    root["settings"] = {
        {"compute_real_time", m_settings.computeRealTime},
        {"gravity_formula", m_settings.gravityFormula},
        {"magnetic_formula", m_settings.magneticFormula},
        {"active_components", m_settings.activeComponents}
    };

    // Model
    nlohmann::json jModel;
    jModel["initialized"] = m_model.isInitialized();
    jModel["dimensions"] = {
        {"rows", m_model.getRows()},
        {"cols", m_model.getCols()},
        {"x0", m_model.getX0()},
        {"y0", m_model.getY0()},
        {"x_size", m_model.getXSize()},
        {"y_size", m_model.getYSize()},
        {"z_min", m_model.getZMin()},
        {"z_max", m_model.getZMax()},
        {"x_min", m_model.getXMin()},
        {"x_max", m_model.getXMax()},
        {"y_min", m_model.getYMin()},
        {"y_max", m_model.getYMax()}
    };

    jModel["extension"] = {
        {"enabled", m_model.isExtend()},
        {"north", m_model.getExN()},
        {"south", m_model.getExS()},
        {"east", m_model.getExE()},
        {"west", m_model.getExW()}
    };

    // Bodies
    nlohmann::json jBodies = nlohmann::json::array();
    for (const auto &b : m_model.getBodies()) {
        if (b) {
            jBodies.push_back(bodyToJson(*b));
        }
    }
    jModel["bodies"] = jBodies;

    // Stratigraphic Columns
    nlohmann::json jColumns = nlohmann::json::array();
    int rows = m_model.getRows();
    int cols = m_model.getCols();
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            size_t count = m_model.getCount(r, c);
            if (count > 0) {
                nlohmann::json colObj;
                colObj["r"] = r;
                colObj["c"] = c;
                nlohmann::json ptArray = nlohmann::json::array();
                for (size_t i = 0; i < count; ++i) {
                    const ColumnPoint *pt = m_model.getAt(r, c, static_cast<int>(i));
                    if (pt) {
                        ptArray.push_back({
                            {"body_id", pt->bodyId()},
                            {"z", pt->z()},
                            {"x", pt->point().x},
                            {"y", pt->point().y}
                        });
                    }
                }
                colObj["points"] = ptArray;
                jColumns.push_back(colObj);
            }
        }
    }
    jModel["columns"] = jColumns;
    root["model"] = jModel;

    // Observation Space
    nlohmann::json jObs;
    jObs["geometry"] = {
        {"rows", m_observation.getRows()},
        {"cols", m_observation.getCols()},
        {"x0", m_observation.getX0()},
        {"y0", m_observation.getY0()},
        {"dx", m_observation.getDx()},
        {"dy", m_observation.getDy()},
        {"rotation_deg", m_observation.getRotDeg()}
    };

    jObs["surface_relief"] = gridToJson(m_observation.getSurfaceRelief());

    jObs["gravity"] = {
        {"mode", observationModeToString(m_observation.getGravityMode())},
        {"height", m_observation.getGravityHeight()},
        {"elevation_grid", (m_observation.getGravityMode() == ObservationMode::ElevationGrid) ? gridToJson(m_observation.getGravityElevGrid()) : nlohmann::json{{"empty", true}}}
    };

    jObs["magnetic"] = {
        {"mode", observationModeToString(m_observation.getMagneticMode())},
        {"height", m_observation.getMagneticHeight()},
        {"elevation_grid", (m_observation.getMagneticMode() == ObservationMode::ElevationGrid) ? gridToJson(m_observation.getMagneticElevGrid()) : nlohmann::json{{"empty", true}}}
    };

    jObs["tensor"] = {
        {"mode", observationModeToString(m_observation.getTensorMode())},
        {"height", m_observation.getTensorHeight()},
        {"elevation_grid", (m_observation.getTensorMode() == ObservationMode::ElevationGrid) ? gridToJson(m_observation.getTensorElevGrid()) : nlohmann::json{{"empty", true}}}
    };

    jObs["ambient_field"] = {
        {"inclination_deg", m_observation.getInclinationDeg()},
        {"declination_deg", m_observation.getDeclinationDeg()},
        {"intensity_nt", m_observation.getMagneticIntensity()}
    };
    jObs["reference_density"] = m_observation.getReferenceDensity();

    // Modeled Grids
    nlohmann::json jModeled = nlohmann::json::object();
    for (const auto &[comp, grd] : m_observation.getModeledGrids()) {
        jModeled[fieldComponentToString(comp)] = gridToJson(grd);
    }
    jObs["modeled_grids"] = jModeled;

    // Observed Grids
    nlohmann::json jObserved = nlohmann::json::object();
    for (const auto &[comp, grd] : m_observation.getObservedGrids()) {
        jObserved[fieldComponentToString(comp)] = gridToJson(grd);
    }
    jObs["observed_grids"] = jObserved;

    root["observation"] = jObs;

    return (indent >= 0) ? root.dump(indent) : root.dump();
}

bool Project::saveJson(const std::string &filePath, int indent) const
{
    std::ofstream ofs(filePath);
    if (!ofs.is_open()) {
        return false;
    }
    ofs << toJsonString(indent);
    return ofs.good();
}

bool Project::fromJsonString(const std::string &jsonStr)
{
    try {
        auto root = nlohmann::json::parse(jsonStr);

        clear();

        // Format check
        if (!root.contains("format") || root["format"] != "Mod3D") {
            return false;
        }

        // Metadata
        if (root.contains("metadata")) {
            const auto &jm = root["metadata"];
            m_metadata.title = jm.value("title", "Untitled Project");
            m_metadata.author = jm.value("author", "");
            m_metadata.description = jm.value("description", "");
            m_metadata.createdAt = jm.value("created_at", "");
            m_metadata.modifiedAt = jm.value("modified_at", "");
            m_metadata.version = root.value("version", "2.1.0");
        }

        // Settings
        if (root.contains("settings")) {
            const auto &js = root["settings"];
            m_settings.computeRealTime = js.value("compute_real_time", false);
            m_settings.gravityFormula = js.value("gravity_formula", 0);
            m_settings.magneticFormula = js.value("magnetic_formula", 0);
            if (js.contains("active_components") && js["active_components"].is_array()) {
                m_settings.activeComponents = js["active_components"].get<std::vector<std::string>>();
            }
        }

        // Model
        if (root.contains("model")) {
            const auto &jm = root["model"];
            if (jm.contains("dimensions")) {
                const auto &jd = jm["dimensions"];
                int rows = jd.value("rows", 0);
                int cols = jd.value("cols", 0);
                double x0 = jd.value("x0", 0.0);
                double y0 = jd.value("y0", 0.0);
                double xSize = jd.value("x_size", 100.0);
                double ySize = jd.value("y_size", 100.0);
                double zMin = jd.value("z_min", -5000.0);
                double zMax = jd.value("z_max", 1000.0);

                m_model.initEmpty(rows, cols, x0, y0, xSize, ySize, zMin, zMax);
            }

            if (jm.contains("extension")) {
                const auto &je = jm["extension"];
                bool en = je.value("enabled", false);
                double n = je.value("north", 0.0);
                double s = je.value("south", 0.0);
                double e = je.value("east", 0.0);
                double w = je.value("west", 0.0);
                m_model.setExtensions(n, s, e, w, en);
            }

            // Bodies
            if (jm.contains("bodies") && jm["bodies"].is_array()) {
                for (const auto &jb : jm["bodies"]) {
                    m_model.addBody(bodyFromJson(jb));
                }
            }

            // Columns
            if (jm.contains("columns") && jm["columns"].is_array()) {
                for (const auto &colObj : jm["columns"]) {
                    int r = colObj.value("r", 0);
                    int c = colObj.value("c", 0);
                    if (colObj.contains("points") && colObj["points"].is_array()) {
                        for (const auto &ptObj : colObj["points"]) {
                            int bId = ptObj.value("body_id", -1);
                            double z = ptObj.value("z", 0.0);
                            double x = ptObj.value("x", m_model.getXe(c));
                            double y = ptObj.value("y", m_model.getYe(r));
                            Body *bodyPtr = (bId > 0) ? m_model.getBody(bId) : nullptr;
                            ColumnPoint cp(Point3D(x, y, z), bId, bodyPtr);
                            m_model.add(r, c, cp);
                        }
                    }
                }
            }

            // Regenerate facets and body indices
            m_model.updateBodyIndex();
            m_model.initFacetList();
            m_model.setComputeRealTime(m_settings.computeRealTime);
        }

        // Observation Space
        if (root.contains("observation")) {
            const auto &jo = root["observation"];
            if (jo.contains("geometry")) {
                const auto &jg = jo["geometry"];
                size_t rows = jg.value("rows", 0);
                size_t cols = jg.value("cols", 0);
                double x0 = jg.value("x0", 0.0);
                double y0 = jg.value("y0", 0.0);
                double dx = jg.value("dx", 100.0);
                double dy = jg.value("dy", 100.0);
                double rot = jg.value("rotation_deg", 0.0);
                m_observation.initGeometry(rows, cols, x0, y0, dx, dy, rot);
            }

            if (jo.contains("surface_relief")) {
                m_observation.setSurfaceRelief(gridFromJson(jo["surface_relief"]));
            }

            if (jo.contains("gravity")) {
                const auto &jg = jo["gravity"];
                auto mode = stringToObservationMode(jg.value("mode", "SensorHeight"));
                double height = jg.value("height", 0.0);
                if (mode == ObservationMode::ElevationGrid && jg.contains("elevation_grid")) {
                    Grid eg = gridFromJson(jg["elevation_grid"]);
                    if (!eg.empty()) m_observation.setGravityObservationGrid(eg);
                } else {
                    m_observation.setGravityObservation(mode, height);
                }
            }

            if (jo.contains("magnetic")) {
                const auto &jm = jo["magnetic"];
                auto mode = stringToObservationMode(jm.value("mode", "SensorHeight"));
                double height = jm.value("height", 0.0);
                if (mode == ObservationMode::ElevationGrid && jm.contains("elevation_grid")) {
                    Grid eg = gridFromJson(jm["elevation_grid"]);
                    if (!eg.empty()) m_observation.setMagneticObservationGrid(eg);
                } else {
                    m_observation.setMagneticObservation(mode, height);
                }
            }

            if (jo.contains("tensor")) {
                const auto &jt = jo["tensor"];
                auto mode = stringToObservationMode(jt.value("mode", "SensorHeight"));
                double height = jt.value("height", 0.0);
                if (mode == ObservationMode::ElevationGrid && jt.contains("elevation_grid")) {
                    Grid eg = gridFromJson(jt["elevation_grid"]);
                    if (!eg.empty()) m_observation.setTensorObservationGrid(eg);
                } else {
                    m_observation.setTensorObservation(mode, height);
                }
            }

            if (jo.contains("ambient_field")) {
                const auto &ja = jo["ambient_field"];
                double inc = ja.value("inclination_deg", 60.0);
                double dec = ja.value("declination_deg", 0.0);
                double intensity = ja.value("intensity_nt", 50000.0);
                m_observation.setAmbientFieldParams(inc, dec, intensity);
            }
            if (jo.contains("reference_density")) {
                m_observation.setReferenceDensity(jo.value("reference_density", 2670.0));
            }

            // Observed Grids
            if (jo.contains("observed_grids") && jo["observed_grids"].is_object()) {
                for (auto &[compStr, gridJsonObj] : jo["observed_grids"].items()) {
                    Grid og = gridFromJson(gridJsonObj);
                    if (!og.empty()) {
                        m_observation.setObservedGrid(stringToFieldComponent(compStr), og);
                    }
                }
            }

            // Modeled Grids
            if (jo.contains("modeled_grids") && jo["modeled_grids"].is_object()) {
                for (auto &[compStr, gridJsonObj] : jo["modeled_grids"].items()) {
                    Grid mg = gridFromJson(gridJsonObj);
                    if (!mg.empty()) {
                        Grid *target = m_observation.getModeledGrid(stringToFieldComponent(compStr));
                        if (target) {
                            *target = mg;
                        }
                    }
                }
            }
        }

        return true;
    }
    catch (const std::exception &ex) {
        std::cerr << "Mod3D JSON Parse Error: " << ex.what() << std::endl;
        return false;
    }
}

bool Project::loadJson(const std::string &filePath)
{
    std::ifstream ifs(filePath);
    if (!ifs.is_open()) {
        return false;
    }
    std::stringstream buffer;
    buffer << ifs.rdbuf();
    return fromJsonString(buffer.str());
}

// =========================================================================
// 3D Polyhedral Mesh Exporters
// =========================================================================

bool Project::exportObj(const std::string &filePath, int bodyId) const
{
    std::ofstream ofs(filePath);
    if (!ofs.is_open()) return false;

    ofs << "# Wavefront OBJ exported by Mod3D\n";
    ofs << "# Project: " << m_metadata.title << "\n";
    ofs << "# Unit: meters\n\n";

    std::vector<Facet3Pt> facets;
    m_model.getFacetsComputation(facets);

    // Group facets by body ID
    std::map<int, std::vector<const Facet3Pt*>> bodyFacetMap;
    for (const auto &fct : facets) {
        if (!fct.pBody) continue;
        int id = fct.pBody->GetID();
        if (bodyId == -1 || id == bodyId) {
            bodyFacetMap[id].push_back(&fct);
        }
    }

    size_t vertexIndexOffset = 1;
    for (const auto &[id, fctList] : bodyFacetMap) {
        const Body *b = m_model.getBody(id);
        std::string bodyName = b ? b->GetName() : ("Body_" + std::to_string(id));
        ofs << "o Body_" << id << "_" << bodyName << "\n";
        ofs << "g Body_" << id << "\n";

        // Write vertices and normals
        for (const auto *fct : fctList) {
            for (int k = 0; k < 3; ++k) {
                ofs << "v " << std::fixed << std::setprecision(3)
                    << fct->pts[k].x << " " << fct->pts[k].y << " " << fct->pts[k].z << "\n";
            }
            ofs << "vn " << std::setprecision(5)
                << fct->v_n.x << " " << fct->v_n.y << " " << fct->v_n.z << "\n";
        }

        // Write faces
        size_t normalIdx = vertexIndexOffset / 3 + 1;
        for (size_t i = 0; i < fctList.size(); ++i) {
            size_t v1 = vertexIndexOffset;
            size_t v2 = vertexIndexOffset + 1;
            size_t v3 = vertexIndexOffset + 2;
            size_t vn = normalIdx + i;
            ofs << "f " << v1 << "//" << vn << " " << v2 << "//" << vn << " " << v3 << "//" << vn << "\n";
            vertexIndexOffset += 3;
        }
        ofs << "\n";
    }

    return ofs.good();
}

bool Project::exportStl(const std::string &filePath, int bodyId, bool binary) const
{
    std::vector<Facet3Pt> facets;
    m_model.getFacetsComputation(facets);

    std::vector<const Facet3Pt*> exportList;
    for (const auto &fct : facets) {
        if (!fct.pBody) continue;
        if (bodyId == -1 || fct.pBody->GetID() == bodyId) {
            exportList.push_back(&fct);
        }
    }

    if (!binary) {
        // ASCII STL
        std::ofstream ofs(filePath);
        if (!ofs.is_open()) return false;

        ofs << "solid Mod3D_Export\n";
        for (const auto *fct : exportList) {
            ofs << "  facet normal " << fct->v_n.x << " " << fct->v_n.y << " " << fct->v_n.z << "\n";
            ofs << "    outer loop\n";
            for (int k = 0; k < 3; ++k) {
                ofs << "      vertex " << fct->pts[k].x << " " << fct->pts[k].y << " " << fct->pts[k].z << "\n";
            }
            ofs << "    endloop\n";
            ofs << "  endfacet\n";
        }
        ofs << "endsolid Mod3D_Export\n";
        return ofs.good();
    }
    else {
        // Binary STL
        std::ofstream ofs(filePath, std::ios::binary);
        if (!ofs.is_open()) return false;

        // 80 bytes header
        char header[80] = {0};
        std::snprintf(header, sizeof(header), "Mod3D Binary STL Export - Triangles: %zu", exportList.size());
        ofs.write(header, 80);

        uint32_t numTriangles = static_cast<uint32_t>(exportList.size());
        ofs.write(reinterpret_cast<const char*>(&numTriangles), 4);

        for (const auto *fct : exportList) {
            float n[3] = { static_cast<float>(fct->v_n.x), static_cast<float>(fct->v_n.y), static_cast<float>(fct->v_n.z) };
            ofs.write(reinterpret_cast<const char*>(n), 12);

            for (int k = 0; k < 3; ++k) {
                float v[3] = { static_cast<float>(fct->pts[k].x), static_cast<float>(fct->pts[k].y), static_cast<float>(fct->pts[k].z) };
                ofs.write(reinterpret_cast<const char*>(v), 12);
            }

            uint16_t attrByteCount = 0;
            ofs.write(reinterpret_cast<const char*>(&attrByteCount), 2);
        }

        return ofs.good();
    }
}

bool Project::exportVtk(const std::string &filePath, int bodyId) const
{
    std::ofstream ofs(filePath);
    if (!ofs.is_open()) return false;

    std::vector<Facet3Pt> facets;
    m_model.getFacetsComputation(facets);

    std::vector<const Facet3Pt*> exportList;
    for (const auto &fct : facets) {
        if (!fct.pBody) continue;
        if (bodyId == -1 || fct.pBody->GetID() == bodyId) {
            exportList.push_back(&fct);
        }
    }

    const size_t numTriangles = exportList.size();
    const size_t numPoints = numTriangles * 3;

    ofs << "# vtk DataFile Version 3.0\n";
    ofs << "Mod3D Polyhedral Model Mesh\n";
    ofs << "ASCII\n";
    ofs << "DATASET POLYDATA\n";

    // Points
    ofs << "POINTS " << numPoints << " float\n";
    for (const auto *fct : exportList) {
        for (int k = 0; k < 3; ++k) {
            ofs << static_cast<float>(fct->pts[k].x) << " "
                << static_cast<float>(fct->pts[k].y) << " "
                << static_cast<float>(fct->pts[k].z) << "\n";
        }
    }

    // Polygons (Triangles)
    ofs << "\nPOLYGONS " << numTriangles << " " << (numTriangles * 4) << "\n";
    for (size_t i = 0; i < numTriangles; ++i) {
        size_t p0 = i * 3;
        ofs << "3 " << p0 << " " << (p0 + 1) << " " << (p0 + 2) << "\n";
    }

    // Cell Data (Attributes per triangle facet)
    ofs << "\nCELL_DATA " << numTriangles << "\n";

    // Density
    ofs << "SCALARS Density float 1\n";
    ofs << "LOOKUP_TABLE default\n";
    for (const auto *fct : exportList) {
        float dens = static_cast<float>(fct->pBody ? fct->pBody->GetRawDensity() : 0.0);
        ofs << dens << "\n";
    }

    // Susceptibility
    ofs << "\nSCALARS Susceptibility float 1\n";
    ofs << "LOOKUP_TABLE default\n";
    for (const auto *fct : exportList) {
        float susc = static_cast<float>(fct->pBody ? fct->pBody->GetSusceptibility() : 0.0);
        ofs << susc << "\n";
    }

    // Body ID
    ofs << "\nSCALARS BodyID int 1\n";
    ofs << "LOOKUP_TABLE default\n";
    for (const auto *fct : exportList) {
        int id = fct->pBody ? fct->pBody->GetID() : 0;
        ofs << id << "\n";
    }

    return ofs.good();
}

// =========================================================================
// Legacy Windows MFC .m3d Binary Archive Deserializer
// =========================================================================

namespace {

class BinaryStreamReader {
public:
    explicit BinaryStreamReader(std::istream &is) : m_is(is) {}

    bool readBytes(void *dest, size_t count) {
        m_is.read(reinterpret_cast<char*>(dest), count);
        return m_is.good();
    }

    template <typename T>
    T read() {
        T val{};
        readBytes(&val, sizeof(T));
        return val;
    }

    // MFC CString reader
    std::string readCString() {
        uint8_t lenByte = read<uint8_t>();
        size_t len = 0;
        if (lenByte < 0xFF) {
            len = lenByte;
        } else {
            uint16_t lenWord = read<uint16_t>();
            if (lenWord < 0xFFFF) {
                len = lenWord;
            } else {
                len = read<uint32_t>();
            }
        }
        std::string str(len, '\0');
        if (len > 0) {
            readBytes(&str[0], len);
        }
        return str;
    }

    // MFC CObArray count
    uint32_t readArrayCount() {
        uint16_t count16 = read<uint16_t>();
        if (count16 < 0xFFFF) {
            return count16;
        }
        return read<uint32_t>();
    }

    struct MfcClassInfo {
        std::string name;
        uint16_t schema{0};
    };

    MfcClassInfo readObjectTag() {
        if (!good() || eof()) return {"", 0};
        uint16_t tag = read<uint16_t>();
        if (tag == 0xFFFF) {
            uint16_t schema = read<uint16_t>();
            uint16_t len = read<uint16_t>();
            std::string className(len, '\0');
            if (len > 0) readBytes(&className[0], len);
            MfcClassInfo info{className, schema};
            m_classes.push_back(info);
            return info;
        }
        if (tag & 0x8000) {
            size_t idx = (tag & 0x7FFF);
            if (idx > 0 && idx <= m_classes.size()) {
                return m_classes[idx - 1];
            }
            if (!m_classes.empty()) {
                return m_classes.back();
            }
            return {"", 0};
        }
        // Not an MFC class tag - seek back 2 bytes
        m_is.seekg(-2, std::ios::cur);
        return {"", 0};
    }

    bool eof() const { return m_is.eof(); }
    bool good() const { return m_is.good(); }

private:
    std::istream &m_is;
    std::vector<MfcClassInfo> m_classes;
};

Grid readMfcGrid(BinaryStreamReader &reader) {
    if (!reader.good() || reader.eof()) return Grid();
    std::string strId = reader.readCString();
    if (strId.find("_gridMod3D") == std::string::npos) {
        return Grid();
    }
    int32_t version = reader.read<int32_t>();
    int32_t type = reader.read<int32_t>();
    uint32_t rows = reader.read<uint32_t>();
    uint32_t cols = reader.read<uint32_t>();
    double x0 = reader.read<double>();
    double y0 = reader.read<double>();
    double xSize = reader.read<double>();
    double ySize = reader.read<double>();
    double dMinX = reader.read<double>();
    double dMaxX = reader.read<double>();
    double dMinY = reader.read<double>();
    double dMaxY = reader.read<double>();
    double dMinZ = reader.read<double>();
    double dMaxZ = reader.read<double>();
    int32_t clrTbl = reader.read<int32_t>();
    int32_t contNum = reader.read<int32_t>();
    double dRMS = reader.read<double>();
    double dRot = reader.read<double>();
    double dDrv = reader.read<double>();
    (void)version; (void)type; (void)dMinX; (void)dMaxX; (void)dMinY; (void)dMaxY;
    (void)dMinZ; (void)dMaxZ; (void)clrTbl; (void)contNum; (void)dRMS; (void)dDrv;

    Grid g(rows, cols, x0, y0, xSize, ySize, dRot);
    for (uint32_t r = 0; r < rows; ++r) {
        for (uint32_t c = 0; c < cols; ++c) {
            g(r, c) = reader.read<double>();
        }
    }

    // Skip m_clrGrad
    reader.read<uint32_t>(); // m_Background
    reader.read<uint32_t>(); // m_StartPeg
    reader.read<uint32_t>(); // m_EndPeg
    reader.read<uint32_t>(); // m_UseBackground
    reader.read<int32_t>();  // m_Quantization
    reader.read<int32_t>();  // m_InterpolationMethod
    int32_t pegCount = reader.read<int32_t>();
    for (int p = 0; p < pegCount; ++p) {
        reader.read<uint32_t>(); // colour
        reader.read<float>();    // position
    }

    // Color range
    reader.read<int32_t>(); // m_bHistClr
    reader.read<int32_t>(); // m_bCustomRange
    reader.read<double>();  // m_dMinHstCst
    reader.read<double>();  // m_dMaxHstCst

    return g;
}

} // namespace

bool Project::loadLegacyM3D(const std::string &filePath)
{
    std::ifstream ifs(filePath, std::ios::binary);
    if (!ifs.is_open()) {
        return false;
    }

    BinaryStreamReader reader(ifs);

    // Read 256-byte header
    char hdr[256] = {0};
    if (!reader.readBytes(hdr, 256)) {
        return false;
    }

    // Verify format header string
    if (std::strstr(hdr, "Mod3D") == nullptr) {
        return false;
    }

    clear();
    m_metadata.title = "Imported Legacy Mod3D Project";
    m_metadata.description = std::string(hdr);

    int32_t version = reader.read<int32_t>();
    int32_t docId = reader.read<int32_t>();
    (void)docId;

    int32_t nRows = reader.read<int32_t>();
    int32_t nCols = reader.read<int32_t>();
    double x0 = reader.read<double>();
    double y0 = reader.read<double>();
    double xSize = reader.read<double>();
    double ySize = reader.read<double>();
    double dMinX = reader.read<double>();
    double dMaxX = reader.read<double>();
    double dMinY = reader.read<double>();
    double dMaxY = reader.read<double>();
    double dMinZ = reader.read<double>();
    double dMaxZ = reader.read<double>();

    (void)dMinX; (void)dMaxX; (void)dMinY; (void)dMaxY; (void)dMinZ; (void)dMaxZ;

    // Computation
    m_settings.computeRealTime = (reader.read<int32_t>() != 0);
    int32_t sherComp = reader.read<int32_t>();
    (void)sherComp;

    // Gravity
    m_settings.gravityFormula = reader.read<int32_t>();
    double grvDensRef = reader.read<double>();
    double grvUnits = reader.read<double>();
    (void)grvUnits;
    Point3D grvDensGrad;
    grvDensGrad.x = reader.read<double>();
    grvDensGrad.y = reader.read<double>();
    grvDensGrad.z = reader.read<double>();
    Point3D grvDensOrigo;
    grvDensOrigo.x = reader.read<double>();
    grvDensOrigo.y = reader.read<double>();
    grvDensOrigo.z = reader.read<double>();

    int32_t tensTag = reader.read<int32_t>();
    int32_t tensCompute = reader.read<int32_t>();
    double tensFlightElev = reader.read<double>();
    double tensHeight = reader.read<double>();
    double tensUnits = reader.read<double>();
    (void)tensTag; (void)tensCompute; (void)tensUnits;

    double grvSens = reader.read<double>();
    double grvElev = reader.read<double>();
    int32_t grvObsTag = reader.read<int32_t>();
    (void)grvSens; (void)grvElev; (void)grvObsTag;

    if (version >= 20040419) {
        int32_t remMeanGrv = reader.read<int32_t>();
        int32_t remMeanTns = reader.read<int32_t>();
        (void)remMeanGrv; (void)remMeanTns;
    }

    // Magnetic
    m_settings.magneticFormula = reader.read<int32_t>();
    double magSens = reader.read<double>();
    double magElev = reader.read<double>();
    Point3D magIndFld;
    magIndFld.x = reader.read<double>();
    magIndFld.y = reader.read<double>();
    magIndFld.z = reader.read<double>();
    int32_t magObsTag = reader.read<int32_t>();
    (void)magSens; (void)magElev; (void)magObsTag;

    if (version >= 20040419) {
        int32_t remMeanMag = reader.read<int32_t>();
        (void)remMeanMag;
    }

    // Fitting parameters (skip)
    int32_t fitFld = reader.read<int32_t>();
    int32_t fitVrtxChar = reader.read<int32_t>();
    int32_t fitVrtxMaxIter = reader.read<int32_t>();
    int32_t fitVrtxMeth = reader.read<int32_t>();
    double fitVrtxEps = reader.read<double>();
    double fitVrtxEpsAuto = reader.read<double>();
    double fitVrtxTol = reader.read<double>();
    int32_t bFitVrtxEpsAuto = reader.read<int32_t>();
    int32_t bFitVrtxLog = reader.read<int32_t>();
    int32_t fitDensChar = reader.read<int32_t>();
    int32_t fitDensMaxIter = reader.read<int32_t>();
    int32_t fitDensMeth = reader.read<int32_t>();
    double fitDensEps = reader.read<double>();
    double fitDensTol = reader.read<double>();
    int32_t bFitDensLog = reader.read<int32_t>();
    (void)fitFld; (void)fitVrtxChar; (void)fitVrtxMaxIter; (void)fitVrtxMeth;
    (void)fitVrtxEps; (void)fitVrtxEpsAuto; (void)fitVrtxTol; (void)bFitVrtxEpsAuto; (void)bFitVrtxLog;
    (void)fitDensChar; (void)fitDensMaxIter; (void)fitDensMeth; (void)fitDensEps; (void)fitDensTol; (void)bFitDensLog;

    // CModel::Serialize
    int32_t modelId = reader.read<int32_t>();
    std::string modelInfo = reader.readCString();
    int32_t modelBodyId = reader.read<int32_t>();
    (void)modelId; (void)modelInfo; (void)modelBodyId;

    int32_t modRows = reader.read<int32_t>();
    int32_t modCols = reader.read<int32_t>();
    double modX0 = reader.read<double>();
    double modXSize = reader.read<double>();
    double modY0 = reader.read<double>();
    double modYSize = reader.read<double>();
    double modXMin = reader.read<double>();
    double modXMax = reader.read<double>();
    double modYMin = reader.read<double>();
    double modYMax = reader.read<double>();
    double modZMin = reader.read<double>();
    double modZMax = reader.read<double>();
    (void)modXMin; (void)modXMax; (void)modYMin; (void)modYMax;

    int32_t bExtend = reader.read<int32_t>();
    double dExE = reader.read<double>();
    double dExW = reader.read<double>();
    double dExN = reader.read<double>();
    double dExS = reader.read<double>();

    m_model.initEmpty(modRows, modCols, modX0, modY0, modXSize, modYSize, modZMin, modZMax);
    m_model.setExtensions(dExN, dExS, dExE, dExW, (bExtend != 0));

    // Read column points for all modRows * modCols cells
    int totalCells = modRows * modCols;
    for (int i = 0; i < totalCells; ++i) {
        int r = i / modCols;
        int c = i % modCols;
        uint32_t ptCount = reader.readArrayCount();
        for (uint32_t k = 0; k < ptCount; ++k) {
            reader.readObjectTag();
            int32_t bodyId = reader.read<int32_t>();
            double zVal = reader.read<double>();
            (void)zVal;
            Point3D pt;
            pt.x = reader.read<double>();
            pt.y = reader.read<double>();
            pt.z = reader.read<double>();
            ColumnPoint cp(pt, bodyId);
            m_model.add(r, c, cp);
        }
    }

    // Read Bodies array
    uint32_t bodyCount = reader.readArrayCount();
    for (uint32_t b = 0; b < bodyCount; ++b) {
        auto classInfo = reader.readObjectTag();
        int32_t bId = reader.read<int32_t>();
        std::string bName = reader.readCString();
        std::string bDesc = reader.readCString();
        int32_t bActive = reader.read<int32_t>();
        int32_t bLocked = reader.read<int32_t>();
        int32_t bShow = reader.read<int32_t>();
        double dens = reader.read<double>();
        Point3D densGrad, densOrg;
        densGrad.x = reader.read<double>();
        densGrad.y = reader.read<double>();
        densGrad.z = reader.read<double>();
        densOrg.x = reader.read<double>();
        densOrg.y = reader.read<double>();
        densOrg.z = reader.read<double>();
        double susc = reader.read<double>();
        Point3D rem;
        rem.x = reader.read<double>();
        rem.y = reader.read<double>();
        rem.z = reader.read<double>();

        // Skip 15 pen/brush drawing attributes (COLORREF, styles, hatches, fills)
        uint32_t bCol = 0;
        for (int attr = 0; attr < 15; ++attr) {
            uint32_t val = reader.read<uint32_t>();
            if (attr == 3) {
                bCol = val; // brush color
            }
        }

        if (classInfo.schema >= 3) {
            int32_t bTransparent = reader.read<int32_t>();
            float fAlpha = reader.read<float>();
            (void)bTransparent; (void)fAlpha;
        }

        auto bodyObj = std::make_unique<Body>(bId, bName, dens);
        bodyObj->SetDescription(bDesc);
        bodyObj->SetActive(bActive != 0);
        bodyObj->SetLocked(bLocked != 0);
        bodyObj->SetVisible(bShow != 0);
        bodyObj->SetDensityGradient(densGrad);
        bodyObj->SetDensityOrigo(densOrg);
        bodyObj->SetSusceptibility(susc);
        bodyObj->SetRemanentMagnetization(rem);

        BodyColor col;
        col.r = static_cast<uint8_t>(bCol & 0xFF);
        col.g = static_cast<uint8_t>((bCol >> 8) & 0xFF);
        col.b = static_cast<uint8_t>((bCol >> 16) & 0xFF);
        col.a = 255;
        bodyObj->SetColor(col);

        m_model.addBody(std::move(bodyObj));
    }

    // Set observation parameters
    m_observation.initGeometry(nRows, nCols, x0, y0, xSize, ySize, 0.0);
    m_observation.setReferenceDensity(grvDensRef);
    m_observation.setGravityObservation(ObservationMode::SensorHeight, tensHeight);
    m_observation.setMagneticObservation(ObservationMode::FlightElevation, tensFlightElev);

    // Attempt to read observation grids if present (Relief, Gravity, Magnetic)
    if (reader.good() && !reader.eof()) {
        uint32_t extraObjs = reader.readArrayCount();
        (void)extraObjs;
        Grid relGrid = readMfcGrid(reader);
        if (!relGrid.empty()) {
            m_observation.setSurfaceRelief(relGrid);
        }
        Grid grvGrid = readMfcGrid(reader);
        if (!grvGrid.empty()) {
            m_observation.setObservedGrid(FieldComponent::GZ, grvGrid);
        }
        Grid magGrid = readMfcGrid(reader);
        if (!magGrid.empty()) {
            m_observation.setObservedGrid(FieldComponent::DELTA_T, magGrid);
        }
    }

    // Regenerate 3D polyhedral facets
    m_model.updateBodyIndex();
    m_model.initFacetList();

    return true;
}

} // namespace mod3d
