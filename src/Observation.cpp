#include "mod3d/Observation.h"
#include "mod3d/Magnetics.h"
#include "mod3d/Body.h"
#include <cmath>
#include <stdexcept>
#include <utility>

namespace mod3d {

ObservationSpace::ObservationSpace() = default;

ObservationSpace::ObservationSpace(
    size_t rows, size_t cols, double x0, double y0, double dx, double dy, double rotDeg)
{
    init_geometry(rows, cols, x0, y0, dx, dy, rotDeg);
}

void ObservationSpace::swap(ObservationSpace &other) noexcept
{
    using std::swap;
    swap(rows_, other.rows_);
    swap(cols_, other.cols_);
    swap(x0_, other.x0_);
    swap(y0_, other.y0_);
    swap(dx_, other.dx_);
    swap(dy_, other.dy_);
    swap(rot_deg_, other.rot_deg_);
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
    swap(inc_deg_, other.inc_deg_);
    swap(dec_deg_, other.dec_deg_);
    swap(mag_intensity_, other.mag_intensity_);
    swap(ref_density_, other.ref_density_);
    swap(grv_units_, other.grv_units_);
    swap(mag_units_, other.mag_units_);
    swap(tensor_units_, other.tensor_units_);
    swap(modeled_grids_, other.modeled_grids_);
    swap(observed_grids_, other.observed_grids_);
    swap(difference_grids_, other.difference_grids_);
}

void ObservationSpace::init_geometry(
    size_t rows, size_t cols, double x0, double y0, double dx, double dy, double rotDeg)
{
    rows_ = rows;
    cols_ = cols;
    x0_ = x0;
    y0_ = y0;
    dx_ = dx;
    dy_ = dy;
    rot_deg_ = rotDeg;

    relief_grid_ = Grid(rows, cols, x0, y0, dx, dy, rotDeg);
    grv_elev_grid_ = Grid(rows, cols, x0, y0, dx, dy, rotDeg);
    mag_elev_grid_ = Grid(rows, cols, x0, y0, dx, dy, rotDeg);
    tensor_elev_grid_ = Grid(rows, cols, x0, y0, dx, dy, rotDeg);
}

void ObservationSpace::set_gravity_observation(ObservationMode mode, double heightVal)
{
    grv_mode_ = mode;
    grv_height_ = heightVal;
}

void ObservationSpace::set_gravity_observation_grid(const Grid &grid)
{
    grv_mode_ = ObservationMode::ElevationGrid;
    grv_elev_grid_ = grid;
}

void ObservationSpace::set_magnetic_observation(ObservationMode mode, double heightVal)
{
    mag_mode_ = mode;
    mag_height_ = heightVal;
}

void ObservationSpace::set_magnetic_observation_grid(const Grid &grid)
{
    mag_mode_ = ObservationMode::ElevationGrid;
    mag_elev_grid_ = grid;
}

void ObservationSpace::set_tensor_observation(ObservationMode mode, double heightVal)
{
    tensor_mode_ = mode;
    tensor_height_ = heightVal;
}

void ObservationSpace::set_tensor_observation_grid(const Grid &grid)
{
    tensor_mode_ = ObservationMode::ElevationGrid;
    tensor_elev_grid_ = grid;
}

void ObservationSpace::set_surface_relief(const Grid &relief)
{
    relief_grid_ = relief;
}

void ObservationSpace::set_ambient_field_params(double inclinationDeg, double declinationDeg, double intensityNT)
{
    inc_deg_ = inclinationDeg;
    dec_deg_ = declinationDeg;
    mag_intensity_ = intensityNT;
}

Point3D ObservationSpace::ambient_field_vector() const
{
    return ambient_field(inc_deg_, dec_deg_, mag_intensity_);
}

double ObservationSpace::get_elevation_at(
    ObservationMode mode, double heightVal, const Grid &elevGrid, size_t r, size_t c) const
{
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

std::vector<Point3D> ObservationSpace::observation_points(FieldComponent comp) const
{
    std::vector<Point3D> points;
    if (rows_ == 0 || cols_ == 0) return points;

    points.reserve(rows_ * cols_);

    ObservationMode mode = grv_mode_;
    double heightVal = grv_height_;
    const Grid *elevGrid = &grv_elev_grid_;

    if (is_magnetic_component(comp)) {
        mode = mag_mode_;
        heightVal = mag_height_;
        elevGrid = &mag_elev_grid_;
    } else if (is_tensor_component(comp)) {
        mode = tensor_mode_;
        heightVal = tensor_height_;
        elevGrid = &tensor_elev_grid_;
    }

    const double pi = std::numbers::pi_v<double>;
    const double rad = rot_deg_ * pi / 180.0;
    const double cRot = std::cos(rad);
    const double sRot = std::sin(rad);

    for (size_t r = 0; r < rows_; ++r) {
        for (size_t c = 0; c < cols_; ++c) {
            double x = x0_ + (c * dx_) * cRot - (r * dy_) * sRot;
            double y = y0_ + (c * dx_) * sRot + (r * dy_) * cRot;
            double z = get_elevation_at(mode, heightVal, *elevGrid, r, c);
            points.emplace_back(x, y, z);
        }
    }

    return points;
}

void ObservationSpace::ensure_grid_allocated(std::map<FieldComponent, Grid> &gridMap, FieldComponent comp)
{
    auto it = gridMap.find(comp);
    if (it == gridMap.end()) {
        gridMap.emplace(comp, Grid(rows_, cols_, x0_, y0_, dx_, dy_, rot_deg_));
    }
}

Grid *ObservationSpace::modeled_grid(FieldComponent comp)
{
    ensure_grid_allocated(modeled_grids_, comp);
    return &modeled_grids_[comp];
}

const Grid *ObservationSpace::modeled_grid(FieldComponent comp) const
{
    auto it = modeled_grids_.find(comp);
    if (it != modeled_grids_.end()) return &it->second;
    return nullptr;
}

Grid *ObservationSpace::observed_grid(FieldComponent comp)
{
    auto it = observed_grids_.find(comp);
    if (it != observed_grids_.end()) return &it->second;
    return nullptr;
}

const Grid *ObservationSpace::observed_grid(FieldComponent comp) const
{
    auto it = observed_grids_.find(comp);
    if (it != observed_grids_.end()) return &it->second;
    return nullptr;
}

void ObservationSpace::set_modeled_grid(FieldComponent comp, const Grid &grid)
{
    modeled_grids_[comp] = grid;
}

void ObservationSpace::set_observed_grid(FieldComponent comp, const Grid &grid)
{
    observed_grids_[comp] = grid;
}

Grid *ObservationSpace::difference_grid(FieldComponent comp)
{
    ensure_grid_allocated(difference_grids_, comp);
    return &difference_grids_[comp];
}

const Grid *ObservationSpace::difference_grid(FieldComponent comp) const
{
    auto it = difference_grids_.find(comp);
    if (it != difference_grids_.end()) return &it->second;
    return nullptr;
}

void ObservationSpace::set_difference_grid(FieldComponent comp, const Grid &grid)
{
    difference_grids_[comp] = grid;
}

void ObservationSpace::compute_forward_field(const std::vector<Facet3Pt> &facets, bool zeroFirst)
{
    if (rows_ == 0 || cols_ == 0) return;

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

    bool hasMagnetics = false;
    for (const auto &fct : facets) {
        if ((fct.primary.body && fct.primary.body->is_active() && !fct.primary.body->magnetization_vector().is_zero()) ||
            (fct.opposite.body && fct.opposite.body->is_active() && !fct.opposite.body->magnetization_vector().is_zero())) {
            hasMagnetics = true;
            break;
        }
    }

    auto obsPointsGrv = observation_points(FieldComponent::GZ);
    std::vector<Point3D> obsPointsMag;
    if (hasMagnetics) {
        obsPointsMag = observation_points(FieldComponent::MZ);
    }

    const size_t nPts = obsPointsGrv.size();

    for (size_t p = 0; p < nPts; ++p) {
        Point3D totalG(0.0, 0.0, 0.0);
        Point3D totalM(0.0, 0.0, 0.0);

        bool validGrv = !Grid::is_dummy_value(obsPointsGrv[p].z);
        bool validMag = hasMagnetics && p < obsPointsMag.size() && !Grid::is_dummy_value(obsPointsMag[p].z);

        Point3D rMag(0.0, 0.0, 0.0);
        if (validMag) {
            rMag = Point3D(obsPointsMag[p].x + 0.001, obsPointsMag[p].y + 0.003, obsPointsMag[p].z);
        }

        if (validGrv || validMag) {
            for (const auto &fct : facets) {
                if (validGrv) {
                    double densContrast = 0.0;
                    if (fct.primary.body) {
                        double bDens = fct.primary.body->density();
                        double oposDens = fct.opposite.body ? fct.opposite.body->density() : ref_density_;
                        densContrast = bDens - oposDens;
                    } else if (fct.opposite.density != 0.0) {
                        densContrast = fct.primary.density - fct.opposite.density;
                    } else {
                        densContrast = fct.primary.density - ref_density_;
                    }

                    Point3D g(0.0, 0.0, 0.0);
                    fct.fld_g(obsPointsGrv[p], g);
                    totalG += g * (densContrast * fct.dSign);
                }

                if (validMag) {
                    Point3D fMag(0.0, 0.0, 0.0);
                    if (fct.primary.body && fct.primary.body->is_active() && !fct.primary.body->magnetization_vector().is_zero()) {
                        fct.fld_gs(rMag, fct.primary.body->magnetization_vector(), fMag, 1.0);
                    }
                    if (fct.opposite.body && fct.opposite.body->is_active() && !fct.opposite.body->magnetization_vector().is_zero()) {
                        fct.fld_gs(rMag, fct.opposite.body->magnetization_vector(), fMag, -1.0);
                    }
                    totalM -= fMag * (fct.dSign * mag_units_);
                }
            }
        }

        size_t r = p / cols_;
        size_t c = p % cols_;
        Point3D scaledG = totalG * grv_units_;

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
                if (!grdGx.is_dummy(r, c)) grdGx(r, c) += scaledG.x;
                if (!grdGy.is_dummy(r, c)) grdGy(r, c) += scaledG.y;
                if (!grdGz.is_dummy(r, c)) grdGz(r, c) += scaledG.z;
            }
            if (validMag) {
                if (!grdMx.is_dummy(r, c)) grdMx(r, c) += totalM.x;
                if (!grdMy.is_dummy(r, c)) grdMy(r, c) += totalM.y;
                if (!grdMz.is_dummy(r, c)) grdMz(r, c) += totalM.z;
            }
        }
    }

    compute_total_fields();
}

void ObservationSpace::update_delta_field(const std::vector<Facet3Pt> &deltaFacets)
{
    if (deltaFacets.empty()) return;
    compute_forward_field(deltaFacets, false);
}

void ObservationSpace::compute_total_fields()
{
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
                    (*pDT)(r, c) = -mag_delta_t(mx, my, mz, af.x, af.y, af.z, mag_intensity_);
                }
            }
        }
    }
}

void ObservationSpace::compute_difference(FieldComponent comp, bool removeMean)
{
    Grid *pMod = modeled_grid(comp);
    Grid *pObs = observed_grid(comp);
    if ((!pObs || pObs->empty()) && comp == FieldComponent::GZ) {
        pObs = observed_grid(FieldComponent::G_TOT);
    } else if ((!pObs || pObs->empty()) && comp == FieldComponent::G_TOT) {
        pObs = observed_grid(FieldComponent::GZ);
    } else if ((!pObs || pObs->empty()) && comp == FieldComponent::DELTA_T) {
        pObs = observed_grid(FieldComponent::MZ);
    } else if ((!pObs || pObs->empty()) && comp == FieldComponent::MZ) {
        pObs = observed_grid(FieldComponent::DELTA_T);
    }
    Grid *pDif = difference_grid(comp);

    if (!pMod || !pObs || !pDif) return;

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

FitResult ObservationSpace::fit_density(
    Body *pBody,
    const std::vector<Facet3Pt> &bodyFacets,
    FieldComponent comp,
    double initialDensity,
    double tol,
    FitMethod method)
{
    if (!pBody || bodyFacets.empty()) {
        return FitResult{};
    }

    Grid *pObs = observed_grid(comp);
    if (!pObs || pObs->empty()) {
        return FitResult{};
    }

    // 1. Compute unit response (contrast = 1.0)
    auto obsPoints = observation_points(comp);
    Grid grdUnit(rows_, cols_, x0_, y0_, dx_, dy_, rot_deg_);

    for (size_t p = 0; p < obsPoints.size(); ++p) {
        double unitGz = 0.0;
        for (const auto &fct : bodyFacets) {
            Point3D g(0, 0, 0);
            fct.fld_g(obsPoints[p], g);
            unitGz += g.z * fct.dSign;
        }
        size_t r = p / cols_;
        size_t c = p % cols_;
        grdUnit(r, c) = unitGz * grv_units_;
    }

    // Objective function for differential density x = (dens - ref_density_)
    auto objFunc = [&](double x) -> double {
        double sumSq = 0.0;
        size_t count = 0;

        for (size_t r = 0; r < rows_; ++r) {
            for (size_t c = 0; c < cols_; ++c) {
                if (!pObs->is_dummy(r, c) && !grdUnit.is_dummy(r, c)) {
                    double calcVal = x * grdUnit(r, c);
                    double diff = calcVal - (*pObs)(r, c);
                    sumSq += diff * diff;
                    count++;
                }
            }
        }
        return count > 0 ? std::sqrt(sumSq / count) : 0.0;
    };

    double dens0 = initialDensity - ref_density_;
    double startA = dens0;
    double startB = dens0 + 10.0;

    FitResult res = Inversion1D::optimize(
        startA, startB, objFunc, method, tol, 100, "Density inversion");

    double optimalDiffDensity = res.optimal_parameter;
    double optimalDensity = optimalDiffDensity + ref_density_;
    pBody->set_density(optimalDensity);
    res.optimal_parameter = optimalDensity;

    return res;
}

FitResult ObservationSpace::fit_vertex(
    Model &model,
    int row, int col, int index,
    FieldComponent comp,
    double tol,
    FitMethod method)
{
    Grid *pObs = observed_grid(comp);
    if (!pObs || pObs->empty()) {
        return FitResult{};
    }

    double originalZ = model.z(row, col, index);
    double zU = (index > 0) ? model.z(row, col, index - 1) : model.z_max();
    double zD = (index + 1 < static_cast<int>(model.count(row, col))) ? model.z(row, col, index + 1) : model.z_min();

    // Objective function evaluating RMS of (modeled - observed) when vertex is at z
    auto objFunc = [&](double zVal) -> double {
        if (zVal <= zD + 1e-6 || zVal >= zU - 1e-6) {
            return 1e9;
        }
        int vIdx = index;
        int moveRes = model.move_vertex(vIdx, row, col, zVal, BodyMoveType::Constrained);
        if (moveRes != 0) {
            return 1e9;
        }

        std::vector<Facet3Pt> currentFacets;
        model.facets_computation(currentFacets);

        compute_forward_field(currentFacets, true);
        const Grid *pMod = modeled_grid(comp);

        double sumSq = 0.0;
        size_t count = 0;
        for (size_t r = 0; r < rows_; ++r) {
            for (size_t c = 0; c < cols_; ++c) {
                if (!pObs->is_dummy(r, c) && !pMod->is_dummy(r, c)) {
                    double diff = (*pMod)(r, c) - (*pObs)(r, c);
                    sumSq += diff * diff;
                    count++;
                }
            }
        }
        return count > 0 ? std::sqrt(sumSq / count) : 0.0;
    };

    double eps = 0.01 * std::min(zU - originalZ, originalZ - zD);
    if (eps <= 0.01) eps = 0.5;

    double startA = originalZ;
    double startB = originalZ + eps;
    if (startB >= zU) startB = originalZ - eps;

    FitResult res = Inversion1D::optimize(
        startA, startB, objFunc, method, tol, 100, "Vertex inversion");

    int vIdx = index;
    model.move_vertex(vIdx, row, col, res.optimal_parameter, BodyMoveType::Normal);

    return res;
}

} // namespace mod3d
