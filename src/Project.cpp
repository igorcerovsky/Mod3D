#include "mod3d/Project.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <iostream>
#include <string_view>
#include <vector>

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
    j["x0"] = grid.x0().value();
    j["y0"] = grid.y0().value();
    j["x_size"] = grid.dx().value();
    j["y_size"] = grid.dy().value();
    j["rotation_deg"] = grid.rotation().value();
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
    j["id"] = b.id();
    j["name"] = b.name();
    j["description"] = b.description();
    j["active"] = b.is_active();
    j["visible"] = b.is_visible();
    j["locked"] = b.is_locked();
    j["filled"] = b.is_filled();
    j["density"] = b.raw_density().value();
    j["density_gradient"] = { b.density_gradient().x.value(), b.density_gradient().y.value(), b.density_gradient().z.value() };
    j["density_origin"] = { b.density_origin().x.value(), b.density_origin().y.value(), b.density_origin().z.value() };
    j["susceptibility"] = b.susceptibility();
    j["remanence"] = { b.remanent_magnetization().mx.value(), b.remanent_magnetization().my.value(), b.remanent_magnetization().mz.value() };
    j["magnetization_vector"] = { b.magnetization_vector().mx.value(), b.magnetization_vector().my.value(), b.magnetization_vector().mz.value() };
    j["color"] = { b.color().r, b.color().g, b.color().b, b.color().a };
    j["transparent"] = b.is_transparent();
    j["alpha"] = b.transparency();
    return j;
}

std::unique_ptr<Body> bodyFromJson(const nlohmann::json &j) {
    int id = j.value("id", 1);
    std::string name = j.value("name", "Body");
    double dens = j.value("density", 2670.0);
    auto b = std::make_unique<Body>(id, name, dens);
    b->set_description(j.value("description", ""));
    b->set_active(j.value("active", true));
    b->set_visible(j.value("visible", true));
    b->set_locked(j.value("locked", false));
    b->set_filled(j.value("filled", true));

    if (j.contains("density_gradient") && j["density_gradient"].size() >= 3) {
        b->set_density_gradient(Point3D(j["density_gradient"][0], j["density_gradient"][1], j["density_gradient"][2]));
    }
    if (j.contains("density_origin") && j["density_origin"].size() >= 3) {
        b->set_density_origin(Point3D(j["density_origin"][0], j["density_origin"][1], j["density_origin"][2]));
    }
    b->set_susceptibility(j.value("susceptibility", 0.0));
    if (j.contains("remanence") && j["remanence"].size() >= 3) {
        b->set_remanent_magnetization(Point3D(j["remanence"][0], j["remanence"][1], j["remanence"][2]));
    }
    if (j.contains("magnetization_vector") && j["magnetization_vector"].size() >= 3) {
        b->set_magnetization_vector(Point3D(j["magnetization_vector"][0], j["magnetization_vector"][1], j["magnetization_vector"][2]));
    }
    if (j.contains("color") && j["color"].size() >= 4) {
        BodyColor col;
        col.r = static_cast<uint8_t>(j["color"][0]);
        col.g = static_cast<uint8_t>(j["color"][1]);
        col.b = static_cast<uint8_t>(j["color"][2]);
        col.a = static_cast<uint8_t>(j["color"][3]);
        b->set_color(col);
    } else if (j.contains("fill_color") && j["fill_color"].size() >= 4) {
        BodyColor col;
        col.r = static_cast<uint8_t>(j["fill_color"][0]);
        col.g = static_cast<uint8_t>(j["fill_color"][1]);
        col.b = static_cast<uint8_t>(j["fill_color"][2]);
        col.a = static_cast<uint8_t>(j["fill_color"][3]);
        b->set_color(col);
    } else if (j.contains("map_color") && j["map_color"].size() >= 4) {
        BodyColor col;
        col.r = static_cast<uint8_t>(j["map_color"][0]);
        col.g = static_cast<uint8_t>(j["map_color"][1]);
        col.b = static_cast<uint8_t>(j["map_color"][2]);
        col.a = static_cast<uint8_t>(j["map_color"][3]);
        b->set_color(col);
    }
    b->set_transparent(j.value("transparent", false));
    b->set_transparency(j.value("alpha", 1.0f));
    return b;
}

} // namespace

Project::Project() = default;

void Project::clear()
{
    metadata_ = ProjectMetadata();
    settings_ = ProjectSettings();
    model_.clear();
    observation_ = ObservationSpace();
}

std::string Project::to_json_string(int indent) const
{
    nlohmann::json root;
    root["format"] = "Mod3D";
    root["version"] = metadata_.version;

    // Metadata
    root["metadata"] = {
        {"title", metadata_.title},
        {"author", metadata_.author},
        {"description", metadata_.description},
        {"created_at", metadata_.created_at},
        {"modified_at", metadata_.modified_at}
    };

    // Settings
    root["settings"] = {
        {"compute_real_time", settings_.compute_real_time},
        {"gravity_formula", settings_.gravity_formula},
        {"magnetic_formula", settings_.magnetic_formula},
        {"active_components", settings_.active_components}
    };

    // Model
    nlohmann::json jModel;
    jModel["initialized"] = model_.is_initialized();
    jModel["dimensions"] = {
        {"rows", model_.rows()},
        {"cols", model_.cols()},
        {"x0", model_.x0_val()},
        {"y0", model_.y0_val()},
        {"x_size", model_.dx_val()},
        {"y_size", model_.dy_val()},
        {"z_min", model_.z_min_val()},
        {"z_max", model_.z_max_val()},
        {"x_min", model_.x_min_val()},
        {"x_max", model_.x_max_val()},
        {"y_min", model_.y_min_val()},
        {"y_max", model_.y_max_val()}
    };

    jModel["extension"] = {
        {"enabled", model_.is_extend()},
        {"north", model_.ex_n_val()},
        {"south", model_.ex_s_val()},
        {"east", model_.ex_e_val()},
        {"west", model_.ex_w_val()}
    };

    // Bodies
    nlohmann::json jBodies = nlohmann::json::array();
    for (const auto &b : model_.bodies()) {
        if (b) {
            jBodies.push_back(bodyToJson(*b));
        }
    }
    jModel["bodies"] = jBodies;

    // Stratigraphic Columns
    nlohmann::json jColumns = nlohmann::json::array();
    int rows = model_.rows();
    int cols = model_.cols();
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            size_t count = model_.count(r, c);
            if (count > 0) {
                nlohmann::json colObj;
                colObj["r"] = r;
                colObj["c"] = c;
                nlohmann::json ptArray = nlohmann::json::array();
                for (size_t i = 0; i < count; ++i) {
                    const ColumnPoint *pt = model_.at(r, c, static_cast<int>(i));
                    if (pt) {
                        ptArray.push_back({
                            {"body_id", pt->body_id()},
                            {"z", pt->z().value()},
                            {"x", pt->x().value()},
                            {"y", pt->y().value()}
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
        {"rows", observation_.rows()},
        {"cols", observation_.cols()},
        {"x0", observation_.x0().value()},
        {"y0", observation_.y0().value()},
        {"dx", observation_.dx().value()},
        {"dy", observation_.dy().value()},
        {"rotation_deg", observation_.rotation().value()}
    };

    jObs["surface_relief"] = gridToJson(observation_.surface_relief());

    jObs["gravity"] = {
        {"mode", observationModeToString(observation_.gravity_mode())},
        {"height", observation_.gravity_height().value()},
        {"elevation_grid", (observation_.gravity_mode() == ObservationMode::ElevationGrid) ? gridToJson(observation_.gravity_elevation_grid()) : nlohmann::json{{"empty", true}}}
    };

    jObs["magnetic"] = {
        {"mode", observationModeToString(observation_.magnetic_mode())},
        {"height", observation_.magnetic_height().value()},
        {"elevation_grid", (observation_.magnetic_mode() == ObservationMode::ElevationGrid) ? gridToJson(observation_.magnetic_elevation_grid()) : nlohmann::json{{"empty", true}}}
    };

    jObs["tensor"] = {
        {"mode", observationModeToString(observation_.tensor_mode())},
        {"height", observation_.tensor_height().value()},
        {"elevation_grid", (observation_.tensor_mode() == ObservationMode::ElevationGrid) ? gridToJson(observation_.tensor_elevation_grid()) : nlohmann::json{{"empty", true}}}
    };

    jObs["ambient_field"] = {
        {"inclination_deg", observation_.inclination().value()},
        {"declination_deg", observation_.declination().value()},
        {"intensity_nt", observation_.magnetic_intensity().value()}
    };
    jObs["reference_density"] = observation_.reference_density().value();
    jObs["units"] = {
        {"gravity", to_string(observation_.gravity_field_unit())},
        {"gravity_scale", observation_.gravity_scale()},
        {"magnetic", "nT"},
        {"tensor", "E"}
    };

    // Modeled Grids
    nlohmann::json jModeled = nlohmann::json::object();
    for (const auto &[comp, grd] : observation_.modeled_grids()) {
        jModeled[fieldComponentToString(comp)] = gridToJson(grd);
    }
    jObs["modeled_grids"] = jModeled;

    // Observed Grids
    nlohmann::json jObserved = nlohmann::json::object();
    for (const auto &[comp, grd] : observation_.observed_grids()) {
        jObserved[fieldComponentToString(comp)] = gridToJson(grd);
    }
    jObs["observed_grids"] = jObserved;

    root["observation"] = jObs;

    return (indent >= 0) ? root.dump(indent) : root.dump();
}

bool Project::save_json(const std::filesystem::path &filePath, int indent) const
{
    std::ofstream ofs(filePath);
    if (!ofs.is_open()) {
        return false;
    }
    ofs << to_json_string(indent);
    return ofs.good();
}

bool Project::from_json_string(std::string_view jsonStr)
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
            metadata_.title = jm.value("title", "Untitled Project");
            metadata_.author = jm.value("author", "");
            metadata_.description = jm.value("description", "");
            metadata_.created_at = jm.value("created_at", "");
            metadata_.modified_at = jm.value("modified_at", "");
            metadata_.version = root.value("version", "2.4.3");
        }

        // Settings
        if (root.contains("settings")) {
            const auto &js = root["settings"];
            settings_.compute_real_time = js.value("compute_real_time", false);
            settings_.gravity_formula = js.value("gravity_formula", 0);
            settings_.magnetic_formula = js.value("magnetic_formula", 0);
            if (js.contains("active_components") && js["active_components"].is_array()) {
                settings_.active_components = js["active_components"].get<std::vector<std::string>>();
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

                (void)model_.init_empty(rows, cols, x0, y0, xSize, ySize, zMin, zMax);
            }

            if (jm.contains("extension")) {
                const auto &je = jm["extension"];
                bool en = je.value("enabled", false);
                double n = je.value("north", 0.0);
                double s = je.value("south", 0.0);
                double e = je.value("east", 0.0);
                double w = je.value("west", 0.0);
                model_.set_extensions(n, s, e, w, en);
            }

            // Bodies
            if (jm.contains("bodies") && jm["bodies"].is_array()) {
                for (const auto &jb : jm["bodies"]) {
                    model_.add_body(bodyFromJson(jb));
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
                            double x = ptObj.value("x", model_.xe_val(c));
                            double y = ptObj.value("y", model_.ye_val(r));
                            Body *bodyPtr = (bId >= 0) ? model_.body(bId) : nullptr;
                            ColumnPoint cp(Point3D(x, y, z), bId, bodyPtr);
                            model_.add(r, c, cp);
                        }
                    }
                }
            }

            // Regenerate facets
            model_.init_facet_list();
            model_.clear_facets_update();
            model_.set_compute_real_time(settings_.compute_real_time);
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
                observation_.init_geometry(rows, cols, x0, y0, dx, dy, rot);
            }

            if (jo.contains("surface_relief")) {
                observation_.set_surface_relief(gridFromJson(jo["surface_relief"]));
            }

            if (jo.contains("gravity")) {
                const auto &jg = jo["gravity"];
                auto mode = stringToObservationMode(jg.value("mode", "SensorHeight"));
                double height = jg.value("height", 0.0);
                if (mode == ObservationMode::ElevationGrid && jg.contains("elevation_grid")) {
                    Grid eg = gridFromJson(jg["elevation_grid"]);
                    if (!eg.empty()) observation_.set_gravity_observation_grid(eg);
                } else {
                    observation_.set_gravity_observation(mode, height);
                }
            }

            if (jo.contains("magnetic")) {
                const auto &jm = jo["magnetic"];
                auto mode = stringToObservationMode(jm.value("mode", "SensorHeight"));
                double height = jm.value("height", 0.0);
                if (mode == ObservationMode::ElevationGrid && jm.contains("elevation_grid")) {
                    Grid eg = gridFromJson(jm["elevation_grid"]);
                    if (!eg.empty()) observation_.set_magnetic_observation_grid(eg);
                } else {
                    observation_.set_magnetic_observation(mode, height);
                }
            }

            if (jo.contains("tensor")) {
                const auto &jt = jo["tensor"];
                auto mode = stringToObservationMode(jt.value("mode", "SensorHeight"));
                double height = jt.value("height", 0.0);
                if (mode == ObservationMode::ElevationGrid && jt.contains("elevation_grid")) {
                    Grid eg = gridFromJson(jt["elevation_grid"]);
                    if (!eg.empty()) observation_.set_tensor_observation_grid(eg);
                } else {
                    observation_.set_tensor_observation(mode, height);
                }
            }

            if (jo.contains("ambient_field")) {
                const auto &ja = jo["ambient_field"];
                double inc = ja.value("inclination_deg", 60.0);
                double dec = ja.value("declination_deg", 0.0);
                double intensity = ja.value("intensity_nt", 50000.0);
                observation_.set_ambient_field_params(inc, dec, intensity);
            }
            if (jo.contains("reference_density")) {
                observation_.set_reference_density(jo.value("reference_density", 2670.0));
            }
            if (jo.contains("units")) {
                const auto &ju = jo["units"];
                if (ju.contains("gravity")) {
                    if (ju["gravity"].is_string()) {
                        observation_.set_gravity_field_unit(gravity_field_unit_from_string(ju["gravity"].get<std::string>()));
                    } else if (ju["gravity"].is_number()) {
                        observation_.set_gravity_field_unit(gravity_field_unit_from_scale(ju["gravity"].get<double>()));
                    }
                }
            }

            // Observed Grids
            if (jo.contains("observed_grids") && jo["observed_grids"].is_object()) {
                for (auto &[compStr, gridJsonObj] : jo["observed_grids"].items()) {
                    Grid og = gridFromJson(gridJsonObj);
                    if (!og.empty()) {
                        observation_.set_observed_grid(stringToFieldComponent(compStr), og);
                    }
                }
            }

            // Modeled Grids
            if (jo.contains("modeled_grids") && jo["modeled_grids"].is_object()) {
                for (auto &[compStr, gridJsonObj] : jo["modeled_grids"].items()) {
                    Grid mg = gridFromJson(gridJsonObj);
                    if (!mg.empty()) {
                        Grid *target = observation_.modeled_grid(stringToFieldComponent(compStr));
                        if (target) {
                            *target = mg;
                        }
                    }
                }
            }
        }

        model_.compute_body_magnetization_vector(
            observation_.inclination_deg(),
            observation_.declination_deg(),
            observation_.magnetic_intensity());

        return true;
    }
    catch (const std::exception &ex) {
        std::cerr << "Mod3D JSON Parse Error: " << ex.what() << std::endl;
        return false;
    }
}

bool Project::load_json(const std::filesystem::path &filePath)
{
    std::ifstream ifs(filePath);
    if (!ifs.is_open()) {
        return false;
    }
    std::stringstream buffer;
    buffer << ifs.rdbuf();
    return from_json_string(buffer.str());
}

// =========================================================================
// 3D Polyhedral Mesh Exporters
// =========================================================================

bool Project::export_obj(const std::filesystem::path &filePath, int bodyId) const
{
    std::ofstream ofs(filePath);
    if (!ofs.is_open()) return false;

    ofs << "# Wavefront OBJ exported by Mod3D\n";
    ofs << "# Project: " << metadata_.title << "\n";
    ofs << "# Unit: meters\n\n";

    std::vector<Facet3Pt> facets;
    std::map<int, std::vector<const Facet3Pt*>> bodyFacetMap;

    if (bodyId == -1) {
        model_.facets_computation(facets);
        for (const auto &fct : facets) {
            if (!fct.primary) continue;
            int id = fct.primary->id();
            bodyFacetMap[id].push_back(&fct);
        }
    } else {
        model_.body_facets(bodyId, facets, false);
        for (const auto &fct : facets) {
            bodyFacetMap[bodyId].push_back(&fct);
        }
    }

    size_t vertexIndexOffset = 1;
    for (const auto &[id, fctList] : bodyFacetMap) {
        const Body *b = model_.body(id);
        std::string bodyName = b ? b->name() : ("Body_" + std::to_string(id));
        ofs << "o Body_" << id << "_" << bodyName << "\n";
        ofs << "g Body_" << id << "\n";

        // Write vertices and normals
        for (const auto *fct : fctList) {
            for (int k = 0; k < 3; ++k) {
                ofs << "v " << std::fixed << std::setprecision(3)
                    << (*fct)[k].x << " " << (*fct)[k].y << " " << (*fct)[k].z << "\n";
            }
            ofs << "vn " << std::setprecision(5)
                << fct->normal().x << " " << fct->normal().y << " " << fct->normal().z << "\n";
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

bool Project::export_stl(const std::filesystem::path &filePath, int bodyId, bool binary) const
{
    std::vector<Facet3Pt> facets;
    std::vector<const Facet3Pt*> exportList;

    if (bodyId == -1) {
        model_.facets_computation(facets);
        for (const auto &fct : facets) {
            if (fct.primary) {
                exportList.push_back(&fct);
            }
        }
    } else {
        model_.body_facets(bodyId, facets, false);
        for (const auto &fct : facets) {
            exportList.push_back(&fct);
        }
    }

    if (!binary) {
        // ASCII STL
        std::ofstream ofs(filePath);
        if (!ofs.is_open()) return false;

        ofs << "solid Mod3D_Export\n";
        for (const auto *fct : exportList) {
            ofs << "  facet normal " << fct->normal().x << " " << fct->normal().y << " " << fct->normal().z << "\n";
            ofs << "    outer loop\n";
            for (int k = 0; k < 3; ++k) {
                ofs << "      vertex " << (*fct)[k].x << " " << (*fct)[k].y << " " << (*fct)[k].z << "\n";
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
            float n[3] = { static_cast<float>(fct->normal().x), static_cast<float>(fct->normal().y), static_cast<float>(fct->normal().z) };
            ofs.write(reinterpret_cast<const char*>(n), 12);

            for (int k = 0; k < 3; ++k) {
                float v[3] = { static_cast<float>((*fct)[k].x), static_cast<float>((*fct)[k].y), static_cast<float>((*fct)[k].z) };
                ofs.write(reinterpret_cast<const char*>(v), 12);
            }

            uint16_t attrByteCount = 0;
            ofs.write(reinterpret_cast<const char*>(&attrByteCount), 2);
        }

        return ofs.good();
    }
}

bool Project::export_vtk(const std::filesystem::path &filePath, int bodyId) const
{
    std::ofstream ofs(filePath);
    if (!ofs.is_open()) return false;

    std::vector<Facet3Pt> facets;
    std::vector<const Facet3Pt*> exportList;

    if (bodyId == -1) {
        model_.facets_computation(facets);
        for (const auto &fct : facets) {
            if (fct.primary) {
                exportList.push_back(&fct);
            }
        }
    } else {
        model_.body_facets(bodyId, facets, false);
        for (const auto &fct : facets) {
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
            ofs << static_cast<float>((*fct)[k].x) << " "
                << static_cast<float>((*fct)[k].y) << " "
                << static_cast<float>((*fct)[k].z) << "\n";
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
        float dens = static_cast<float>(fct->primary ? fct->primary->raw_density().value() : 0.0);
        ofs << dens << "\n";
    }

    // Susceptibility
    ofs << "\nSCALARS Susceptibility float 1\n";
    ofs << "LOOKUP_TABLE default\n";
    for (const auto *fct : exportList) {
        float susc = static_cast<float>(fct->primary ? fct->primary->susceptibility() : 0.0);
        ofs << susc << "\n";
    }

    // Body ID
    ofs << "\nSCALARS BodyID int 1\n";
    ofs << "LOOKUP_TABLE default\n";
    for (const auto *fct : exportList) {
        int id = fct->primary ? fct->primary->id() : 0;
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

    std::istream &stream() { return m_is; }
    std::streampos tell() { return m_is.tellg(); }
    void seek(std::streampos pos) {
        m_is.clear();
        m_is.seekg(pos);
    }

private:
    std::istream &m_is;
    std::vector<MfcClassInfo> m_classes;
};

bool validateMfcGrid(const uint8_t *buf, size_t bufLen, size_t &pos) {
    if (pos >= bufLen) return false;
    uint8_t lenByte = buf[pos++];
    size_t strLen = lenByte;
    if (lenByte == 0xFF) {
        if (pos + 2 > bufLen) return false;
        uint16_t w = static_cast<uint16_t>(buf[pos] | (buf[pos + 1] << 8));
        pos += 2;
        if (w == 0xFFFF) {
            if (pos + 4 > bufLen) return false;
            strLen = static_cast<size_t>(buf[pos] | (buf[pos + 1] << 8) | (buf[pos + 2] << 16) | (buf[pos + 3] << 24));
            pos += 4;
        } else {
            strLen = w;
        }
    }
    if (pos + strLen > bufLen) return false;
    std::string_view s(reinterpret_cast<const char*>(buf + pos), strLen);
    if (s.find("_gridMod3D") == std::string_view::npos) return false;
    pos += strLen;

    // Header: version(4), type(4), rows(4), cols(4)
    if (pos + 16 > bufLen) return false;
    int32_t version = 0;
    std::memcpy(&version, buf + pos, sizeof(version));
    uint32_t rows = 0;
    std::memcpy(&rows, buf + pos + 8, sizeof(rows));
    uint32_t cols = 0;
    std::memcpy(&cols, buf + pos + 12, sizeof(cols));
    pos += 16;

    // Bounds: 10 doubles = 80 bytes
    // clrTbl(4), contNum(4), dRMS(8), dRot(8), dDrv(8) = 32 bytes
    if (pos + 112 > bufLen) return false;
    pos += 112;

    if (rows > 5000 || cols > 5000) return false;
    size_t valBytes = static_cast<size_t>(rows) * cols * sizeof(double);
    if (pos + valBytes > bufLen) return false;
    pos += valBytes;

    // Gradient: 4 uint32 (16), 2 int32 (8), pegCount (4) = 28 bytes
    if (pos + 28 > bufLen) return false;
    int32_t pegCount = 0;
    std::memcpy(&pegCount, buf + pos + 24, sizeof(pegCount));
    pos += 28;
    if (pegCount < 0 || pegCount > 1000) return false;
    size_t pegBytes = static_cast<size_t>(pegCount) * 8;
    if (pos + pegBytes > bufLen) return false;
    pos += pegBytes;

    if (version >= 20040326) {
        if (pos + 24 > bufLen) return false;
        pos += 24;
    }
    return true;
}

bool canRead64Grids(const uint8_t *buf, size_t bufLen, size_t startPos) {
    size_t pos = startPos;
    // 4 observation grids (relief, grv, mag, tns)
    for (int i = 0; i < 4; ++i) {
        if (!validateMfcGrid(buf, bufLen, pos)) return false;
    }
    // 60 component grids each followed by activeCompute int32_t (4 bytes)
    for (int i = 0; i < 60; ++i) {
        if (!validateMfcGrid(buf, bufLen, pos)) return false;
        if (pos + 4 > bufLen) return false;
        pos += 4;
    }
    return true;
}

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
    (void)type; (void)dMinX; (void)dMaxX; (void)dMinY; (void)dMaxY;
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

    // Color range if version >= 20040326
    if (version >= 20040326) {
        reader.read<int32_t>(); // m_bHistClr
        reader.read<int32_t>(); // m_bCustomRange
        reader.read<double>();  // m_dMinHstCst
        reader.read<double>();  // m_dMaxHstCst
    }

    return g;
}

} // namespace

bool Project::load_legacy_m3d(const std::filesystem::path &filePath)
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
    metadata_.title = "Imported Legacy Mod3D Project";
    metadata_.description = std::string(hdr);

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
    settings_.compute_real_time = (reader.read<int32_t>() != 0);
    int32_t sherComp = reader.read<int32_t>();
    (void)sherComp;

    // Gravity
    settings_.gravity_formula = reader.read<int32_t>();
    double grvDensRef = reader.read<double>();
    double grvUnits = reader.read<double>();
    if (grvUnits > 0.0) {
        observation_.set_gravity_field_unit(gravity_field_unit_from_scale(grvUnits));
    }
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
    settings_.magnetic_formula = reader.read<int32_t>();
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

    (void)model_.init_empty(modRows, modCols, modX0, modY0, modXSize, modYSize, modZMin, modZMax);
    model_.set_extensions(dExN, dExS, dExE, dExW, (bExtend != 0));

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
            model_.add(r, c, cp);
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

        // Read 15 pen/brush drawing attributes (COLORREF, styles, hatches, fills)
        uint32_t pCol = reader.read<uint32_t>();
        uint32_t pStyle = reader.read<uint32_t>();
        int32_t pWidthX = reader.read<int32_t>();
        int32_t pWidthY = reader.read<int32_t>();
        uint32_t bCol = reader.read<uint32_t>();
        uint32_t bHatch = reader.read<uint32_t>();
        uint32_t bStyle = reader.read<uint32_t>();
        uint32_t pMapCol = reader.read<uint32_t>();
        uint32_t pMapStyle = reader.read<uint32_t>();
        int32_t pMapWidthX = reader.read<int32_t>();
        int32_t pMapWidthY = reader.read<int32_t>();
        uint32_t bMapCol = reader.read<uint32_t>();
        uint32_t bMapHatch = reader.read<uint32_t>();
        uint32_t bMapStyle = reader.read<uint32_t>();
        int32_t bFill = reader.read<int32_t>();

        (void)pStyle; (void)pWidthX; (void)pWidthY;
        (void)bHatch; (void)bStyle;
        (void)pMapStyle; (void)pMapWidthX; (void)pMapWidthY;
        (void)bMapCol; (void)bMapHatch; (void)bMapStyle;

        bool isTransparent = false;
        float fAlpha = 0.5f;
        if (classInfo.schema >= 3) {
            int32_t bTransparent = reader.read<int32_t>();
            float alpha = reader.read<float>();
            isTransparent = (bTransparent != 0);
            if (alpha >= 0.0f && alpha <= 1.0f) {
                fAlpha = alpha;
            }
        }

        if (bName.empty()) {
            bName = "Body " + std::to_string(bId);
        }
        if (bDesc == "Body description...") {
            bDesc = "";
        }

        auto bodyObj = std::make_unique<Body>(bId, bName, dens);
        bodyObj->set_description(bDesc);
        bodyObj->set_active(bActive != 0);
        bodyObj->set_locked(bLocked != 0);
        bodyObj->set_visible(bShow != 0);
        bodyObj->set_filled(bFill != 0);
        bodyObj->set_transparent(isTransparent);
        bodyObj->set_transparency(fAlpha);
        bodyObj->set_density_gradient(densGrad);
        bodyObj->set_density_origin(densOrg);
        bodyObj->set_susceptibility(susc);
        bodyObj->set_remanent_magnetization(rem);

        auto rgbFromColorref = [](uint32_t cr) -> BodyColor {
            return BodyColor(
                static_cast<uint8_t>(cr & 0xFF),
                static_cast<uint8_t>((cr >> 8) & 0xFF),
                static_cast<uint8_t>((cr >> 16) & 0xFF),
                255
            );
        };

        // Prefer profile pen color (pCol), then brush fill color (bCol), then map pen color (pMapCol)
        uint32_t chosenCol = pCol;
        if (chosenCol == 0 && bCol != 0) {
            chosenCol = bCol;
        } else if (chosenCol == 0 && pMapCol != 0) {
            chosenCol = pMapCol;
        }

        if (chosenCol != 0) {
            bodyObj->set_color(rgbFromColorref(chosenCol));
        } else {
            static const BodyColor defaultPalette[] = {
                {180, 120, 80, 255},  // Sandstone
                {220, 80, 60, 255},   // Granite
                {60, 140, 180, 255},  // Limestone
                {100, 180, 100, 255}, // Shale
                {180, 100, 180, 255}, // Basalt
                {220, 180, 60, 255},  // Alluvium
                {80, 80, 90, 255}     // Gabbro
            };
            int pIdx = (bId >= 0 ? bId : 0) % 7;
            bodyObj->set_color(defaultPalette[pIdx]);
        }

        model_.add_body(std::move(bodyObj));
    }

    // Set observation parameters
    observation_.init_geometry(nRows, nCols, x0, y0, xSize, ySize, 0.0);
    observation_.set_reference_density(grvDensRef);
    observation_.set_gravity_observation(ObservationMode::SensorHeight, tensHeight);
    observation_.set_magnetic_observation(ObservationMode::FlightElevation, tensFlightElev);
    observation_.set_ambient_field_params(magIndFld.x, magIndFld.y, magIndFld.z);

    // Attempt to read grids (Relief, Observation elevations, Modeled, Difference, Measured)
    if (reader.good() && !reader.eof()) {
        std::streampos curPos = reader.tell();
        reader.stream().seekg(0, std::ios::end);
        std::streampos endPos = reader.stream().tellg();
        reader.seek(curPos);

        bool foundGrids = false;
        if (endPos > curPos) {
            size_t remBytes = static_cast<size_t>(endPos - curPos);
            std::vector<uint8_t> buffer(remBytes);
            reader.readBytes(buffer.data(), remBytes);

            const std::string tag = "_gridMod3D";
            for (size_t i = 0; i + tag.size() <= buffer.size(); ++i) {
                if (std::memcmp(buffer.data() + i, tag.data(), tag.size()) == 0) {
                    size_t candidates[] = { (i >= 1) ? i - 1 : 0, (i >= 3) ? i - 3 : 0 };
                    for (size_t cand : candidates) {
                        if (canRead64Grids(buffer.data(), buffer.size(), cand)) {
                            reader.seek(curPos + static_cast<std::streamoff>(cand));
                            foundGrids = true;
                            break;
                        }
                    }
                    if (foundGrids) break;
                }
            }
        }

        if (!foundGrids) {
            reader.seek(curPos);
            uint32_t extraObjs = reader.readArrayCount();
            (void)extraObjs;
        }

        Grid relGrid = readMfcGrid(reader);
        if (!relGrid.empty()) {
            observation_.set_surface_relief(relGrid);
        }
        Grid grvGrid = readMfcGrid(reader);
        if (!grvGrid.empty()) {
            observation_.set_gravity_observation_grid(grvGrid);
        }
        Grid magGrid = readMfcGrid(reader);
        if (!magGrid.empty()) {
            observation_.set_magnetic_observation_grid(magGrid);
        }
        Grid tnsGrid = readMfcGrid(reader);
        if (!tnsGrid.empty()) {
            observation_.set_tensor_observation_grid(tnsGrid);
        }

        auto legacyCompToFieldComponent = [](int compIdx) -> std::optional<FieldComponent> {
            switch (compIdx) {
            case 0: return FieldComponent::GX;
            case 1: return FieldComponent::GY;
            case 2: return FieldComponent::GZ;
            case 3: return FieldComponent::G_TOT;
            case 4: return FieldComponent::GXX;
            case 5: return FieldComponent::GYY;
            case 6: return FieldComponent::GZZ;
            case 7: return FieldComponent::GXY;
            case 8: return FieldComponent::GXZ;
            case 9: return FieldComponent::GYZ;
            case 10: return FieldComponent::MX;
            case 11: return FieldComponent::MY;
            case 12: return FieldComponent::MZ;
            case 13: return FieldComponent::DELTA_T;
            default: return std::nullopt;
            }
        };

        // Read 60 component grids (m_grd[0..59])
        for (int i = 0; i < 60 && reader.good() && !reader.eof(); ++i) {
            Grid grid = readMfcGrid(reader);
            int32_t activeCompute = 0;
            if (reader.good() && !reader.eof()) {
                activeCompute = reader.read<int32_t>();
            }
            (void)activeCompute;

            if (grid.empty()) continue;

            if (i >= 0 && i < 20) {
                auto comp = legacyCompToFieldComponent(i);
                if (comp) {
                    observation_.set_modeled_grid(*comp, grid);
                }
            } else if (i >= 20 && i < 40) {
                auto comp = legacyCompToFieldComponent(i - 20);
                if (comp) {
                    observation_.set_difference_grid(*comp, grid);
                }
            } else if (i >= 40 && i < 60) {
                auto comp = legacyCompToFieldComponent(i - 40);
                if (comp) {
                    observation_.set_observed_grid(*comp, grid);
                }
            }
        }
    }

    // Regenerate 3D polyhedral facets
    model_.init_facet_list();
    model_.clear_facets_update();
    model_.set_compute_real_time(settings_.compute_real_time);
    model_.compute_body_magnetization_vector(
        observation_.inclination_deg(),
        observation_.declination_deg(),
        observation_.magnetic_intensity());

    return true;
}

} // namespace mod3d
