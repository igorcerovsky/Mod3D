#pragma once

#include "mod3d/Facet3Pt.h"
#include "mod3d/Grid.h"
#include "mod3d/Inversion.h"
#include "mod3d/Model.h"
#include "mod3d/Point3D.h"
#include "mod3d/Units.h"
#include <map>
#include <numbers>
#include <vector>

namespace mod3d {

enum class ObservationMode {
  SensorHeight = 0,    // Surface relief + sensor altitude (h_sensor)
  FlightElevation = 1, // Constant absolute flight elevation level
  ElevationGrid = 2    // Arbitrary 2D observation elevation grid
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

[[nodiscard]] constexpr bool
is_gravity_component(FieldComponent comp) noexcept {
  return static_cast<int>(comp) >= 0 && static_cast<int>(comp) < 10;
}

[[nodiscard]] constexpr bool
is_magnetic_component(FieldComponent comp) noexcept {
  return static_cast<int>(comp) >= 10 && static_cast<int>(comp) < 20;
}

[[nodiscard]] constexpr bool is_tensor_component(FieldComponent comp) noexcept {
  return static_cast<int>(comp) >= 20;
}

/**
 * @brief Geophysical Observation Space & Forward Modeling / Inversion
 * Coordinator.
 *
 * Manages observation geometry, flight levels, modeled/observed/difference
 * grids, forward field simulation, real-time delta updates, and 1D inversions.
 */
class ObservationSpace {
public:
  // ========================================================================
  // Lifecycle & Rule of 5
  // ========================================================================
  ObservationSpace();
  ObservationSpace(size_t rows, size_t cols, double x0, double y0, double dx,
                   double dy, double rotDeg = 0.0);
  ObservationSpace(size_t rows, size_t cols, units::Meters x0, units::Meters y0,
                   units::Meters dx, units::Meters dy,
                   units::Degrees rotDeg = units::Degrees(0.0));

  template <units::UnitType U0, units::UnitType Us,
            units::UnitType Ur = units::tag::Degrees>
    requires(units::SameDimension<U0, units::tag::Meters> &&
             units::SameDimension<Us, units::tag::Meters> &&
             units::SameDimension<Ur, units::tag::Degrees>)
  ObservationSpace(size_t rows, size_t cols, const units::Quantity<U0> &x0,
                   const units::Quantity<U0> &y0, const units::Quantity<Us> &dx,
                   const units::Quantity<Us> &dy,
                   const units::Quantity<Ur> &rot = units::Degrees(0.0))
      : ObservationSpace(rows, cols, x0.template as<units::tag::Meters>(),
                         y0.template as<units::tag::Meters>(),
                         dx.template as<units::tag::Meters>(),
                         dy.template as<units::tag::Meters>(),
                         rot.template as<units::tag::Degrees>()) {}
  ~ObservationSpace() = default;

  ObservationSpace(const ObservationSpace &) = default;
  ObservationSpace &operator=(const ObservationSpace &) = default;
  ObservationSpace(ObservationSpace &&) noexcept = default;
  ObservationSpace &operator=(ObservationSpace &&) noexcept = default;

  void swap(ObservationSpace &other) noexcept;
  friend void swap(ObservationSpace &a, ObservationSpace &b) noexcept {
    a.swap(b);
  }

  void init_geometry(size_t rows, size_t cols, double x0, double y0, double dx,
                     double dy, double rotDeg = 0.0);
  void init_geometry(size_t rows, size_t cols, units::Meters x0, units::Meters y0,
                     units::Meters dx, units::Meters dy,
                     units::Degrees rotDeg = units::Degrees(0.0));

  template <units::UnitType U0, units::UnitType Us,
            units::UnitType Ur = units::tag::Degrees>
    requires(units::SameDimension<U0, units::tag::Meters> &&
             units::SameDimension<Us, units::tag::Meters> &&
             units::SameDimension<Ur, units::tag::Degrees>)
  void init_geometry(size_t rows, size_t cols, const units::Quantity<U0> &x0,
                     const units::Quantity<U0> &y0, const units::Quantity<Us> &dx,
                     const units::Quantity<Us> &dy,
                     const units::Quantity<Ur> &rot = units::Degrees(0.0)) {
    init_geometry(rows, cols, x0.template as<units::tag::Meters>(),
                  y0.template as<units::tag::Meters>(),
                  dx.template as<units::tag::Meters>(),
                  dy.template as<units::tag::Meters>(),
                  rot.template as<units::tag::Degrees>());
  }

  // ========================================================================
  // Modern C++20 API (STL snake_case convention, noexcept, [[nodiscard]])
  // ========================================================================

  // Dimensions & Geometry (Canonical Unit Accessors)
  [[nodiscard]] size_t rows() const noexcept { return rows_; }
  [[nodiscard]] size_t cols() const noexcept { return cols_; }
  [[nodiscard]] constexpr units::Meters x0() const noexcept { return x0_; }
  [[nodiscard]] constexpr units::Meters y0() const noexcept { return y0_; }
  [[nodiscard]] constexpr units::Meters dx() const noexcept { return dx_; }
  [[nodiscard]] constexpr units::Meters dy() const noexcept { return dy_; }
  [[nodiscard]] constexpr units::Degrees rotation() const noexcept { return rotation_; }
  [[nodiscard]] constexpr units::Degrees rotation_deg() const noexcept { return rotation_; }
  [[nodiscard]] units::Radians rotation_rad() const noexcept {
    return units::Radians(rotation_.value() * std::numbers::pi_v<double> / 180.0);
  }

  // Gravity Observation Settings
  void set_gravity_observation(ObservationMode mode, double heightVal);
  void set_gravity_observation(ObservationMode mode, units::Meters heightVal);
  void set_gravity_observation_grid(const Grid &grid);

  template <units::UnitType U>
    requires units::SameDimension<U, units::tag::Meters>
  void set_gravity_observation(ObservationMode mode,
                               const units::Quantity<U> &heightVal) {
    set_gravity_observation(mode, heightVal.template as<units::tag::Meters>());
  }

  [[nodiscard]] ObservationMode gravity_mode() const noexcept {
    return grv_mode_;
  }
  [[nodiscard]] constexpr units::Meters gravity_height() const noexcept { return grv_height_; }
  [[nodiscard]] const Grid &gravity_elevation_grid() const noexcept {
    return grv_elev_grid_;
  }

  // Magnetic Observation Settings
  void set_magnetic_observation(ObservationMode mode, double heightVal);
  void set_magnetic_observation(ObservationMode mode, units::Meters heightVal);
  void set_magnetic_observation_grid(const Grid &grid);

  template <units::UnitType U>
    requires units::SameDimension<U, units::tag::Meters>
  void set_magnetic_observation(ObservationMode mode,
                                const units::Quantity<U> &heightVal) {
    set_magnetic_observation(mode, heightVal.template as<units::tag::Meters>());
  }

  [[nodiscard]] ObservationMode magnetic_mode() const noexcept {
    return mag_mode_;
  }
  [[nodiscard]] constexpr units::Meters magnetic_height() const noexcept { return mag_height_; }
  [[nodiscard]] const Grid &magnetic_elevation_grid() const noexcept {
    return mag_elev_grid_;
  }

  // Tensor Observation Settings
  void set_tensor_observation(ObservationMode mode, double heightVal);
  void set_tensor_observation(ObservationMode mode, units::Meters heightVal);
  void set_tensor_observation_grid(const Grid &grid);

  template <units::UnitType U>
    requires units::SameDimension<U, units::tag::Meters>
  void set_tensor_observation(ObservationMode mode,
                              const units::Quantity<U> &heightVal) {
    set_tensor_observation(mode, heightVal.template as<units::tag::Meters>());
  }

  [[nodiscard]] ObservationMode tensor_mode() const noexcept {
    return tensor_mode_;
  }
  [[nodiscard]] constexpr units::Meters tensor_height() const noexcept { return tensor_height_; }
  [[nodiscard]] const Grid &tensor_elevation_grid() const noexcept {
    return tensor_elev_grid_;
  }

  // Surface Relief
  void set_surface_relief(const Grid &relief);
  [[nodiscard]] const Grid &surface_relief() const noexcept {
    return relief_grid_;
  }

  // Ambient Geomagnetic Field Parameters
  void set_ambient_field_params(double inclinationDeg, double declinationDeg,
                                double intensityNT);
  void set_ambient_field_params(units::Degrees inclination,
                                units::Degrees declination,
                                units::NanoTesla intensity);

  template <units::UnitType IncU, units::UnitType DecU,
            units::UnitType IntensityU>
    requires(units::SameDimension<IncU, units::tag::Degrees> &&
             units::SameDimension<DecU, units::tag::Degrees> &&
             units::SameDimension<IntensityU, units::tag::NanoTesla>)
  void set_ambient_field_params(const units::Quantity<IncU> &inclination,
                                const units::Quantity<DecU> &declination,
                                const units::Quantity<IntensityU> &intensity) {
    set_ambient_field_params(inclination.template as<units::tag::Degrees>(),
                             declination.template as<units::tag::Degrees>(),
                             intensity.template as<units::tag::NanoTesla>());
  }

  [[nodiscard]] MagneticVector ambient_field_vector() const;

  [[nodiscard]] constexpr units::Degrees inclination() const noexcept { return inclination_; }
  [[nodiscard]] constexpr units::Degrees declination() const noexcept { return declination_; }
  [[nodiscard]] constexpr units::Degrees inclination_deg() const noexcept { return inclination_; }
  [[nodiscard]] constexpr units::Degrees declination_deg() const noexcept { return declination_; }
  [[nodiscard]] constexpr units::NanoTesla magnetic_intensity() const noexcept {
    return mag_intensity_;
  }

  // Reference Density
  void set_reference_density(double refDens) noexcept {
    ref_density_ = units::KgPerM3(refDens);
  }
  void set_reference_density(units::KgPerM3 refDens) noexcept {
    ref_density_ = refDens;
  }

  template <units::UnitType U>
    requires units::SameDimension<U, units::tag::KgPerM3>
  void set_reference_density(const units::Quantity<U> &refDens) noexcept {
    set_reference_density(refDens.template in<units::tag::KgPerM3>());
  }

  [[nodiscard]] constexpr units::KgPerM3 reference_density() const noexcept {
    return ref_density_;
  }

  // Gravity Field Unit convention (GravityUnit [g.u., 1e-6 m/s²] vs MilliGal [mGal, 1e-5 m/s²])
  void set_gravity_field_unit(GravityFieldUnit unit) noexcept { grv_field_unit_ = unit; }
  [[nodiscard]] constexpr GravityFieldUnit gravity_field_unit() const noexcept { return grv_field_unit_; }

  [[nodiscard]] constexpr double gravity_scale() const noexcept {
    return gravity_unit_scale(grv_field_unit_);
  }

  // Observation Point Generation
  [[nodiscard]] std::vector<Point3D>
  observation_points(FieldComponent comp) const;

  // Grid Management
  [[nodiscard]] const std::map<FieldComponent, Grid> &
  modeled_grids() const noexcept {
    return modeled_grids_;
  }
  [[nodiscard]] const std::map<FieldComponent, Grid> &
  observed_grids() const noexcept {
    return observed_grids_;
  }
  [[nodiscard]] const std::map<FieldComponent, Grid> &
  difference_grids() const noexcept {
    return difference_grids_;
  }

  [[nodiscard]] Grid *modeled_grid(FieldComponent comp);
  [[nodiscard]] const Grid *modeled_grid(FieldComponent comp) const;
  void set_modeled_grid(FieldComponent comp, const Grid &grid);

  [[nodiscard]] Grid *observed_grid(FieldComponent comp);
  [[nodiscard]] const Grid *observed_grid(FieldComponent comp) const;
  void set_observed_grid(FieldComponent comp, const Grid &grid);

  [[nodiscard]] Grid *difference_grid(FieldComponent comp);
  [[nodiscard]] const Grid *difference_grid(FieldComponent comp) const;
  void set_difference_grid(FieldComponent comp, const Grid &grid);

  // Forward Modeling & Difference Computation
  void compute_forward_field(const std::vector<Facet3Pt> &facets,
                             bool zeroFirst = true);
  void update_delta_field(const std::vector<Facet3Pt> &deltaFacets);
  void compute_total_fields();
  void compute_difference(FieldComponent comp, bool removeMean = false);

  // Virtual mean removal of the measured (observed) field.
  // Raw observed grids are never modified; the mean of every observed
  // component is stored, and while enabled the const observed_grid()
  // accessor, the difference grids and the inversions work with the virtual
  // anomaly (observed - stored mean).
  void set_remove_observed_mean(bool enable);
  [[nodiscard]] bool remove_observed_mean() const noexcept {
    return remove_observed_mean_;
  }
  [[nodiscard]] double observed_mean(FieldComponent comp) const;
  [[nodiscard]] const Grid *raw_observed_grid(FieldComponent comp) const;

  // Automated 1D Inversion
  FitResult fit_density(Body *pBody, const std::vector<Facet3Pt> &bodyFacets,
                        FieldComponent comp = FieldComponent::GZ,
                        double initialDensity = 2670.0, double tol = 1e-4,
                        FitMethod method = FitMethod::Brent);

  FitResult fit_vertex(Model &model, int row, int col, int index,
                       FieldComponent comp = FieldComponent::GZ,
                       double tol = 1e-4, FitMethod method = FitMethod::Brent);

private:
  void ensure_grid_allocated(std::map<FieldComponent, Grid> &gridMap,
                             FieldComponent comp);
  void update_virtual_observed_anomalies();
  double get_elevation_at(ObservationMode mode, double heightVal,
                          const Grid &elevGrid, size_t r, size_t c) const;
  double get_elevation_at(ObservationMode mode, units::Meters heightVal,
                          const Grid &elevGrid, size_t r, size_t c) const {
    return get_elevation_at(mode, heightVal.value(), elevGrid, r, c);
  }

  size_t rows_{0};
  size_t cols_{0};
  units::Meters x0_{0.0};
  units::Meters y0_{0.0};
  units::Meters dx_{100.0};
  units::Meters dy_{100.0};
  units::Degrees rotation_{0.0};

  Grid relief_grid_;

  ObservationMode grv_mode_{ObservationMode::SensorHeight};
  units::Meters grv_height_{0.0};
  Grid grv_elev_grid_;

  ObservationMode mag_mode_{ObservationMode::SensorHeight};
  units::Meters mag_height_{0.0};
  Grid mag_elev_grid_;

  ObservationMode tensor_mode_{ObservationMode::SensorHeight};
  units::Meters tensor_height_{0.0};
  Grid tensor_elev_grid_;

  units::Degrees inclination_{60.0};
  units::Degrees declination_{0.0};
  units::NanoTesla mag_intensity_{50000.0};
  units::KgPerM3 ref_density_{2670.0};
  GravityFieldUnit grv_field_unit_{GravityFieldUnit::GravityUnit};

  std::map<FieldComponent, Grid> modeled_grids_;
  std::map<FieldComponent, Grid> observed_grids_;
  std::map<FieldComponent, Grid> difference_grids_;

  bool remove_observed_mean_{false};
  std::map<FieldComponent, double> observed_means_;
  std::map<FieldComponent, Grid> observed_anomaly_grids_;
};

} // namespace mod3d
