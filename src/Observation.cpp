#include "mod3d/Observation.h"
#include "mod3d/Body.h"
#include "mod3d/Magnetics.h"
#include "pfld/pfld_compute.hpp"
#include <cmath>
#include <iomanip>
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
  swap(remove_observed_mean_, other.remove_observed_mean_);
  swap(observed_means_, other.observed_means_);
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
  if (remove_observed_mean_) {
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

void ObservationSpace::set_remove_observed_mean(bool enable) {
  remove_observed_mean_ = enable;
  update_virtual_observed_anomalies();
}

void ObservationSpace::update_virtual_observed_anomalies() {
  observed_means_.clear();
  observed_anomaly_grids_.clear();
  for (const auto &[comp, grd] : observed_grids_) {
    if (grd.empty())
      continue;
    const double meanVal = grd.mean();
    observed_means_[comp] = meanVal;
    if (remove_observed_mean_) {
      Grid anomaly = grd;
      anomaly -= meanVal;
      observed_anomaly_grids_.emplace(comp, std::move(anomaly));
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

} // namespace mod3d
