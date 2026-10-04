#pragma once

#include "mod3d/Grid.h"
#include "mod3d/Point3D.h"
#include "mod3d/Facet3Pt.h"
#include "mod3d/Inversion.h"
#include "mod3d/Model.h"
#include <vector>
#include <string>
#include <memory>
#include <map>
#include <numbers>
#include <utility>

namespace mod3d {

enum class ObservationMode {
    SensorHeight = 0,      // Surface relief + sensor altitude (h_sensor)
    FlightElevation = 1,   // Constant absolute flight elevation level
    ElevationGrid = 2      // Arbitrary 2D observation elevation grid
};

enum class FieldComponent {
    // Gravity components
    GX = 0,
    GY = 1,
    GZ = 2,
    G_TOT = 3,

    // Magnetic components
    MX = 10,
    MY = 11,
    MZ = 12,
    DELTA_T = 13,

    // Gravity gradient tensor components
    GXX = 20,
    GYY = 21,
    GZZ = 22,
    GXY = 23,
    GXZ = 24,
    GYZ = 25
};

[[nodiscard]] constexpr bool is_gravity_component(FieldComponent comp) noexcept {
    return static_cast<int>(comp) >= 0 && static_cast<int>(comp) < 10;
}

[[nodiscard]] constexpr bool is_magnetic_component(FieldComponent comp) noexcept {
    return static_cast<int>(comp) >= 10 && static_cast<int>(comp) < 20;
}

[[nodiscard]] constexpr bool is_tensor_component(FieldComponent comp) noexcept {
    return static_cast<int>(comp) >= 20;
}

/**
 * @brief Geophysical Observation Space & Forward Modeling / Inversion Coordinator.
 * 
 * Manages observation geometry, flight levels, modeled/observed/difference grids,
 * forward field simulation, real-time delta updates, and 1D inversions.
 */
class ObservationSpace {
public:
    // ========================================================================
    // Lifecycle & Rule of 5
    // ========================================================================
    ObservationSpace();
    ObservationSpace(size_t rows, size_t cols, double x0, double y0, double dx, double dy, double rotDeg = 0.0);
    ~ObservationSpace() = default;

    ObservationSpace(const ObservationSpace &) = default;
    ObservationSpace &operator=(const ObservationSpace &) = default;
    ObservationSpace(ObservationSpace &&) noexcept = default;
    ObservationSpace &operator=(ObservationSpace &&) noexcept = default;

    void swap(ObservationSpace &other) noexcept;
    friend void swap(ObservationSpace &a, ObservationSpace &b) noexcept { a.swap(b); }

    void init_geometry(size_t rows, size_t cols, double x0, double y0, double dx, double dy, double rotDeg = 0.0);

    // ========================================================================
    // Modern C++20 API (STL snake_case convention, noexcept, [[nodiscard]])
    // ========================================================================

    // Dimensions & Geometry
    [[nodiscard]] size_t rows() const noexcept { return rows_; }
    [[nodiscard]] size_t cols() const noexcept { return cols_; }
    [[nodiscard]] double x0() const noexcept { return x0_; }
    [[nodiscard]] double y0() const noexcept { return y0_; }
    [[nodiscard]] double dx() const noexcept { return dx_; }
    [[nodiscard]] double dy() const noexcept { return dy_; }
    [[nodiscard]] double rotation_deg() const noexcept { return rot_deg_; }
    [[nodiscard]] double rotation_rad() const noexcept { return rot_deg_ * std::numbers::pi_v<double> / 180.0; }

    // Gravity Observation Settings
    void set_gravity_observation(ObservationMode mode, double heightVal);
    void set_gravity_observation_grid(const Grid &grid);
    [[nodiscard]] ObservationMode gravity_mode() const noexcept { return grv_mode_; }
    [[nodiscard]] double gravity_height() const noexcept { return grv_height_; }
    [[nodiscard]] const Grid &gravity_elevation_grid() const noexcept { return grv_elev_grid_; }

    // Magnetic Observation Settings
    void set_magnetic_observation(ObservationMode mode, double heightVal);
    void set_magnetic_observation_grid(const Grid &grid);
    [[nodiscard]] ObservationMode magnetic_mode() const noexcept { return mag_mode_; }
    [[nodiscard]] double magnetic_height() const noexcept { return mag_height_; }
    [[nodiscard]] const Grid &magnetic_elevation_grid() const noexcept { return mag_elev_grid_; }

    // Tensor Observation Settings
    void set_tensor_observation(ObservationMode mode, double heightVal);
    void set_tensor_observation_grid(const Grid &grid);
    [[nodiscard]] ObservationMode tensor_mode() const noexcept { return tensor_mode_; }
    [[nodiscard]] double tensor_height() const noexcept { return tensor_height_; }
    [[nodiscard]] const Grid &tensor_elevation_grid() const noexcept { return tensor_elev_grid_; }

    // Surface Relief
    void set_surface_relief(const Grid &relief);
    [[nodiscard]] const Grid &surface_relief() const noexcept { return relief_grid_; }

    // Ambient Geomagnetic Field Parameters
    void set_ambient_field_params(double inclinationDeg, double declinationDeg, double intensityNT);
    [[nodiscard]] Point3D ambient_field_vector() const;
    [[nodiscard]] double inclination_deg() const noexcept { return inc_deg_; }
    [[nodiscard]] double declination_deg() const noexcept { return dec_deg_; }
    [[nodiscard]] double magnetic_intensity() const noexcept { return mag_intensity_; }

    // Reference Density
    void set_reference_density(double refDens) noexcept { ref_density_ = refDens; }
    [[nodiscard]] double reference_density() const noexcept { return ref_density_; }

    // Observation Point Generation
    [[nodiscard]] std::vector<Point3D> observation_points(FieldComponent comp) const;

    // Grid Management
    [[nodiscard]] const std::map<FieldComponent, Grid> &modeled_grids() const noexcept { return modeled_grids_; }
    [[nodiscard]] const std::map<FieldComponent, Grid> &observed_grids() const noexcept { return observed_grids_; }
    [[nodiscard]] const std::map<FieldComponent, Grid> &difference_grids() const noexcept { return difference_grids_; }

    [[nodiscard]] Grid *modeled_grid(FieldComponent comp);
    [[nodiscard]] const Grid *modeled_grid(FieldComponent comp) const;

    [[nodiscard]] Grid *observed_grid(FieldComponent comp);
    [[nodiscard]] const Grid *observed_grid(FieldComponent comp) const;
    void set_observed_grid(FieldComponent comp, const Grid &grid);

    [[nodiscard]] Grid *difference_grid(FieldComponent comp);
    [[nodiscard]] const Grid *difference_grid(FieldComponent comp) const;

    // Forward Modeling & Difference Computation
    void compute_forward_field(const std::vector<Facet3Pt> &facets, bool zeroFirst = true);
    void update_delta_field(const std::vector<Facet3Pt> &deltaFacets);
    void compute_total_fields();
    void compute_difference(FieldComponent comp, bool removeMean = false);

    // Automated 1D Inversion
    FitResult fit_density(
        Body *pBody,
        const std::vector<Facet3Pt> &bodyFacets,
        FieldComponent comp = FieldComponent::GZ,
        double initialDensity = 2670.0,
        double tol = 1e-4,
        FitMethod method = FitMethod::Brent);

    FitResult fit_vertex(
        Model &model,
        int row, int col, int index,
        FieldComponent comp = FieldComponent::GZ,
        double tol = 1e-4,
        FitMethod method = FitMethod::Brent);

    // ========================================================================
    // Legacy API (CamelCase compatibility wrappers for MFC Mod3D codebase)
    // ========================================================================
    void initGeometry(size_t rows, size_t cols, double x0, double y0, double dx, double dy, double rotDeg = 0.0) {
        init_geometry(rows, cols, x0, y0, dx, dy, rotDeg);
    }

    [[nodiscard]] size_t getRows() const noexcept { return rows(); }
    [[nodiscard]] size_t getCols() const noexcept { return cols(); }
    [[nodiscard]] double getX0() const noexcept { return x0(); }
    [[nodiscard]] double getY0() const noexcept { return y0(); }
    [[nodiscard]] double getDx() const noexcept { return dx(); }
    [[nodiscard]] double getDy() const noexcept { return dy(); }
    [[nodiscard]] double getRotDeg() const noexcept { return rotation_deg(); }

    void setGravityObservation(ObservationMode mode, double heightVal) { set_gravity_observation(mode, heightVal); }
    void setGravityObservationGrid(const Grid &grid) { set_gravity_observation_grid(grid); }
    [[nodiscard]] ObservationMode getGravityMode() const noexcept { return gravity_mode(); }
    [[nodiscard]] double getGravityHeight() const noexcept { return gravity_height(); }
    [[nodiscard]] const Grid &getGravityElevGrid() const noexcept { return gravity_elevation_grid(); }

    void setMagneticObservation(ObservationMode mode, double heightVal) { set_magnetic_observation(mode, heightVal); }
    void setMagneticObservationGrid(const Grid &grid) { set_magnetic_observation_grid(grid); }
    [[nodiscard]] ObservationMode getMagneticMode() const noexcept { return magnetic_mode(); }
    [[nodiscard]] double getMagneticHeight() const noexcept { return magnetic_height(); }
    [[nodiscard]] const Grid &getMagneticElevGrid() const noexcept { return magnetic_elevation_grid(); }

    void setTensorObservation(ObservationMode mode, double heightVal) { set_tensor_observation(mode, heightVal); }
    void setTensorObservationGrid(const Grid &grid) { set_tensor_observation_grid(grid); }
    [[nodiscard]] ObservationMode getTensorMode() const noexcept { return tensor_mode(); }
    [[nodiscard]] double getTensorHeight() const noexcept { return tensor_height(); }
    [[nodiscard]] const Grid &getTensorElevGrid() const noexcept { return tensor_elevation_grid(); }

    void setSurfaceRelief(const Grid &relief) { set_surface_relief(relief); }
    [[nodiscard]] const Grid &getSurfaceRelief() const noexcept { return surface_relief(); }

    void setAmbientFieldParams(double inclinationDeg, double declinationDeg, double intensityNT) {
        set_ambient_field_params(inclinationDeg, declinationDeg, intensityNT);
    }
    [[nodiscard]] Point3D getAmbientFieldVector() const { return ambient_field_vector(); }
    [[nodiscard]] double getInclinationDeg() const noexcept { return inclination_deg(); }
    [[nodiscard]] double getDeclinationDeg() const noexcept { return declination_deg(); }
    [[nodiscard]] double getMagneticIntensity() const noexcept { return magnetic_intensity(); }

    void setReferenceDensity(double refDens) noexcept { set_reference_density(refDens); }
    [[nodiscard]] double getReferenceDensity() const noexcept { return reference_density(); }

    [[nodiscard]] const std::map<FieldComponent, Grid> &getModeledGrids() const noexcept { return modeled_grids(); }
    [[nodiscard]] const std::map<FieldComponent, Grid> &getObservedGrids() const noexcept { return observed_grids(); }
    [[nodiscard]] const std::map<FieldComponent, Grid> &getDifferenceGrids() const noexcept { return difference_grids(); }

    [[nodiscard]] std::vector<Point3D> getObservationPoints(FieldComponent comp) const { return observation_points(comp); }

    [[nodiscard]] Grid *getModeledGrid(FieldComponent comp) { return modeled_grid(comp); }
    [[nodiscard]] const Grid *getModeledGrid(FieldComponent comp) const { return modeled_grid(comp); }

    [[nodiscard]] Grid *getObservedGrid(FieldComponent comp) { return observed_grid(comp); }
    [[nodiscard]] const Grid *getObservedGrid(FieldComponent comp) const { return observed_grid(comp); }
    void setObservedGrid(FieldComponent comp, const Grid &grid) { set_observed_grid(comp, grid); }

    [[nodiscard]] Grid *getDifferenceGrid(FieldComponent comp) { return difference_grid(comp); }
    [[nodiscard]] const Grid *getDifferenceGrid(FieldComponent comp) const { return difference_grid(comp); }

    void computeForwardField(const std::vector<Facet3Pt> &facets, bool zeroFirst = true) {
        compute_forward_field(facets, zeroFirst);
    }
    void updateDeltaField(const std::vector<Facet3Pt> &deltaFacets) { update_delta_field(deltaFacets); }
    void computeTotalFields() { compute_total_fields(); }
    void computeDifference(FieldComponent comp, bool removeMean = false) { compute_difference(comp, removeMean); }

    FitResult fitDensity(
        Body *pBody,
        const std::vector<Facet3Pt> &bodyFacets,
        FieldComponent comp = FieldComponent::GZ,
        double initialDensity = 2670.0,
        double tol = 1e-4,
        FitMethod method = FitMethod::Brent) {
        return fit_density(pBody, bodyFacets, comp, initialDensity, tol, method);
    }

    FitResult fitVertex(
        Model &model,
        int row, int col, int index,
        FieldComponent comp = FieldComponent::GZ,
        double tol = 1e-4,
        FitMethod method = FitMethod::Brent) {
        return fit_vertex(model, row, col, index, comp, tol, method);
    }

private:
    void ensure_grid_allocated(std::map<FieldComponent, Grid> &gridMap, FieldComponent comp);
    double get_elevation_at(ObservationMode mode, double heightVal, const Grid &elevGrid, size_t r, size_t c) const;

    size_t rows_{0};
    size_t cols_{0};
    double x0_{0.0};
    double y0_{0.0};
    double dx_{100.0};
    double dy_{100.0};
    double rot_deg_{0.0};

    Grid relief_grid_;

    ObservationMode grv_mode_{ObservationMode::SensorHeight};
    double grv_height_{0.0};
    Grid grv_elev_grid_;

    ObservationMode mag_mode_{ObservationMode::SensorHeight};
    double mag_height_{0.0};
    Grid mag_elev_grid_;

    ObservationMode tensor_mode_{ObservationMode::SensorHeight};
    double tensor_height_{0.0};
    Grid tensor_elev_grid_;

    double inc_deg_{60.0};
    double dec_deg_{0.0};
    double mag_intensity_{50000.0};
    double ref_density_{2670.0};

    std::map<FieldComponent, Grid> modeled_grids_;
    std::map<FieldComponent, Grid> observed_grids_;
    std::map<FieldComponent, Grid> difference_grids_;
};

} // namespace mod3d
