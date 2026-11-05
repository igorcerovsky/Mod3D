#include "mod3d/Observation.h"
#include "mod3d/Body.h"
#include "mod3d/Magnetics.h"
#include "pfld/pfld_compute.hpp"
#include "inv/inv.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace mod3d {

ObservationSpace::ObservationSpace() = default;

ObservationSpace::ObservationSpace(size_t rows, size_t cols, double x0,
                                   double y0, double dx, double dy,
                                   double rotDeg) {
  init_geometry(rows, cols, units::Meters(x0), units::Meters(y0),
                units::Meters(dx), units::Meters(dy), units::Degrees(rotDeg));
}

ObservationSpace::ObservationSpace(size_t rows, size_t cols, units::Meters x0,
                                   units::Meters y0, units::Meters dx,
                                   units::Meters dy, units::Degrees rot) {
  init_geometry(rows, cols, x0, y0, dx, dy, rot);
}

void ObservationSpace::swap(ObservationSpace &other) noexcept {
  using std::swap;
  swap(rows_, other.rows_);
  swap(cols_, other.cols_);
  swap(x0_, other.x0_);
  swap(y0_, other.y0_);
  swap(dx_, other.dx_);
  swap(dy_, other.dy_);
  swap(rotation_, other.rotation_);
  swap(relief_grid_, other.relief_grid_);
  swap(grv_mode_, other.grv_mode_);
  swap(grv_height_, other.grv_height_);
  swap(grv_elev_grid_, other.grv_elev_grid_);
  swap(mag_mode_, other.mag_mode_);
  swap(mag_height_, other.mag_height_);
  swap(mag_elev_grid_, other.mag_elev_grid_);
  swap(tensor_mode_, other.tensor_mode_);
  swap(tensor_height_, other.tensor_height_);
  swap(tensor_elev_grid_, other.tensor_elev_grid_);
  swap(inclination_, other.inclination_);
  swap(declination_, other.declination_);
  swap(mag_intensity_, other.mag_intensity_);
  swap(ref_density_, other.ref_density_);
  swap(grv_field_unit_, other.grv_field_unit_);
  swap(modeled_grids_, other.modeled_grids_);
  swap(observed_grids_, other.observed_grids_);
  swap(difference_grids_, other.difference_grids_);
  swap(background_trend_mode_, other.background_trend_mode_);
  swap(observed_means_, other.observed_means_);
  swap(observed_trends_, other.observed_trends_);
  swap(observed_anomaly_grids_, other.observed_anomaly_grids_);
}

void ObservationSpace::init_geometry(size_t rows, size_t cols, double x0,
                                     double y0, double dx, double dy,
                                     double rotDeg) {
  init_geometry(rows, cols, units::Meters(x0), units::Meters(y0),
                units::Meters(dx), units::Meters(dy), units::Degrees(rotDeg));
}

void ObservationSpace::init_geometry(size_t rows, size_t cols, units::Meters x0,
                                     units::Meters y0, units::Meters dx,
                                     units::Meters dy, units::Degrees rotDeg) {
  rows_ = rows;
  cols_ = cols;
  x0_ = x0;
  y0_ = y0;
  dx_ = dx;
  dy_ = dy;
  rotation_ = rotDeg;

  relief_grid_ = Grid(rows, cols, x0, y0, dx, dy, rotDeg);
  grv_elev_grid_ = Grid(rows, cols, x0, y0, dx, dy, rotDeg);
  mag_elev_grid_ = Grid(rows, cols, x0, y0, dx, dy, rotDeg);
  tensor_elev_grid_ = Grid(rows, cols, x0, y0, dx, dy, rotDeg);
}

void ObservationSpace::set_gravity_observation(ObservationMode mode,
                                               double heightVal) {
  set_gravity_observation(mode, units::Meters(heightVal));
}

void ObservationSpace::set_gravity_observation(ObservationMode mode,
                                               units::Meters heightVal) {
  grv_mode_ = mode;
  grv_height_ = heightVal;
}

void ObservationSpace::set_gravity_observation_grid(const Grid &grid) {
  grv_mode_ = ObservationMode::ElevationGrid;
  grv_elev_grid_ = grid;
}

void ObservationSpace::set_magnetic_observation(ObservationMode mode,
                                                double heightVal) {
  set_magnetic_observation(mode, units::Meters(heightVal));
}

void ObservationSpace::set_magnetic_observation(ObservationMode mode,
                                                units::Meters heightVal) {
  mag_mode_ = mode;
  mag_height_ = heightVal;
}

void ObservationSpace::set_magnetic_observation_grid(const Grid &grid) {
  mag_mode_ = ObservationMode::ElevationGrid;
  mag_elev_grid_ = grid;
}

void ObservationSpace::set_tensor_observation(ObservationMode mode,
                                              double heightVal) {
  set_tensor_observation(mode, units::Meters(heightVal));
}

void ObservationSpace::set_tensor_observation(ObservationMode mode,
                                              units::Meters heightVal) {
  tensor_mode_ = mode;
  tensor_height_ = heightVal;
}

void ObservationSpace::set_tensor_observation_grid(const Grid &grid) {
  tensor_mode_ = ObservationMode::ElevationGrid;
  tensor_elev_grid_ = grid;
}

void ObservationSpace::set_surface_relief(const Grid &relief) {
  if (relief.empty() || (rows_ == 0 || cols_ == 0) ||
      (relief.rows() == rows_ && relief.cols() == cols_)) {
    relief_grid_ = relief;
  } else {
    Grid resampled(rows_, cols_, x0_, y0_, dx_, dy_, rotation_);
    const double pi = std::numbers::pi_v<double>;
    const double rad = rotation_.value() * pi / 180.0;
    const double cRot = std::cos(rad);
    const double sRot = std::sin(rad);

    for (size_t r = 0; r < rows_; ++r) {
      for (size_t c = 0; c < cols_; ++c) {
        double x =
            x0_.value() + (c * dx_.value()) * cRot - (r * dy_.value()) * sRot;
        double y =
            y0_.value() + (c * dx_.value()) * sRot + (r * dy_.value()) * cRot;
        double v = relief.interpolate(x, y);
        if (v == GRID_DUMMY) {
          double cx =
              std::clamp(x, relief.x_min().value(), relief.x_max().value());
          double cy =
              std::clamp(y, relief.y_min().value(), relief.y_max().value());
          v = relief.interpolate(cx, cy);
          if (v == GRID_DUMMY) {
            v = 0.5 * (relief.z_min().value() + relief.z_max().value());
          }
        }
        resampled(r, c) = v;
      }
    }
    relief_grid_ = resampled;
  }
}

void ObservationSpace::set_ambient_field_params(double inclinationDeg,
                                                double declinationDeg,
                                                double intensityNT) {
  set_ambient_field_params(units::Degrees(inclinationDeg),
                           units::Degrees(declinationDeg),
                           units::NanoTesla(intensityNT));
}

void ObservationSpace::set_ambient_field_params(units::Degrees inclination,
                                                units::Degrees declination,
                                                units::NanoTesla intensity) {
  inclination_ = inclination;
  declination_ = declination;
  mag_intensity_ = intensity;
}

MagneticVector ObservationSpace::ambient_field_vector() const {
  Point3D af = ambient_field(inclination_.value(), declination_.value(),
                             mag_intensity_.value());
  return MagneticVector(af);
}

double ObservationSpace::get_elevation_at(ObservationMode mode,
                                          double heightVal,
                                          const Grid &elevGrid, size_t r,
                                          size_t c) const {
  switch (mode) {
  case ObservationMode::SensorHeight:
    if (!relief_grid_.empty()) {
      return relief_grid_(r, c) + heightVal;
    }
    return heightVal;
  case ObservationMode::FlightElevation:
    return heightVal;
  case ObservationMode::ElevationGrid:
    if (!elevGrid.empty()) {
      return elevGrid(r, c);
    }
    return heightVal;
  }
  return heightVal;
}

std::vector<Point3D>
ObservationSpace::observation_points(FieldComponent comp) const {
  std::vector<Point3D> points;
  if (rows_ == 0 || cols_ == 0)
    return points;

  points.reserve(rows_ * cols_);

  ObservationMode mode = grv_mode_;
  double heightVal = grv_height_.value();
  const Grid *elevGrid = &grv_elev_grid_;

  if (is_magnetic_component(comp)) {
    mode = mag_mode_;
    heightVal = mag_height_.value();
    elevGrid = &mag_elev_grid_;
  } else if (is_tensor_component(comp)) {
    mode = tensor_mode_;
    heightVal = tensor_height_.value();
    elevGrid = &tensor_elev_grid_;
  }

  const double pi = std::numbers::pi_v<double>;
  const double rad = rotation_.value() * pi / 180.0;
  const double cRot = std::cos(rad);
  const double sRot = std::sin(rad);

  for (size_t r = 0; r < rows_; ++r) {
    for (size_t c = 0; c < cols_; ++c) {
      double x =
          x0_.value() + (c * dx_.value()) * cRot - (r * dy_.value()) * sRot;
      double y =
          y0_.value() + (c * dx_.value()) * sRot + (r * dy_.value()) * cRot;
      double z = get_elevation_at(mode, heightVal, *elevGrid, r, c);
      points.emplace_back(x, y, z);
    }
  }

  return points;
}

void ObservationSpace::ensure_grid_allocated(
    std::map<FieldComponent, Grid> &gridMap, FieldComponent comp) {
  auto it = gridMap.find(comp);
  if (it == gridMap.end()) {
    gridMap.emplace(comp, Grid(rows_, cols_, x0_, y0_, dx_, dy_, rotation_));
  }
}

Grid *ObservationSpace::modeled_grid(FieldComponent comp) {
  ensure_grid_allocated(modeled_grids_, comp);
  return &modeled_grids_[comp];
}

const Grid *ObservationSpace::modeled_grid(FieldComponent comp) const {
  auto it = modeled_grids_.find(comp);
  if (it != modeled_grids_.end())
    return &it->second;
  return nullptr;
}

Grid *ObservationSpace::observed_grid(FieldComponent comp) {
  auto it = observed_grids_.find(comp);
  if (it != observed_grids_.end())
    return &it->second;
  return nullptr;
}

const Grid *ObservationSpace::observed_grid(FieldComponent comp) const {
  if (background_trend_mode_ != 0) {
    auto ait = observed_anomaly_grids_.find(comp);
    if (ait != observed_anomaly_grids_.end())
      return &ait->second;
  }
  return raw_observed_grid(comp);
}

const Grid *ObservationSpace::raw_observed_grid(FieldComponent comp) const {
  auto it = observed_grids_.find(comp);
  if (it != observed_grids_.end())
    return &it->second;
  return nullptr;
}

double ObservationSpace::observed_mean(FieldComponent comp) const {
  auto it = observed_means_.find(comp);
  return it != observed_means_.end() ? it->second : 0.0;
}

TrendParameters ObservationSpace::observed_trend_parameters(FieldComponent comp) const {
  auto it = observed_trends_.find(comp);
  if (it != observed_trends_.end())
    return it->second;
  return TrendParameters{};
}

void ObservationSpace::set_background_trend_mode(int mode) {
  if (background_trend_mode_ != mode) {
    background_trend_mode_ = mode;
    update_virtual_observed_anomalies();
  }
}

void ObservationSpace::update_virtual_observed_anomalies() {
  observed_means_.clear();
  observed_trends_.clear();
  observed_anomaly_grids_.clear();

  for (const auto &[comp, grd] : observed_grids_) {
    if (grd.empty() || grd.rows() == 0 || grd.cols() == 0)
      continue;

    const double meanVal = grd.mean();
    observed_means_[comp] = meanVal;

    if (background_trend_mode_ == 0) {
      observed_trends_[comp] = TrendParameters{meanVal, 0.0, 0.0};
      continue;
    }

    if (background_trend_mode_ == 1) {
      // Mode 1: Constant shift c0 = mean
      Grid anomaly = grd;
      anomaly -= meanVal;
      observed_anomaly_grids_.emplace(comp, std::move(anomaly));
      observed_trends_[comp] = TrendParameters{meanVal, 0.0, 0.0};
    } else {
      // Mode 2 (Planar regional trend) or Mode 3 (Border-fitted regional plane)
      inv::GridGeometry geom;
      geom.nx = cols_;
      geom.ny = rows_;
      geom.dx = std::abs(dx_.value());
      geom.dy = std::abs(dy_.value());
      geom.x0 = x0_.value() - 0.5 * geom.dx;
      geom.y0 = y0_.value() - 0.5 * geom.dy;

      std::vector<double> v(cols_ * rows_, 0.0);
      for (size_t ix = 0; ix < cols_; ++ix) {
        for (size_t iy = 0; iy < rows_; ++iy) {
          double val = grd(iy, ix);
          v[ix * rows_ + iy] = (Grid::is_dummy_value(val) || std::isnan(val)) ? 0.0 : val;
        }
      }

      inv::BackgroundBasis bg(geom, background_trend_mode_);
      std::vector<double> bg_field;
      double c0 = 0.0, gx_km = 0.0, gy_km = 0.0;
      bg.evaluate(v, bg_field, c0, gx_km, gy_km);

      Grid anomaly(rows_, cols_, x0_, y0_, dx_, dy_, rotation_);
      for (size_t ix = 0; ix < cols_; ++ix) {
        for (size_t iy = 0; iy < rows_; ++iy) {
          if (grd.is_dummy(iy, ix)) {
            anomaly(iy, ix) = GRID_DUMMY;
          } else {
            anomaly(iy, ix) = grd(iy, ix) - bg_field[ix * rows_ + iy];
          }
        }
      }

      observed_anomaly_grids_.emplace(comp, std::move(anomaly));
      observed_trends_[comp] = TrendParameters{c0, gx_km, gy_km};
      observed_means_[comp] = c0;
    }
  }
}

void ObservationSpace::compute_voxel_grid_gravity(
    const inv::VoxelGrid<double> &grid, double zDomainTop, bool zeroFirst) {
  if (grid.total_cells() == 0 || rows_ == 0 || cols_ == 0) {
    return;
  }

  Grid *pGz = modeled_grid(FieldComponent::GZ);
  if (!pGz) return;

  if (zeroFirst) {
    pGz->fill(0.0);
  }

  const auto obsPoints = observation_points(FieldComponent::GZ);
  if (obsPoints.empty()) return;

  const size_t nx = grid.nx();
  const size_t ny = grid.ny();
  const double dx = grid.dx();
  const double dy = grid.dy();
  const double x0_geom = grid.x0();
  const double y0_geom = grid.y0();

  const bool hasRelief = !relief_grid_.empty() && relief_grid_.rows() >= 2 && relief_grid_.cols() >= 2;
  std::vector<double> topo_nodes;
  if (hasRelief) {
    topo_nodes.resize((nx + 1) * (ny + 1), 0.0);
    for (size_t iy = 0; iy <= ny; ++iy) {
      const double y_c = y0_geom + static_cast<double>(iy) * dy;
      for (size_t ix = 0; ix <= nx; ++ix) {
        const double x_c = x0_geom + static_cast<double>(ix) * dx;
        double elev = relief_grid_.sample(x_c, y_c);
        if (elev == GRID_DUMMY || std::isnan(elev)) {
          elev = relief_grid_.mean();
        }
        topo_nodes[iy * (nx + 1) + ix] = std::max(0.0, zDomainTop - elev);
      }
    }
  }

  std::vector<inv::Point3D<double>> inv_obs;
  inv_obs.reserve(obsPoints.size());
  const bool alignWithColumnTopo = hasRelief && (grv_mode_ == ObservationMode::SensorHeight) &&
                                   (rows_ == ny) && (cols_ == nx);
  const double hSens = (grv_height_.value() > 0.0) ? grv_height_.value() : 0.1;
  for (size_t idx = 0; idx < obsPoints.size(); ++idx) {
    const auto &pt = obsPoints[idx];
    double zd = Grid::is_dummy_value(pt.z) ? 0.0 : (zDomainTop - pt.z);
    if (alignWithColumnTopo && !Grid::is_dummy_value(pt.z)) {
      const size_t iy = idx / cols_;
      const size_t ix = idx % cols_;
      const double colSurf = 0.25 * (topo_nodes[iy * (nx + 1) + ix] +
                                     topo_nodes[iy * (nx + 1) + (ix + 1)] +
                                     topo_nodes[(iy + 1) * (nx + 1) + (ix + 1)] +
                                     topo_nodes[(iy + 1) * (nx + 1) + ix]);
      zd = colSurf - hSens;
    }
    inv_obs.emplace_back(pt.x, pt.y, zd);
  }

  std::vector<double> out_gz(inv_obs.size(), 0.0);
  if (hasRelief) {
    inv::ForwardKernel<double>::compute_gravity_field_interface_mesh(grid, topo_nodes, inv_obs, out_gz);
  } else {
    inv::ForwardKernel<double>::compute_gravity_field_interface_mesh(grid, {}, inv_obs, out_gz);
  }

  for (size_t r = 0; r < rows_; ++r) {
    for (size_t c = 0; c < cols_; ++c) {
      const size_t idx = r * cols_ + c;
      if (idx < obsPoints.size() && !Grid::is_dummy_value(obsPoints[idx].z) && !std::isnan(obsPoints[idx].z)) {
        const double val = out_gz[idx];
        if (zeroFirst) {
          (*pGz)(r, c) = val;
        } else {
          double &cell = (*pGz)(r, c);
          if (Grid::is_dummy_value(cell)) cell = val;
          else cell += val;
        }
      }
    }
  }
}

void ObservationSpace::set_modeled_grid(FieldComponent comp, const Grid &grid) {
  modeled_grids_[comp] = grid;
}

void ObservationSpace::set_observed_grid(FieldComponent comp,
                                         const Grid &grid) {
  observed_grids_[comp] = grid;
  update_virtual_observed_anomalies();
}

Grid *ObservationSpace::difference_grid(FieldComponent comp) {
  ensure_grid_allocated(difference_grids_, comp);
  return &difference_grids_[comp];
}

const Grid *ObservationSpace::difference_grid(FieldComponent comp) const {
  auto it = difference_grids_.find(comp);
  if (it != difference_grids_.end())
    return &it->second;
  return nullptr;
}

void ObservationSpace::set_difference_grid(FieldComponent comp,
                                           const Grid &grid) {
  difference_grids_[comp] = grid;
}

void ObservationSpace::compute_forward_field(
    const std::vector<Facet3Pt> &facets, bool zeroFirst) {
  if (rows_ == 0 || cols_ == 0)
    return;

  ensure_grid_allocated(modeled_grids_, FieldComponent::GZ);
  ensure_grid_allocated(modeled_grids_, FieldComponent::GX);
  ensure_grid_allocated(modeled_grids_, FieldComponent::GY);
  ensure_grid_allocated(modeled_grids_, FieldComponent::G_TOT);
  Grid &grdGz = modeled_grids_[FieldComponent::GZ];
  Grid &grdGx = modeled_grids_[FieldComponent::GX];
  Grid &grdGy = modeled_grids_[FieldComponent::GY];
  if (zeroFirst) {
    grdGx.zero();
    grdGy.zero();
    grdGz.zero();
  }

  ensure_grid_allocated(modeled_grids_, FieldComponent::MZ);
  ensure_grid_allocated(modeled_grids_, FieldComponent::MX);
  ensure_grid_allocated(modeled_grids_, FieldComponent::MY);
  ensure_grid_allocated(modeled_grids_, FieldComponent::DELTA_T);
  Grid &grdMz = modeled_grids_[FieldComponent::MZ];
  Grid &grdMx = modeled_grids_[FieldComponent::MX];
  Grid &grdMy = modeled_grids_[FieldComponent::MY];
  Grid &grdDeltaT = modeled_grids_[FieldComponent::DELTA_T];
  if (zeroFirst) {
    grdMx.zero();
    grdMy.zero();
    grdMz.zero();
    grdDeltaT.zero();
  }

  // Pre-compile active facet physical contrasts once outside the station loop
  struct ActiveFacetEntry {
    const Facet3Pt *fct{nullptr};
    double grvScale{0.0}; // (rho_primary - rho_opposite_or_ref) * dSign
    Point3D netMag{};     // (M_primary - M_opposite)
    bool hasGrv{false};
    bool hasMag{false};
  };

  std::vector<ActiveFacetEntry> activeFacets;
  activeFacets.reserve(facets.size());
  bool hasMagnetics = false;
  const double refDens = ref_density_.value();

  for (const auto &fct : facets) {
    if (fct.is_null() || !fct.pfld_facet().is_initialized())
      continue;

    double grvScale = 0.0;
    if (fct.primary) {
      const double bDens = fct.primary->density().value();
      const double oposDens =
          fct.opposite ? fct.opposite->density().value() : refDens;
      grvScale = (bDens - oposDens) * fct.dSign;
    }

    Point3D netMag(0.0, 0.0, 0.0);
    if (fct.primary && fct.primary->is_active()) {
      netMag += fct.primary->magnetization_vector();
    }
    if (fct.opposite && fct.opposite->is_active()) {
      netMag -= fct.opposite->magnetization_vector();
    }

    const bool hasGrv = std::abs(grvScale) > 1e-15;
    const bool hasMag = !netMag.is_zero();
    if (hasMag) {
      hasMagnetics = true;
    }
    if (hasGrv || hasMag) {
      activeFacets.push_back(
          ActiveFacetEntry{&fct, grvScale, netMag, hasGrv, hasMag});
    }
  }

  auto obsPointsGrv = observation_points(FieldComponent::GZ);
  std::vector<Point3D> obsPointsMag;
  if (hasMagnetics) {
    obsPointsMag = observation_points(FieldComponent::MZ);
  }

  const size_t nPts = obsPointsGrv.size();

  auto processStationRange = [&](size_t start_idx, size_t end_idx) {
    for (size_t p = start_idx; p < end_idx; ++p) {
      Point3D totalG(0.0, 0.0, 0.0);
      Point3D totalM(0.0, 0.0, 0.0);

      bool validGrv = !Grid::is_dummy_value(obsPointsGrv[p].z);
      bool validMag = hasMagnetics && p < obsPointsMag.size() &&
                      !Grid::is_dummy_value(obsPointsMag[p].z);

      Point3D rMag(0.0, 0.0, 0.0);
      if (validMag) {
        rMag = Point3D(obsPointsMag[p].x + 0.001, obsPointsMag[p].y + 0.003,
                       obsPointsMag[p].z);
      }

      if (validGrv || validMag) {
        for (const auto &entry : activeFacets) {
          const Facet3Pt &fct = *entry.fct;
          if (validGrv && entry.hasGrv) {
            Point3D g(0.0, 0.0, 0.0);
            fct.fld_g(obsPointsGrv[p], g);
            totalG += g * entry.grvScale;
          }

          if (validMag && entry.hasMag) {
            Point3D fMag(0.0, 0.0, 0.0);
            fct.fld_gs(rMag, entry.netMag, fMag, 1.0);
            totalM -= fMag * fct.dSign;
          }
        }
      }

      size_t r = p / cols_;
      size_t c = p % cols_;
      Point3D scaledG = (grv_field_unit_ == GravityFieldUnit::MilliGal)
                            ? Point3D(units::SI_Acceleration(totalG.x)
                                          .as<units::tag::MilliGal>()
                                          .value(),
                                      units::SI_Acceleration(totalG.y)
                                          .as<units::tag::MilliGal>()
                                          .value(),
                                      units::SI_Acceleration(totalG.z)
                                          .as<units::tag::MilliGal>()
                                          .value())
                            : Point3D(units::SI_Acceleration(totalG.x)
                                          .as<units::tag::GravityUnit>()
                                          .value(),
                                      units::SI_Acceleration(totalG.y)
                                          .as<units::tag::GravityUnit>()
                                          .value(),
                                      units::SI_Acceleration(totalG.z)
                                          .as<units::tag::GravityUnit>()
                                          .value());

      if (zeroFirst) {
        if (validGrv) {
          grdGx(r, c) = scaledG.x;
          grdGy(r, c) = scaledG.y;
          grdGz(r, c) = scaledG.z;
        } else {
          grdGx(r, c) = GRID_DUMMY;
          grdGy(r, c) = GRID_DUMMY;
          grdGz(r, c) = GRID_DUMMY;
        }

        if (validMag) {
          grdMx(r, c) = totalM.x;
          grdMy(r, c) = totalM.y;
          grdMz(r, c) = totalM.z;
        } else if (hasMagnetics) {
          grdMx(r, c) = GRID_DUMMY;
          grdMy(r, c) = GRID_DUMMY;
          grdMz(r, c) = GRID_DUMMY;
        }
      } else {
        if (validGrv) {
          if (!grdGx.is_dummy(r, c))
            grdGx(r, c) += scaledG.x;
          if (!grdGy.is_dummy(r, c))
            grdGy(r, c) += scaledG.y;
          if (!grdGz.is_dummy(r, c))
            grdGz(r, c) += scaledG.z;
        }
        if (validMag) {
          if (!grdMx.is_dummy(r, c))
            grdMx(r, c) += totalM.x;
          if (!grdMy.is_dummy(r, c))
            grdMy(r, c) += totalM.y;
          if (!grdMz.is_dummy(r, c))
            grdMz(r, c) += totalM.z;
        }
      }
    }
  };

  const auto [threads_count] = pfld::detail::calculate_partition(nPts, 32);
  if (threads_count <= 1) {
    processStationRange(0, nPts);
  } else {
    const size_t chunk_size = (nPts + threads_count - 1) / threads_count;
    std::vector<pfld::detail::jthread_type> threads;
    threads.reserve(threads_count);
    for (size_t i = 0; i < threads_count; ++i) {
      const size_t start_idx = i * chunk_size;
      const size_t end_idx = std::min(start_idx + chunk_size, nPts);
      if (start_idx >= end_idx) break;
      threads.emplace_back([&processStationRange, start_idx, end_idx]() {
        processStationRange(start_idx, end_idx);
      });
    }
  }

  compute_total_fields();
}

void ObservationSpace::update_delta_field(
    const std::vector<Facet3Pt> &deltaFacets) {
  if (deltaFacets.empty())
    return;
  compute_forward_field(deltaFacets, false);
}

void ObservationSpace::compute_total_fields() {
  Grid *pGx = modeled_grid(FieldComponent::GX);
  Grid *pGy = modeled_grid(FieldComponent::GY);
  Grid *pGz = modeled_grid(FieldComponent::GZ);
  Grid *pGTot = modeled_grid(FieldComponent::G_TOT);

  if (pGx && pGy && pGz && pGTot) {
    for (size_t r = 0; r < rows_; ++r) {
      for (size_t c = 0; c < cols_; ++c) {
        double gx = (*pGx)(r, c);
        double gy = (*pGy)(r, c);
        double gz = (*pGz)(r, c);
        if (pGx->is_dummy(r, c) || pGy->is_dummy(r, c) || pGz->is_dummy(r, c)) {
          (*pGTot)(r, c) = GRID_DUMMY;
        } else {
          (*pGTot)(r, c) = std::sqrt(gx * gx + gy * gy + gz * gz);
        }
      }
    }
  }

  Grid *pMx = modeled_grid(FieldComponent::MX);
  Grid *pMy = modeled_grid(FieldComponent::MY);
  Grid *pMz = modeled_grid(FieldComponent::MZ);
  Grid *pDT = modeled_grid(FieldComponent::DELTA_T);

  if (pMx && pMy && pMz && pDT) {
    Point3D af = ambient_field_vector();
    for (size_t r = 0; r < rows_; ++r) {
      for (size_t c = 0; c < cols_; ++c) {
        double mx = (*pMx)(r, c);
        double my = (*pMy)(r, c);
        double mz = (*pMz)(r, c);
        if (pMx->is_dummy(r, c) || pMy->is_dummy(r, c) || pMz->is_dummy(r, c)) {
          (*pDT)(r, c) = GRID_DUMMY;
        } else {
          (*pDT)(r, c) = -mag_delta_t(mx, my, mz, af.x, af.y, af.z,
                                      mag_intensity_.value());
        }
      }
    }
  }
}

void ObservationSpace::compute_difference(FieldComponent comp,
                                          bool removeMean) {
  Grid *pMod = modeled_grid(comp);
  const Grid *pObs = std::as_const(*this).observed_grid(comp);
  if ((!pObs || pObs->empty()) && comp == FieldComponent::GZ) {
    pObs = std::as_const(*this).observed_grid(FieldComponent::G_TOT);
  } else if ((!pObs || pObs->empty()) && comp == FieldComponent::G_TOT) {
    pObs = std::as_const(*this).observed_grid(FieldComponent::GZ);
  } else if ((!pObs || pObs->empty()) && comp == FieldComponent::DELTA_T) {
    pObs = std::as_const(*this).observed_grid(FieldComponent::MZ);
  } else if ((!pObs || pObs->empty()) && comp == FieldComponent::MZ) {
    pObs = std::as_const(*this).observed_grid(FieldComponent::DELTA_T);
  }
  Grid *pDif = difference_grid(comp);

  if (!pMod || !pObs || !pDif)
    return;

  for (size_t r = 0; r < rows_; ++r) {
    for (size_t c = 0; c < cols_; ++c) {
      if (pMod->is_dummy(r, c) || pObs->is_dummy(r, c)) {
        (*pDif)(r, c) = GRID_DUMMY;
      } else {
        (*pDif)(r, c) = (*pMod)(r, c) - (*pObs)(r, c);
      }
    }
  }

  if (removeMean) {
    double meanVal = pDif->mean();
    *pMod -= meanVal;
    for (size_t r = 0; r < rows_; ++r) {
      for (size_t c = 0; c < cols_; ++c) {
        if (!pDif->is_dummy(r, c)) {
          (*pDif)(r, c) -= meanVal;
        }
      }
    }
  }
}

FitResult ObservationSpace::fit_density(Body *pBody,
                                        const std::vector<Facet3Pt> &bodyFacets,
                                        FieldComponent comp,
                                        double initialDensity, double tol,
                                        FitMethod method) {
  if (!pBody || bodyFacets.empty()) {
    return FitResult{};
  }

  const Grid *pObs = std::as_const(*this).observed_grid(comp);
  if (!pObs || pObs->empty()) {
    return FitResult{};
  }

  // 1. Pre-classify facets into unit response for target body `pBody` and
  // fixed background contribution from any other bodies in `bodyFacets`.
  // Supports both per-body facet lists and full model computation facet lists
  // where `pBody` may appear as `primary` (+1) or `opposite` (-1).
  auto obsPoints = observation_points(comp);
  Grid grdUnit(rows_, cols_, x0_, y0_, dx_, dy_, rotation_);
  Grid grdBg(rows_, cols_, x0_, y0_, dx_, dy_, rotation_);
  grdUnit.zero();
  grdBg.zero();

  const double refDens = ref_density_.value();
  struct DensityFitFacet {
    const Facet3Pt *fct{nullptr};
    double unitScale{0.0};
    double bgScale{0.0};
  };
  std::vector<DensityFitFacet> fitFacets;
  fitFacets.reserve(bodyFacets.size());

  for (const auto &fct : bodyFacets) {
    if (fct.is_null() || !fct.pfld_facet().is_initialized())
      continue;
    double unitScale = 0.0;
    double bgScale = 0.0;

    if (fct.primary == pBody) {
      unitScale += fct.dSign;
    } else if (fct.primary != nullptr) {
      bgScale += (fct.primary->density().value() - refDens) * fct.dSign;
    }

    if (fct.opposite == pBody) {
      unitScale -= fct.dSign;
    } else if (fct.opposite != nullptr) {
      bgScale -= (fct.opposite->density().value() - refDens) * fct.dSign;
    }

    // Fallback if caller passed unlinked facets with primary == nullptr
    if (fct.primary == nullptr && fct.opposite == nullptr) {
      unitScale = fct.dSign;
    }

    if (std::abs(unitScale) > 1e-15 || std::abs(bgScale) > 1e-15) {
      fitFacets.push_back({&fct, unitScale, bgScale});
    }
  }

  auto toUnit = [&](double valSI) -> double {
    return (grv_field_unit_ == GravityFieldUnit::MilliGal)
               ? units::SI_Acceleration(valSI).as<units::tag::MilliGal>().value()
               : units::SI_Acceleration(valSI)
                     .as<units::tag::GravityUnit>()
                     .value();
  };

  const size_t nPts = obsPoints.size();
  auto processDensityStationRange = [&](size_t start_p, size_t end_p) {
    for (size_t p = start_p; p < end_p; ++p) {
      size_t r = p / cols_;
      size_t c = p % cols_;
      if (Grid::is_dummy_value(obsPoints[p].z)) {
        grdUnit(r, c) = GRID_DUMMY;
        grdBg(r, c) = GRID_DUMMY;
        continue;
      }
      double unitG = 0.0;
      double bgG = 0.0;
      for (const auto &entry : fitFacets) {
        Point3D g(0, 0, 0);
        entry.fct->fld_g(obsPoints[p], g);
        double compG = g.z;
        if (comp == FieldComponent::GX)
          compG = g.x;
        else if (comp == FieldComponent::GY)
          compG = g.y;
        unitG += compG * entry.unitScale;
        bgG += compG * entry.bgScale;
      }
      grdUnit(r, c) = toUnit(unitG);
      grdBg(r, c) = toUnit(bgG);
    }
  };

  const auto [threads_count] = pfld::detail::calculate_partition(nPts, 32);
  if (threads_count <= 1) {
    processDensityStationRange(0, nPts);
  } else {
    const size_t chunk_size = (nPts + threads_count - 1) / threads_count;
    std::vector<pfld::detail::jthread_type> threads;
    threads.reserve(threads_count);
    for (size_t i = 0; i < threads_count; ++i) {
      const size_t start_p = i * chunk_size;
      const size_t end_p = std::min(start_p + chunk_size, nPts);
      if (start_p >= end_p) break;
      threads.emplace_back([&processDensityStationRange, start_p, end_p]() {
        processDensityStationRange(start_p, end_p);
      });
    }
  }

  // Exact analytical linear least-squares solution for linear density contrast
  if (method == FitMethod::linear || method == FitMethod::Linear) {
    auto start_time = std::chrono::high_resolution_clock::now();
    double sumUnitSq = 0.0;
    double sumUnitDiff = 0.0;
    size_t count = 0;

    for (size_t r = 0; r < rows_; ++r) {
      for (size_t c = 0; c < cols_; ++c) {
        if (!pObs->is_dummy(r, c) && !grdUnit.is_dummy(r, c)) {
          double u = grdUnit(r, c);
          double d = (*pObs)(r, c) - grdBg(r, c);
          sumUnitSq += u * u;
          sumUnitDiff += u * d;
          count++;
        }
      }
    }

    double optimalDiffDensity =
        (sumUnitSq > 1e-20) ? (sumUnitDiff / sumUnitSq) : (initialDensity - refDens);
    double optimalDensity = optimalDiffDensity + refDens;

    double sumSq = 0.0;
    for (size_t r = 0; r < rows_; ++r) {
      for (size_t c = 0; c < cols_; ++c) {
        if (!pObs->is_dummy(r, c) && !grdUnit.is_dummy(r, c)) {
          double calcVal = grdBg(r, c) + optimalDiffDensity * grdUnit(r, c);
          double diff = calcVal - (*pObs)(r, c);
          sumSq += diff * diff;
        }
      }
    }
    double finalRms = (count > 0) ? std::sqrt(sumSq / count) : 0.0;

    auto finish_time = std::chrono::high_resolution_clock::now();
    double elapsed_sec =
        std::chrono::duration<double>(finish_time - start_time).count();

    FitResult res;
    res.optimal_parameter = optimalDensity;
    res.minimum_objective = finalRms;
    res.iterations = 1;
    res.converged = true;
    res.elapsed_time_sec = elapsed_sec;
    res.history.push_back({1, optimalDensity, finalRms, 0.0, finalRms, 0.0});

    std::ostringstream log;
    log << "Density inversion (Linear Least Squares)\n\n"
        << "Direct analytical solution:\n"
        << "Optimal density: " << std::setprecision(6) << optimalDensity << "\n"
        << "RMS residual:    " << std::setprecision(10) << finalRms << "\n"
        << "Fitting elapsed time: " << elapsed_sec << "s\n";
    res.log = log.str();

    pBody->set_density(optimalDensity);
    return res;
  }

  // Objective function for differential density x = (dens - ref_density_)
  auto objFunc = [&](double x) -> double {
    double sumSq = 0.0;
    size_t count = 0;

    for (size_t r = 0; r < rows_; ++r) {
      for (size_t c = 0; c < cols_; ++c) {
        if (!pObs->is_dummy(r, c) && !grdUnit.is_dummy(r, c)) {
          double calcVal = grdBg(r, c) + x * grdUnit(r, c);
          double diff = calcVal - (*pObs)(r, c);
          sumSq += diff * diff;
          count++;
        }
      }
    }
    return count > 0 ? std::sqrt(sumSq / count) : 0.0;
  };

  double dens0 = initialDensity - ref_density_.value();
  double startA = dens0;
  double startB = dens0 + 10.0;

  FitResult res = Inversion1D::optimize(startA, startB, objFunc, method, tol,
                                        100, "Density inversion");

  double optimalDiffDensity = res.optimal_parameter;
  double optimalDensity = optimalDiffDensity + ref_density_.value();
  pBody->set_density(optimalDensity);
  res.optimal_parameter = optimalDensity;

  return res;
}

FitResult ObservationSpace::fit_vertex(Model &model, int row, int col,
                                       int index, FieldComponent comp,
                                       double tol, FitMethod method,
                                       bool use_superposition) {
  const Grid *pObs = std::as_const(*this).observed_grid(comp);
  if (!pObs || pObs->empty()) {
    return FitResult{};
  }
  if (index <= 0 || index >= static_cast<int>(model.count(row, col)) - 1) {
    return FitResult{};
  }

  double originalZ = model.z(row, col, index);
  double zU = (index > 0) ? model.z(row, col, index - 1) : model.z_max_val();
  double zD = (index + 1 < static_cast<int>(model.count(row, col)))
                  ? model.z(row, col, index + 1)
                  : model.z_min_val();

  std::vector<Facet3Pt> localFacets;
  localFacets.reserve(64);
  auto gatherLocalFacets = [&]() {
    localFacets.clear();
    for (int dr = -1; dr <= 0; ++dr) {
      for (int dc = -1; dc <= 0; ++dc) {
        if (const auto *lst = model.facet_list(row + dr, col + dc)) {
          for (const auto &fct : *lst) {
            Facet3Pt copy = fct;
            copy.set_sign(1.0);
            localFacets.push_back(copy);
          }
        }
      }
    }
  };

  Grid baseGx, baseGy, baseGz, baseMx, baseMy, baseMz;

  if (use_superposition && !modeled_grids_.empty() &&
      modeled_grid(comp) != nullptr && !modeled_grid(comp)->empty()) {
    // Fast superposition: G_static = G_full - G_local(originalZ)
    Grid fullGx = *modeled_grid(FieldComponent::GX);
    Grid fullGy = *modeled_grid(FieldComponent::GY);
    Grid fullGz = *modeled_grid(FieldComponent::GZ);
    Grid fullMx = *modeled_grid(FieldComponent::MX);
    Grid fullMy = *modeled_grid(FieldComponent::MY);
    Grid fullMz = *modeled_grid(FieldComponent::MZ);

    gatherLocalFacets();
    compute_forward_field(localFacets, true);

    baseGx = fullGx - *modeled_grid(FieldComponent::GX);
    baseGy = fullGy - *modeled_grid(FieldComponent::GY);
    baseGz = fullGz - *modeled_grid(FieldComponent::GZ);
    baseMx = fullMx - *modeled_grid(FieldComponent::MX);
    baseMy = fullMy - *modeled_grid(FieldComponent::MY);
    baseMz = fullMz - *modeled_grid(FieldComponent::MZ);
  } else {
    // Precompute the static background field from all facet cells EXCEPT the 4
    // cells adjacent to (row, col): (row-1, col-1), (row-1, col), (row, col-1), (row, col).
    auto isLocalCell = [&](int r, int c) noexcept {
      return (r == row - 1 || r == row) && (c == col - 1 || c == col);
    };

    std::vector<Facet3Pt> staticFacets;
    for (int r = 0; r < model.rows() - 1; ++r) {
      for (int c = 0; c < model.cols() - 1; ++c) {
        if (isLocalCell(r, c))
          continue;
        if (const auto *lst = model.facet_list(r, c)) {
          for (const auto &fct : *lst) {
            Facet3Pt copy = fct;
            copy.set_sign(1.0);
            staticFacets.push_back(copy);
          }
        }
      }
    }

    compute_forward_field(staticFacets, true);
    baseGx = *modeled_grid(FieldComponent::GX);
    baseGy = *modeled_grid(FieldComponent::GY);
    baseGz = *modeled_grid(FieldComponent::GZ);
    baseMx = *modeled_grid(FieldComponent::MX);
    baseMy = *modeled_grid(FieldComponent::MY);
    baseMz = *modeled_grid(FieldComponent::MZ);
  }

  auto toUnit = [&](double valSI) -> double {
    return (grv_field_unit_ == GravityFieldUnit::MilliGal)
               ? units::SI_Acceleration(valSI).as<units::tag::MilliGal>().value()
               : units::SI_Acceleration(valSI)
                     .as<units::tag::GravityUnit>()
                     .value();
  };

  auto obsPointsGrv = observation_points(FieldComponent::GZ);
  const size_t nPts = obsPointsGrv.size();
  const double refDens = ref_density_.value();
  Point3D ambientFld = ambient_field_vector();
  double magInt = mag_intensity_.value();

  struct LocalActiveEntry {
    const Facet3Pt *fct{nullptr};
    double grvScale{0.0};
    Point3D netMag{};
    bool hasGrv{false};
    bool hasMag{false};
  };
  std::vector<LocalActiveEntry> localActive;
  localActive.reserve(64);

  bool isGravComp = is_gravity_component(comp);
  bool isMagComp = is_magnetic_component(comp);

  // Fast zero-copy, single-component, parallel RMS objective function
  auto objFunc = [&](double zVal) -> double {
    if (zVal <= zD + 1e-6 || zVal >= zU - 1e-6) {
      return 1e9;
    }
    int vIdx = index;
    int moveRes =
        model.move_vertex(vIdx, row, col, zVal, BodyMoveType::Constrained);
    if (moveRes != 0) {
      return 1e9;
    }

    gatherLocalFacets();
    localActive.clear();

    for (const auto &fct : localFacets) {
      if (fct.is_null() || !fct.pfld_facet().is_initialized())
        continue;

      double grvScale = 0.0;
      if (fct.primary) {
        const double bDens = fct.primary->density().value();
        const double oposDens =
            fct.opposite ? fct.opposite->density().value() : refDens;
        grvScale = (bDens - oposDens) * fct.dSign;
      }

      Point3D netMag(0.0, 0.0, 0.0);
      if (isMagComp) {
        if (fct.primary && fct.primary->is_active()) {
          netMag += fct.primary->magnetization_vector();
        }
        if (fct.opposite && fct.opposite->is_active()) {
          netMag -= fct.opposite->magnetization_vector();
        }
      }

      const bool hasGrv = isGravComp && std::abs(grvScale) > 1e-15;
      const bool hasM = isMagComp && !netMag.is_zero();
      if (hasGrv || hasM) {
        localActive.push_back({&fct, grvScale, netMag, hasGrv, hasM});
      }
    }

    auto evaluateRange = [&](size_t start_idx, size_t end_idx,
                             double &outSumSq, size_t &outCount) {
      double sumSqLocal = 0.0;
      size_t countLocal = 0;

      for (size_t p = start_idx; p < end_idx; ++p) {
        size_t r = p / cols_;
        size_t c = p % cols_;
        double obsVal = (*pObs)(r, c);
        if (Grid::is_dummy_value(obsVal) ||
            Grid::is_dummy_value(obsPointsGrv[p].z)) {
          continue;
        }

        double modeledVal = 0.0;

        if (comp == FieldComponent::GZ) {
          if (baseGz.is_dummy(r, c))
            continue;
          double localGz = 0.0;
          for (const auto &entry : localActive) {
            Point3D g(0, 0, 0);
            entry.fct->fld_g(obsPointsGrv[p], g);
            localGz += g.z * entry.grvScale;
          }
          modeledVal = baseGz(r, c) + toUnit(localGz);
        } else if (comp == FieldComponent::GX) {
          if (baseGx.is_dummy(r, c))
            continue;
          double localGx = 0.0;
          for (const auto &entry : localActive) {
            Point3D g(0, 0, 0);
            entry.fct->fld_g(obsPointsGrv[p], g);
            localGx += g.x * entry.grvScale;
          }
          modeledVal = baseGx(r, c) + toUnit(localGx);
        } else if (comp == FieldComponent::GY) {
          if (baseGy.is_dummy(r, c))
            continue;
          double localGy = 0.0;
          for (const auto &entry : localActive) {
            Point3D g(0, 0, 0);
            entry.fct->fld_g(obsPointsGrv[p], g);
            localGy += g.y * entry.grvScale;
          }
          modeledVal = baseGy(r, c) + toUnit(localGy);
        } else if (comp == FieldComponent::G_TOT) {
          if (baseGx.is_dummy(r, c) || baseGy.is_dummy(r, c) ||
              baseGz.is_dummy(r, c))
            continue;
          Point3D localG(0, 0, 0);
          for (const auto &entry : localActive) {
            Point3D g(0, 0, 0);
            entry.fct->fld_g(obsPointsGrv[p], g);
            localG += g * entry.grvScale;
          }
          double gx = baseGx(r, c) + toUnit(localG.x);
          double gy = baseGy(r, c) + toUnit(localG.y);
          double gz = baseGz(r, c) + toUnit(localG.z);
          modeledVal = std::sqrt(gx * gx + gy * gy + gz * gz);
        } else if (comp == FieldComponent::DELTA_T) {
          if (baseMx.is_dummy(r, c) || baseMy.is_dummy(r, c) ||
              baseMz.is_dummy(r, c))
            continue;
          Point3D rMag(obsPointsGrv[p].x + 0.001, obsPointsGrv[p].y + 0.003,
                       obsPointsGrv[p].z);
          Point3D localM(0, 0, 0);
          for (const auto &entry : localActive) {
            Point3D fMag(0, 0, 0);
            entry.fct->fld_gs(rMag, entry.netMag, fMag, 1.0);
            localM -= fMag * entry.fct->dSign;
          }
          double mx = baseMx(r, c) + localM.x;
          double my = baseMy(r, c) + localM.y;
          double mz = baseMz(r, c) + localM.z;
          modeledVal = -mag_delta_t(mx, my, mz, ambientFld.x, ambientFld.y,
                                    ambientFld.z, magInt);
        } else if (comp == FieldComponent::MX) {
          if (baseMx.is_dummy(r, c))
            continue;
          Point3D rMag(obsPointsGrv[p].x + 0.001, obsPointsGrv[p].y + 0.003,
                       obsPointsGrv[p].z);
          Point3D localM(0, 0, 0);
          for (const auto &entry : localActive) {
            Point3D fMag(0, 0, 0);
            entry.fct->fld_gs(rMag, entry.netMag, fMag, 1.0);
            localM -= fMag * entry.fct->dSign;
          }
          modeledVal = baseMx(r, c) + localM.x;
        } else if (comp == FieldComponent::MY) {
          if (baseMy.is_dummy(r, c))
            continue;
          Point3D rMag(obsPointsGrv[p].x + 0.001, obsPointsGrv[p].y + 0.003,
                       obsPointsGrv[p].z);
          Point3D localM(0, 0, 0);
          for (const auto &entry : localActive) {
            Point3D fMag(0, 0, 0);
            entry.fct->fld_gs(rMag, entry.netMag, fMag, 1.0);
            localM -= fMag * entry.fct->dSign;
          }
          modeledVal = baseMy(r, c) + localM.y;
        } else if (comp == FieldComponent::MZ) {
          if (baseMz.is_dummy(r, c))
            continue;
          Point3D rMag(obsPointsGrv[p].x + 0.001, obsPointsGrv[p].y + 0.003,
                       obsPointsGrv[p].z);
          Point3D localM(0, 0, 0);
          for (const auto &entry : localActive) {
            Point3D fMag(0, 0, 0);
            entry.fct->fld_gs(rMag, entry.netMag, fMag, 1.0);
            localM -= fMag * entry.fct->dSign;
          }
          modeledVal = baseMz(r, c) + localM.z;
        }

        double diff = modeledVal - obsVal;
        sumSqLocal += diff * diff;
        countLocal++;
      }
      outSumSq = sumSqLocal;
      outCount = countLocal;
    };

    const auto [threads_count] = pfld::detail::calculate_partition(nPts, 64);
    double totalSumSq = 0.0;
    size_t totalCount = 0;

    if (threads_count <= 1) {
      evaluateRange(0, nPts, totalSumSq, totalCount);
    } else {
      std::vector<double> threadSumSq(threads_count, 0.0);
      std::vector<size_t> threadCount(threads_count, 0);
      const size_t chunk_size = (nPts + threads_count - 1) / threads_count;
      {
        std::vector<pfld::detail::jthread_type> threads;
        threads.reserve(threads_count);
        for (size_t t = 0; t < threads_count; ++t) {
          const size_t start_idx = t * chunk_size;
          const size_t end_idx = std::min(start_idx + chunk_size, nPts);
          if (start_idx >= end_idx)
            break;
          threads.emplace_back([&, t, start_idx, end_idx]() {
            evaluateRange(start_idx, end_idx, threadSumSq[t], threadCount[t]);
          });
        }
      }
      for (size_t t = 0; t < threads_count; ++t) {
        totalSumSq += threadSumSq[t];
        totalCount += threadCount[t];
      }
    }

    return totalCount > 0 ? std::sqrt(totalSumSq / totalCount) : 0.0;
  };

  const double initialRms = objFunc(originalZ);

  double eps = 0.01 * std::min(zU - originalZ, originalZ - zD);
  if (eps <= 0.01)
    eps = 0.5;

  double boundMin = zD + eps;
  double boundMax = zU - eps;
  if (boundMin >= boundMax) {
    boundMin = zD + 1e-4;
    boundMax = zU - 1e-4;
  }
  if (boundMin > boundMax) {
    std::swap(boundMin, boundMax);
  }

  FitResult res = Inversion1D::optimize_bounded(
      boundMin, boundMax, objFunc, originalZ, method, tol, 100,
      "Vertex inversion");

  double finalZ = res.optimal_parameter;
  if (!std::isfinite(finalZ) || finalZ <= zD + 1e-6 || finalZ >= zU - 1e-6 ||
      res.minimum_objective > initialRms) {
    finalZ = originalZ;
    res.optimal_parameter = originalZ;
    res.minimum_objective = initialRms;
  }

  int vIdx = index;
  model.move_vertex(vIdx, row, col, finalZ, BodyMoveType::Constrained);

  modeled_grids_[FieldComponent::GX] = baseGx;
  modeled_grids_[FieldComponent::GY] = baseGy;
  modeled_grids_[FieldComponent::GZ] = baseGz;
  modeled_grids_[FieldComponent::MX] = baseMx;
  modeled_grids_[FieldComponent::MY] = baseMy;
  modeled_grids_[FieldComponent::MZ] = baseMz;
  gatherLocalFacets();
  compute_forward_field(localFacets, false);

  return res;
}

static bool solve_linear_system(int n, std::vector<double> &A,
                                std::vector<double> &b,
                                std::vector<double> &x) {
  x = b;
  for (int i = 0; i < n; ++i) {
    int pivot = i;
    double maxVal = std::abs(A[i * n + i]);
    for (int r = i + 1; r < n; ++r) {
      double val = std::abs(A[r * n + i]);
      if (val > maxVal) {
        maxVal = val;
        pivot = r;
      }
    }
    if (maxVal < 1e-18) {
      return false;
    }
    if (pivot != i) {
      for (int c = i; c < n; ++c) {
        std::swap(A[i * n + c], A[pivot * n + c]);
      }
      std::swap(x[i], x[pivot]);
    }
    double diag = A[i * n + i];
    for (int r = i + 1; r < n; ++r) {
      double factor = A[r * n + i] / diag;
      for (int c = i; c < n; ++c) {
        A[r * n + c] -= factor * A[i * n + c];
      }
      x[r] -= factor * x[i];
    }
  }
  for (int i = n - 1; i >= 0; --i) {
    double sum = x[i];
    for (int c = i + 1; c < n; ++c) {
      sum -= A[i * n + c] * x[c];
    }
    x[i] = sum / A[i * n + i];
  }
  return true;
}

NeighborhoodFitResult ObservationSpace::fit_neighborhood(
    Model &model, int row, int col, int index, FieldComponent comp,
    const NeighborhoodFitSettings &settings) {
  auto start_time = std::chrono::high_resolution_clock::now();
  NeighborhoodFitResult result;

  const Grid *pObs = std::as_const(*this).observed_grid(comp);
  if (!pObs || pObs->empty()) {
    result.log = "Error: No observed grid data available for fitting.";
    return result;
  }

  if (row < 0 || row >= static_cast<int>(model.rows()) ||
      col < 0 || col >= static_cast<int>(model.cols())) {
    result.log = "Error: Invalid seed vertex row/col coordinate.";
    return result;
  }

  const int countSeed = static_cast<int>(model.count(row, col));
  if (index <= 0 || index >= countSeed - 1) {
    result.log = "Error: Seed vertex is a boundary point and cannot be inverted.";
    return result;
  }

  const auto *seedPt = model.at(row, col, index);
  if (!seedPt) {
    result.log = "Error: Seed point does not exist.";
    return result;
  }
  const int targetBodyId = seedPt->body_id();
  const double seedZ = model.z(row, col, index);
  const double seedX = model.xe_val(col);
  const double seedY = model.ye_val(row);
  const double modelDx = model.dx_val();
  const double modelDy = model.dy_val();
  const double stepSpacing = std::min(modelDx, modelDy);

  auto toUnit = [&](double valSI) -> double {
    return (grv_field_unit_ == GravityFieldUnit::MilliGal)
               ? units::SI_Acceleration(valSI).as<units::tag::MilliGal>().value()
               : units::SI_Acceleration(valSI)
                     .as<units::tag::GravityUnit>()
                     .value();
  };

  const auto obsPointsGrv = observation_points(FieldComponent::GZ);
  const size_t nPts = obsPointsGrv.size();
  const double refDens = ref_density_.value();
  const Point3D ambientFld = ambient_field_vector();
  const double magInt = mag_intensity_.value();
  const bool isGravComp = is_gravity_component(comp);
  const bool isMagComp = is_magnetic_component(comp);

  struct ActiveFacet {
    Facet3Pt fct{};
    double grvScale{0.0};
    Point3D netMag{};
    bool hasGrv{false};
    bool hasMag{false};
  };

  auto extractActiveFacets = [&](const std::vector<std::pair<int, int>> &cells,
                                 std::vector<ActiveFacet> &outActive) {
    outActive.clear();
    for (const auto &[cr, cc] : cells) {
      const auto *lst = model.facet_list(cr, cc);
      if (!lst) continue;
      for (const auto &fct : *lst) {
        if (fct.is_null() || !fct.pfld_facet().is_initialized())
          continue;
        double grvScale = 0.0;
        if (fct.primary) {
          const double bDens = fct.primary->density().value();
          const double oposDens =
              fct.opposite ? fct.opposite->density().value() : refDens;
          grvScale = (bDens - oposDens) * fct.dSign;
        }
        Point3D netMag(0.0, 0.0, 0.0);
        if (isMagComp) {
          if (fct.primary && fct.primary->is_active()) {
            netMag += fct.primary->magnetization_vector();
          }
          if (fct.opposite && fct.opposite->is_active()) {
            netMag -= fct.opposite->magnetization_vector();
          }
        }
        const bool hasGrv = isGravComp && std::abs(grvScale) > 1e-15;
        const bool hasM = isMagComp && !netMag.is_zero();
        if (hasGrv || hasM) {
          outActive.push_back({fct, grvScale, netMag, hasGrv, hasM});
        }
      }
    }
  };

  auto evaluateStationField = [&](size_t p,
                                  const std::vector<ActiveFacet> &activeFacets) -> double {
    if (Grid::is_dummy_value(obsPointsGrv[p].z)) return 0.0;
    if (comp == FieldComponent::GZ) {
      double gz = 0.0;
      for (const auto &entry : activeFacets) {
        Point3D g(0, 0, 0);
        entry.fct.fld_g(obsPointsGrv[p], g);
        gz += g.z * entry.grvScale;
      }
      return toUnit(gz);
    } else if (comp == FieldComponent::GX) {
      double gx = 0.0;
      for (const auto &entry : activeFacets) {
        Point3D g(0, 0, 0);
        entry.fct.fld_g(obsPointsGrv[p], g);
        gx += g.x * entry.grvScale;
      }
      return toUnit(gx);
    } else if (comp == FieldComponent::GY) {
      double gy = 0.0;
      for (const auto &entry : activeFacets) {
        Point3D g(0, 0, 0);
        entry.fct.fld_g(obsPointsGrv[p], g);
        gy += g.y * entry.grvScale;
      }
      return toUnit(gy);
    } else if (comp == FieldComponent::DELTA_T) {
      Point3D rMag(obsPointsGrv[p].x + 0.001, obsPointsGrv[p].y + 0.003,
                   obsPointsGrv[p].z);
      Point3D localM(0, 0, 0);
      for (const auto &entry : activeFacets) {
        Point3D fMag(0, 0, 0);
        entry.fct.fld_gs(rMag, entry.netMag, fMag, 1.0);
        localM -= fMag * entry.fct.dSign;
      }
      return -mag_delta_t(localM.x, localM.y, localM.z, ambientFld.x,
                          ambientFld.y, ambientFld.z, magInt);
    }
    return 0.0;
  };

  // 1. Determine Neighborhood Radius (Automatic or Custom)
  double chosenRadius = 0.0;
  if (settings.radius_mode == NeighborhoodRadiusMode::Custom) {
    chosenRadius = std::max(0.0, settings.custom_radius_m);
  } else {
    // Measure peak anomaly response of seed vertex under 1.0m perturbation
    std::vector<std::pair<int, int>> seedCells;
    for (int dr = -1; dr <= 0; ++dr) {
      for (int dc = -1; dc <= 0; ++dc) {
        int cr = row + dr;
        int cc = col + dc;
        if (cr >= 0 && cr < static_cast<int>(model.rows()) - 1 &&
            cc >= 0 && cc < static_cast<int>(model.cols()) - 1) {
          seedCells.push_back({cr, cc});
        }
      }
    }
    std::vector<ActiveFacet> baseFacets;
    extractActiveFacets(seedCells, baseFacets);
    std::vector<double> baseStationVals(nPts, 0.0);
    for (size_t p = 0; p < nPts; ++p) {
      if (!Grid::is_dummy_value(obsPointsGrv[p].z)) {
        baseStationVals[p] = evaluateStationField(p, baseFacets);
      }
    }

    model.set_z(row, col, index, seedZ + 1.0);
    for (const auto &[cr, cc] : seedCells) {
      model.update_facet_column(cr, cc);
    }
    std::vector<ActiveFacet> pertFacets;
    extractActiveFacets(seedCells, pertFacets);

    const size_t seedStation = row * cols_ + col;
    double s0 = 0.0;
    if (seedStation < nPts && !Grid::is_dummy_value(obsPointsGrv[seedStation].z)) {
      s0 = std::abs(evaluateStationField(seedStation, pertFacets) - baseStationVals[seedStation]);
    } else {
      for (size_t p = 0; p < nPts; ++p) {
        if (!Grid::is_dummy_value(obsPointsGrv[p].z)) {
          double dVal = std::abs(evaluateStationField(p, pertFacets) - baseStationVals[p]);
          if (dVal > s0) s0 = dVal;
        }
      }
    }

    model.set_z(row, col, index, seedZ);
    for (const auto &[cr, cc] : seedCells) {
      model.update_facet_column(cr, cc);
    }

    if (s0 < 1e-12) {
      chosenRadius = stepSpacing;
    } else {
      chosenRadius = stepSpacing;
      int maxM = static_cast<int>(settings.max_radius_m / stepSpacing);
      for (int m = 1; m <= maxM; ++m) {
        double Rm = m * stepSpacing;
        if (Rm > settings.max_radius_m) {
          chosenRadius = settings.max_radius_m;
          break;
        }

        double maxSm = 0.0;
        bool foundCandidate = false;
        for (int dr = -m; dr <= m; ++dr) {
          for (int dc = -m; dc <= m; ++dc) {
            int cr = row + dr;
            int cc = col + dc;
            if (cr < 0 || cr >= static_cast<int>(model.rows()) ||
                cc < 0 || cc >= static_cast<int>(model.cols())) {
              continue;
            }
            double dist = std::hypot(model.xe_val(cc) - seedX, model.ye_val(cr) - seedY);
            if (dist < (m - 0.5) * stepSpacing || dist > (m + 0.5) * stepSpacing) {
              continue;
            }
            int cnt = static_cast<int>(model.count(cr, cc));
            for (int vi = 1; vi < cnt - 1; ++vi) {
              const auto *cp = model.at(cr, cc, vi);
              if (cp && cp->body_id() == targetBodyId) {
                foundCandidate = true;
                std::vector<std::pair<int, int>> candCells;
                for (int cdr = -1; cdr <= 0; ++cdr) {
                  for (int cdc = -1; cdc <= 0; ++cdc) {
                    int ccr = cr + cdr;
                    int ccc = cc + cdc;
                    if (ccr >= 0 && ccr < static_cast<int>(model.rows()) - 1 &&
                        ccc >= 0 && ccc < static_cast<int>(model.cols()) - 1) {
                      candCells.push_back({ccr, ccc});
                    }
                  }
                }
                std::vector<ActiveFacet> fBase;
                extractActiveFacets(candCells, fBase);
                std::vector<double> candBaseVals(nPts, 0.0);
                for (size_t p = 0; p < nPts; ++p) {
                  if (!Grid::is_dummy_value(obsPointsGrv[p].z)) {
                    candBaseVals[p] = evaluateStationField(p, fBase);
                  }
                }
                double zOrig = model.z(cr, cc, vi);
                model.set_z(cr, cc, vi, zOrig + 1.0);
                for (const auto &[ccr, ccc] : candCells) {
                  model.update_facet_column(ccr, ccc);
                }
                std::vector<ActiveFacet> fPert;
                extractActiveFacets(candCells, fPert);
                double dVal = 0.0;
                if (seedStation < nPts && !Grid::is_dummy_value(obsPointsGrv[seedStation].z)) {
                  dVal = std::abs(evaluateStationField(seedStation, fPert) - candBaseVals[seedStation]);
                } else {
                  for (size_t p = 0; p < nPts; ++p) {
                    if (!Grid::is_dummy_value(obsPointsGrv[p].z)) {
                      double diff = std::abs(evaluateStationField(p, fPert) - candBaseVals[p]);
                      if (diff > dVal) dVal = diff;
                    }
                  }
                }
                if (dVal > maxSm) maxSm = dVal;
                model.set_z(cr, cc, vi, zOrig);
                for (const auto &[ccr, ccc] : candCells) {
                  model.update_facet_column(ccr, ccc);
                }
                break;
              }
            }
          }
        }

        if (!foundCandidate || maxSm < settings.sensitivity_cutoff * s0) {
          chosenRadius = std::max(stepSpacing, (m - 1) * stepSpacing);
          break;
        }
        chosenRadius = Rm;
      }
    }
  }

  // 2. Collect Neighborhood Vertices within chosenRadius
  struct InvertibleVertex {
    int row;
    int col;
    int index;
    double original_z;
    double current_z;
    double z_min;
    double z_max;
  };
  std::vector<InvertibleVertex> vertices;

  double seedZU = (index > 0) ? model.z(row, col, index - 1) : model.z_max_val();
  double seedZD = (index + 1 < countSeed) ? model.z(row, col, index + 1) : model.z_min_val();
  double seedEps = std::clamp(0.01 * std::min(seedZU - seedZ, seedZ - seedZD), 0.01, 0.5);
  vertices.push_back({row, col, index, seedZ, seedZ, seedZD + seedEps, seedZU - seedEps});

  for (int r = 0; r < static_cast<int>(model.rows()); ++r) {
    for (int c = 0; c < static_cast<int>(model.cols()); ++c) {
      if (r == row && c == col) continue;
      double dist = std::hypot(model.xe_val(c) - seedX, model.ye_val(r) - seedY);
      if (dist <= chosenRadius) {
        int bestVi = -1;
        double minZDiff = 1e30;
        int cCount = static_cast<int>(model.count(r, c));
        for (int vi = 1; vi < cCount - 1; ++vi) {
          const auto *cp = model.at(r, c, vi);
          if (cp && cp->body_id() == targetBodyId) {
            double diff = std::abs(model.z(r, c, vi) - seedZ);
            if (diff < minZDiff) {
              minZDiff = diff;
              bestVi = vi;
            }
          }
        }
        if (bestVi > 0) {
          double curZ = model.z(r, c, bestVi);
          double zU = model.z(r, c, bestVi - 1);
          double zD = model.z(r, c, bestVi + 1);
          if (zD + 1e-4 < zU - 1e-4) {
            double eps = std::clamp(0.01 * std::min(zU - curZ, curZ - zD), 0.01, 0.5);
            vertices.push_back({r, c, bestVi, curZ, curZ, zD + eps, zU - eps});
          }
        }
      }
    }
  }

  const size_t K = vertices.size();
  if (K == 0) {
    result.log = "Error: No invertible vertices found in neighborhood.";
    return result;
  }

  // 3. Collect Local Affected Facet Cells
  std::set<std::pair<int, int>> localCellsSet;
  for (const auto &v : vertices) {
    for (int dr = -1; dr <= 0; ++dr) {
      for (int dc = -1; dc <= 0; ++dc) {
        int cr = v.row + dr;
        int cc = v.col + dc;
        if (cr >= 0 && cr < static_cast<int>(model.rows()) - 1 &&
            cc >= 0 && cc < static_cast<int>(model.cols()) - 1) {
          localCellsSet.insert({cr, cc});
        }
      }
    }
  }
  std::vector<std::pair<int, int>> localCells(localCellsSet.begin(), localCellsSet.end());

  // 4. Precompute Static Background Field (Superposition)
  Grid baseField(rows_, cols_, x0_, y0_, dx_, dy_, rotation_);
  baseField.zero();

  if (!modeled_grids_.empty() && modeled_grid(comp) != nullptr &&
      !modeled_grid(comp)->empty()) {
    Grid fullField = *modeled_grid(comp);
    std::vector<ActiveFacet> initLocalFacets;
    extractActiveFacets(localCells, initLocalFacets);
    Grid localField(rows_, cols_, x0_, y0_, dx_, dy_, rotation_);
    localField.zero();
    for (size_t p = 0; p < nPts; ++p) {
      size_t r = p / cols_;
      size_t c = p % cols_;
      if (!Grid::is_dummy_value(obsPointsGrv[p].z)) {
        localField(r, c) = evaluateStationField(p, initLocalFacets);
      } else {
        localField(r, c) = GRID_DUMMY;
      }
    }
    baseField = fullField - localField;
  } else {
    std::vector<Facet3Pt> staticFacets;
    for (int r = 0; r < static_cast<int>(model.rows()) - 1; ++r) {
      for (int c = 0; c < static_cast<int>(model.cols()) - 1; ++c) {
        if (localCellsSet.contains({r, c})) continue;
        if (const auto *lst = model.facet_list(r, c)) {
          for (const auto &fct : *lst) {
            Facet3Pt copy = fct;
            copy.set_sign(1.0);
            staticFacets.push_back(copy);
          }
        }
      }
    }
    compute_forward_field(staticFacets, true);
    baseField = *modeled_grid(comp);
  }

  // 5. Forward Evaluation Helper for Local Cells
  auto evalResidualsAndRms = [&](const std::vector<double> &zVals,
                                 std::vector<double> &outResiduals,
                                 double &outRms) {
    for (size_t k = 0; k < vertices.size(); ++k) {
      model.set_z(vertices[k].row, vertices[k].col, vertices[k].index, zVals[k]);
    }
    for (const auto &[cr, cc] : localCells) {
      model.update_facet_column(cr, cc);
    }
    std::vector<ActiveFacet> activeFacets;
    extractActiveFacets(localCells, activeFacets);

    outResiduals.assign(nPts, 0.0);
    double sumSq = 0.0;
    size_t validCount = 0;

    auto evalChunk = [&](size_t start_p, size_t end_p, double &sumLocal, size_t &cntLocal) {
      for (size_t p = start_p; p < end_p; ++p) {
        size_t r = p / cols_;
        size_t c = p % cols_;
        double obsVal = (*pObs)(r, c);
        if (Grid::is_dummy_value(obsVal) || baseField.is_dummy(r, c) ||
            Grid::is_dummy_value(obsPointsGrv[p].z)) {
          outResiduals[p] = 0.0;
          continue;
        }
        double modVal = baseField(r, c) + evaluateStationField(p, activeFacets);
        double diff = modVal - obsVal;
        outResiduals[p] = diff;
        sumLocal += diff * diff;
        cntLocal++;
      }
    };

    const auto [tCount] = pfld::detail::calculate_partition(nPts, 64);
    if (tCount <= 1) {
      evalChunk(0, nPts, sumSq, validCount);
    } else {
      std::vector<double> threadSum(tCount, 0.0);
      std::vector<size_t> threadCnt(tCount, 0);
      const size_t cSize = (nPts + tCount - 1) / tCount;
      {
        std::vector<pfld::detail::jthread_type> threads;
        threads.reserve(tCount);
        for (size_t t = 0; t < tCount; ++t) {
          size_t sp = t * cSize;
          size_t ep = std::min(sp + cSize, nPts);
          if (sp >= ep) break;
          threads.emplace_back([&, t, sp, ep]() {
            evalChunk(sp, ep, threadSum[t], threadCnt[t]);
          });
        }
      }
      for (size_t t = 0; t < tCount; ++t) {
        sumSq += threadSum[t];
        validCount += threadCnt[t];
      }
    }
    outRms = (validCount > 0) ? std::sqrt(sumSq / validCount) : 0.0;
  };

  std::vector<double> currentZ(K);
  for (size_t k = 0; k < K; ++k) {
    currentZ[k] = vertices[k].original_z;
  }
  std::vector<double> residuals(nPts, 0.0);
  double initialRms = 0.0;
  evalResidualsAndRms(currentZ, residuals, initialRms);
  double currentRms = initialRms;

  // 6. Construct Laplacian Regularization Matrix L (K x K)
  std::vector<double> L(K * K, 0.0);
  if (K > 1) {
    double totalWeight = 0.0;
    for (size_t i = 0; i < K; ++i) {
      for (size_t j = i + 1; j < K; ++j) {
        int dr = std::abs(vertices[i].row - vertices[j].row);
        int dc = std::abs(vertices[i].col - vertices[j].col);
        if (dr <= 1 && dc <= 1) {
          double dist = std::hypot(model.xe_val(vertices[i].col) - model.xe_val(vertices[j].col),
                                   model.ye_val(vertices[i].row) - model.ye_val(vertices[j].row));
          double w = (dist > 1e-3) ? (1.0 / (dist * dist)) : 1.0;
          L[i * K + j] -= w;
          L[j * K + i] -= w;
          L[i * K + i] += w;
          L[j * K + j] += w;
          totalWeight += 2.0 * w;
        }
      }
    }
    if (totalWeight > 1e-12) {
      double avgDiag = totalWeight / static_cast<double>(K);
      for (size_t i = 0; i < K * K; ++i) {
        L[i] /= avgDiag;
      }
    }
  }

  auto evalSmoothness = [&](const std::vector<double> &zVals) -> double {
    if (K <= 1) return 0.0;
    double cost = 0.0;
    for (size_t i = 0; i < K; ++i) {
      double dz_i = zVals[i] - vertices[i].original_z;
      double rowSum = 0.0;
      for (size_t j = 0; j < K; ++j) {
        double dz_j = zVals[j] - vertices[j].original_z;
        rowSum += L[i * K + j] * dz_j;
      }
      cost += dz_i * rowSum;
    }
    return 0.5 * cost;
  };

  const double alpha = settings.smoothness_weight;

  // 7. Valid Observation Stations
  std::vector<size_t> validStationIndices;
  validStationIndices.reserve(nPts);
  for (size_t p = 0; p < nPts; ++p) {
    size_t r = p / cols_;
    size_t c = p % cols_;
    if (!Grid::is_dummy_value((*pObs)(r, c)) && !baseField.is_dummy(r, c) &&
        !Grid::is_dummy_value(obsPointsGrv[p].z)) {
      validStationIndices.push_back(p);
    }
  }
  const size_t M = validStationIndices.size();
  if (M == 0) {
    result.log = "Error: No valid observation stations in survey area.";
    return result;
  }

  // 8. Jacobian Computation: column k = dF / dz_k
  auto computeJacobianColumn = [&](size_t k, std::vector<double> &colOut) {
    std::vector<std::pair<int, int>> cells_k;
    for (int dr = -1; dr <= 0; ++dr) {
      for (int dc = -1; dc <= 0; ++dc) {
        int cr = vertices[k].row + dr;
        int cc = vertices[k].col + dc;
        if (localCellsSet.contains({cr, cc})) {
          cells_k.push_back({cr, cc});
        }
      }
    }
    std::vector<ActiveFacet> facetsBase;
    extractActiveFacets(cells_k, facetsBase);
    std::vector<double> baseVals(M);
    for (size_t m = 0; m < M; ++m) {
      baseVals[m] = evaluateStationField(validStationIndices[m], facetsBase);
    }

    double hk = std::min(1.0, 0.05 * (vertices[k].z_max - vertices[k].z_min));
    if (hk < 1e-4) hk = 0.1;

    model.set_z(vertices[k].row, vertices[k].col, vertices[k].index, currentZ[k] + hk);
    for (const auto &[cr, cc] : cells_k) {
      model.update_facet_column(cr, cc);
    }
    std::vector<ActiveFacet> facetsPert;
    extractActiveFacets(cells_k, facetsPert);

    colOut.resize(M);
    for (size_t m = 0; m < M; ++m) {
      size_t p = validStationIndices[m];
      double valPert = evaluateStationField(p, facetsPert);
      colOut[m] = (valPert - baseVals[m]) / hk;
    }

    model.set_z(vertices[k].row, vertices[k].col, vertices[k].index, currentZ[k]);
    for (const auto &[cr, cc] : cells_k) {
      model.update_facet_column(cr, cc);
    }
  };

  std::vector<std::vector<double>> J_cols(K);
  for (size_t k = 0; k < K; ++k) {
    computeJacobianColumn(k, J_cols[k]);
  }

  // 9. Levenberg-Marquardt Multi-Parameter Inversion Loop
  double lambda = 0.01;
  int iter = 0;
  bool converged = false;

  for (; iter < settings.max_iter; ++iter) {
    std::vector<double> H_data(K * K, 0.0);
    std::vector<double> g_data(K, 0.0);

    for (size_t i = 0; i < K; ++i) {
      for (size_t j = i; j < K; ++j) {
        double dot = 0.0;
        for (size_t m = 0; m < M; ++m) {
          dot += J_cols[i][m] * J_cols[j][m];
        }
        double val = dot / static_cast<double>(M);
        H_data[i * K + j] = val;
        H_data[j * K + i] = val;
      }
    }

    double diagSum = 0.0;
    for (size_t i = 0; i < K; ++i) {
      diagSum += H_data[i * K + i];
    }
    double avgDataDiag = (K > 0 && diagSum > 1e-20) ? (diagSum / static_cast<double>(K)) : 1.0;
    double alpha_eff = alpha * avgDataDiag;

    for (size_t i = 0; i < K; ++i) {
      double jtr = 0.0;
      for (size_t m = 0; m < M; ++m) {
        size_t p = validStationIndices[m];
        jtr += J_cols[i][m] * residuals[p];
      }
      g_data[i] = jtr / static_cast<double>(M);
    }

    std::vector<double> H(K * K, 0.0);
    std::vector<double> g(K, 0.0);
    for (size_t i = 0; i < K; ++i) {
      for (size_t j = 0; j < K; ++j) {
        H[i * K + j] = H_data[i * K + j] + alpha_eff * L[i * K + j];
      }
      double l_dz = 0.0;
      for (size_t j = 0; j < K; ++j) {
        l_dz += L[i * K + j] * (currentZ[j] - vertices[j].original_z);
      }
      g[i] = g_data[i] + alpha_eff * l_dz;
    }

    std::vector<double> A = H;
    std::vector<double> b(K);
    for (size_t i = 0; i < K; ++i) {
      A[i * K + i] = H[i * K + i] * (1.0 + lambda) + 1e-15;
      b[i] = -g[i];
    }

    std::vector<double> delta_z(K, 0.0);
    if (!solve_linear_system(static_cast<int>(K), A, b, delta_z)) {
      lambda *= 10.0;
      if (lambda > 1e8) break;
      continue;
    }

    std::vector<double> candZ(K);
    double maxStep = 0.0;
    for (size_t k = 0; k < K; ++k) {
      candZ[k] = std::clamp(currentZ[k] + delta_z[k], vertices[k].z_min, vertices[k].z_max);
      maxStep = std::max(maxStep, std::abs(candZ[k] - currentZ[k]));
    }

    std::vector<double> candResiduals;
    double candRms = 0.0;
    evalResidualsAndRms(candZ, candResiduals, candRms);
    double candObj = 0.5 * (candRms * candRms) + 0.5 * alpha_eff * evalSmoothness(candZ);
    double curObj = 0.5 * (currentRms * currentRms) + 0.5 * alpha_eff * evalSmoothness(currentZ);

    if (candObj < curObj) {
      double objDecrease = curObj - candObj;
      currentZ = candZ;
      residuals = candResiduals;
      currentRms = candRms;
      lambda = std::max(lambda / 3.0, 1e-6);

      if (maxStep < settings.tol || objDecrease / std::max(curObj, 1e-6) < settings.tol) {
        converged = true;
        iter++;
        break;
      }
      for (size_t k = 0; k < K; ++k) {
        computeJacobianColumn(k, J_cols[k]);
      }
    } else {
      lambda *= 5.0;
      if (lambda > 1e8) {
        iter++;
        break;
      }
    }
  }

  // 10. Apply Final Depths & Update Model
  for (size_t k = 0; k < K; ++k) {
    int vIdx = vertices[k].index;
    model.move_vertex(vIdx, vertices[k].row, vertices[k].col, currentZ[k],
                      BodyMoveType::Constrained);
    result.vertex_changes.push_back({vertices[k].row, vertices[k].col,
                                     vertices[k].index, vertices[k].original_z,
                                     currentZ[k]});
  }

  // Synchronize Modeled Grid for target component
  for (size_t p = 0; p < nPts; ++p) {
    size_t r = p / cols_;
    size_t c = p % cols_;
    if (!baseField.is_dummy(r, c) && !pObs->is_dummy(r, c)) {
      modeled_grids_[comp](r, c) = (*pObs)(r, c) + residuals[p];
    }
  }

  auto finish_time = std::chrono::high_resolution_clock::now();
  double elapsed_sec =
      std::chrono::duration<double>(finish_time - start_time).count();

  result.converged = converged || (currentRms < initialRms);
  result.iterations = iter;
  result.initial_rms = initialRms;
  result.final_rms = currentRms;
  result.elapsed_time_sec = elapsed_sec;
  result.chosen_radius_m = chosenRadius;
  result.vertices_fitted = K;

  std::ostringstream log;
  log << "Tier 4 Neighborhood Inversion\n\n"
      << "Seed vertex: row=" << row << ", col=" << col << ", index=" << index << "\n"
      << "Target body ID: " << targetBodyId << "\n"
      << "Neighborhood radius: " << std::fixed << std::setprecision(1) << chosenRadius << " m ("
      << (settings.radius_mode == NeighborhoodRadiusMode::Automatic ? "Automatic" : "Custom") << ")\n"
      << "Vertices inverted simultaneously: " << K << "\n"
      << "Iterations: " << iter << "\n"
      << "Initial RMS: " << std::setprecision(6) << initialRms << "\n"
      << "Final RMS:   " << std::setprecision(6) << currentRms << "\n"
      << "RMS reduction: " << std::setprecision(2)
      << ((initialRms > 1e-12) ? ((initialRms - currentRms) / initialRms * 100.0) : 0.0) << "%\n"
      << "Elapsed time: " << std::setprecision(4) << elapsed_sec << "s\n";
  result.log = log.str();

  return result;
}

} // namespace mod3d
