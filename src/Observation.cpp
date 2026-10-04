#include "mod3d/Observation.h"
#include "mod3d/PotField.h"
#include "mod3d/FieldCompute.h"
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
    return AmbientField(inc_deg_, dec_deg_, mag_intensity_);
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
    ensure_grid_allocated(observed_grids_, comp);
    return &observed_grids_[comp];
}

const Grid *ObservationSpace::observed_grid(FieldComponent comp) const
{
    auto it = observed_grids_.find(comp);
    if (it != observed_grids_.end()) return &it->second;
    return nullptr;
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

void ObservationSpace::compute_forward_field(const std::vector<Facet3Pt> &facets, bool zeroFirst)
{
    if (rows_ == 0 || cols_ == 0) return;

    ensure_grid_allocated(modeled_grids_, FieldComponent::GZ);
    Grid &grdGz = modeled_grids_[FieldComponent::GZ];
    if (zeroFirst) grdGz.zero();

    ensure_grid_allocated(modeled_grids_, FieldComponent::GX);
    ensure_grid_allocated(modeled_grids_, FieldComponent::GY);
    Grid &grdGx = modeled_grids_[FieldComponent::GX];
    Grid &grdGy = modeled_grids_[FieldComponent::GY];
    if (zeroFirst) {
        grdGx.zero();
        grdGy.zero();
    }

    auto obsPoints = observation_points(FieldComponent::GZ);
    const size_t nPts = obsPoints.size();

    for (size_t p = 0; p < nPts; ++p) {
        Point3D totalG(0.0, 0.0, 0.0);
        for (const auto &fct : facets) {
            double densContrast = 0.0;
            if (fct.pBody) {
                double bDens = fct.pBody->density();
                double oposDens = fct.pBodyOpos ? fct.pBodyOpos->density() : ref_density_;
                densContrast = bDens - oposDens;
            } else if (fct.densityOpos != 0.0) {
                densContrast = fct.density - fct.densityOpos;
            } else {
                densContrast = fct.density - ref_density_;
            }

            Point3D g(0.0, 0.0, 0.0);
            fct.Fld_G(obsPoints[p], g);
            totalG += g * (densContrast * fct.dSign);
        }

        size_t r = p / cols_;
        size_t c = p % cols_;
        if (zeroFirst) {
            grdGx(r, c) = totalG.x;
            grdGy(r, c) = totalG.y;
            grdGz(r, c) = totalG.z;
        } else {
            grdGx(r, c) += totalG.x;
            grdGy(r, c) += totalG.y;
            grdGz(r, c) += totalG.z;
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
}

void ObservationSpace::compute_difference(FieldComponent comp, bool removeMean)
{
    Grid *pMod = modeled_grid(comp);
    Grid *pObs = observed_grid(comp);
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
            fct.Fld_G(obsPoints[p], g);
            unitGz += g.z * fct.dSign;
        }
        size_t r = p / cols_;
        size_t c = p % cols_;
        grdUnit(r, c) = unitGz;
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

    double optimalDiffDensity = res.optimalParameter;
    double optimalDensity = optimalDiffDensity + ref_density_;
    pBody->set_density(optimalDensity);
    res.optimalParameter = optimalDensity;

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

    double originalZ = model.getZ(row, col, index);
    double zU = (index > 0) ? model.getZ(row, col, index - 1) : model.getHeaven();
    double zD = (index + 1 < static_cast<int>(model.getCount(row, col))) ? model.getZ(row, col, index + 1) : model.getHell();

    // Objective function evaluating RMS of (modeled - observed) when vertex is at z
    auto objFunc = [&](double z) -> double {
        if (z <= zD + 1e-6 || z >= zU - 1e-6) {
            return 1e9;
        }
        int vIdx = index;
        int moveRes = model.moveVertex(vIdx, row, col, z, BodyMoveType::Constrained);
        if (moveRes != 0) {
            return 1e9;
        }

        std::vector<Facet3Pt> currentFacets;
        model.getFacetsComputation(currentFacets);

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
    model.moveVertex(vIdx, row, col, res.optimalParameter, BodyMoveType::Normal);

    return res;
}

} // namespace mod3d
