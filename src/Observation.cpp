#include "mod3d/Observation.h"
#include "mod3d/PotField.h"
#include "mod3d/FieldCompute.h"
#include "mod3d/Body.h"
#include <cmath>
#include <stdexcept>

namespace mod3d {

ObservationSpace::ObservationSpace()
{
}

ObservationSpace::ObservationSpace(
    size_t rows, size_t cols, double x0, double y0, double dx, double dy, double rotDeg)
{
    initGeometry(rows, cols, x0, y0, dx, dy, rotDeg);
}

void ObservationSpace::initGeometry(
    size_t rows, size_t cols, double x0, double y0, double dx, double dy, double rotDeg)
{
    m_rows = rows;
    m_cols = cols;
    m_x0 = x0;
    m_y0 = y0;
    m_dx = dx;
    m_dy = dy;
    m_rotDeg = rotDeg;

    m_reliefGrid = Grid(rows, cols, x0, y0, dx, dy, rotDeg);
    m_grvElevGrid = Grid(rows, cols, x0, y0, dx, dy, rotDeg);
    m_magElevGrid = Grid(rows, cols, x0, y0, dx, dy, rotDeg);
    m_tensorElevGrid = Grid(rows, cols, x0, y0, dx, dy, rotDeg);
}

void ObservationSpace::setGravityObservation(ObservationMode mode, double heightVal)
{
    m_grvMode = mode;
    m_grvHeight = heightVal;
}

void ObservationSpace::setGravityObservationGrid(const Grid &grid)
{
    m_grvMode = ObservationMode::ElevationGrid;
    m_grvElevGrid = grid;
}

void ObservationSpace::setMagneticObservation(ObservationMode mode, double heightVal)
{
    m_magMode = mode;
    m_magHeight = heightVal;
}

void ObservationSpace::setMagneticObservationGrid(const Grid &grid)
{
    m_magMode = ObservationMode::ElevationGrid;
    m_magElevGrid = grid;
}

void ObservationSpace::setTensorObservation(ObservationMode mode, double heightVal)
{
    m_tensorMode = mode;
    m_tensorHeight = heightVal;
}

void ObservationSpace::setTensorObservationGrid(const Grid &grid)
{
    m_tensorMode = ObservationMode::ElevationGrid;
    m_tensorElevGrid = grid;
}

void ObservationSpace::setSurfaceRelief(const Grid &relief)
{
    m_reliefGrid = relief;
}

void ObservationSpace::setAmbientFieldParams(double inclinationDeg, double declinationDeg, double intensityNT)
{
    m_incDeg = inclinationDeg;
    m_decDeg = declinationDeg;
    m_magIntensity = intensityNT;
}

Point3D ObservationSpace::getAmbientFieldVector() const
{
    return AmbientField(m_incDeg, m_decDeg, m_magIntensity);
}

double ObservationSpace::getElevationAt(
    ObservationMode mode, double heightVal, const Grid &elevGrid, size_t r, size_t c) const
{
    switch (mode) {
    case ObservationMode::SensorHeight:
        if (!m_reliefGrid.empty()) {
            return m_reliefGrid(r, c) + heightVal;
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

std::vector<Point3D> ObservationSpace::getObservationPoints(FieldComponent comp) const
{
    std::vector<Point3D> points;
    if (m_rows == 0 || m_cols == 0) return points;

    points.reserve(m_rows * m_cols);

    ObservationMode mode = m_grvMode;
    double heightVal = m_grvHeight;
    const Grid *elevGrid = &m_grvElevGrid;

    int compVal = static_cast<int>(comp);
    if (compVal >= 10 && compVal < 20) {
        mode = m_magMode;
        heightVal = m_magHeight;
        elevGrid = &m_magElevGrid;
    } else if (compVal >= 20) {
        mode = m_tensorMode;
        heightVal = m_tensorHeight;
        elevGrid = &m_tensorElevGrid;
    }

    const double pi = 3.14159265358979323846;
    const double rad = m_rotDeg * pi / 180.0;
    const double cRot = std::cos(rad);
    const double sRot = std::sin(rad);

    for (size_t r = 0; r < m_rows; ++r) {
        for (size_t c = 0; c < m_cols; ++c) {
            double x = m_x0 + (c * m_dx) * cRot - (r * m_dy) * sRot;
            double y = m_y0 + (c * m_dx) * sRot + (r * m_dy) * cRot;
            double z = getElevationAt(mode, heightVal, *elevGrid, r, c);
            points.emplace_back(x, y, z);
        }
    }

    return points;
}

void ObservationSpace::ensureGridAllocated(std::map<FieldComponent, Grid> &gridMap, FieldComponent comp)
{
    auto it = gridMap.find(comp);
    if (it == gridMap.end()) {
        gridMap.emplace(comp, Grid(m_rows, m_cols, m_x0, m_y0, m_dx, m_dy, m_rotDeg));
    }
}

Grid *ObservationSpace::getModeledGrid(FieldComponent comp)
{
    ensureGridAllocated(m_modeledGrids, comp);
    return &m_modeledGrids[comp];
}

const Grid *ObservationSpace::getModeledGrid(FieldComponent comp) const
{
    auto it = m_modeledGrids.find(comp);
    if (it != m_modeledGrids.end()) return &it->second;
    return nullptr;
}

Grid *ObservationSpace::getObservedGrid(FieldComponent comp)
{
    ensureGridAllocated(m_observedGrids, comp);
    return &m_observedGrids[comp];
}

const Grid *ObservationSpace::getObservedGrid(FieldComponent comp) const
{
    auto it = m_observedGrids.find(comp);
    if (it != m_observedGrids.end()) return &it->second;
    return nullptr;
}

void ObservationSpace::setObservedGrid(FieldComponent comp, const Grid &grid)
{
    m_observedGrids[comp] = grid;
}

Grid *ObservationSpace::getDifferenceGrid(FieldComponent comp)
{
    ensureGridAllocated(m_differenceGrids, comp);
    return &m_differenceGrids[comp];
}

const Grid *ObservationSpace::getDifferenceGrid(FieldComponent comp) const
{
    auto it = m_differenceGrids.find(comp);
    if (it != m_differenceGrids.end()) return &it->second;
    return nullptr;
}

void ObservationSpace::computeForwardField(const std::vector<Facet3Pt> &facets, bool zeroFirst)
{
    if (m_rows == 0 || m_cols == 0) return;

    ensureGridAllocated(m_modeledGrids, FieldComponent::GZ);
    Grid &grdGz = m_modeledGrids[FieldComponent::GZ];
    if (zeroFirst) grdGz.zeroData();

    ensureGridAllocated(m_modeledGrids, FieldComponent::GX);
    ensureGridAllocated(m_modeledGrids, FieldComponent::GY);
    Grid &grdGx = m_modeledGrids[FieldComponent::GX];
    Grid &grdGy = m_modeledGrids[FieldComponent::GY];
    if (zeroFirst) {
        grdGx.zeroData();
        grdGy.zeroData();
    }

    auto obsPoints = getObservationPoints(FieldComponent::GZ);
    const size_t nPts = obsPoints.size();

    for (size_t p = 0; p < nPts; ++p) {
        Point3D totalG(0.0, 0.0, 0.0);
        for (const auto &fct : facets) {
            double densContrast = 0.0;
            if (fct.pBody) {
                double bDens = fct.pBody->GetDensity();
                double oposDens = fct.pBodyOpos ? fct.pBodyOpos->GetDensity() : m_refDensity;
                densContrast = bDens - oposDens;
            } else if (fct.densityOpos != 0.0) {
                densContrast = fct.density - fct.densityOpos;
            } else {
                densContrast = fct.density - m_refDensity;
            }

            Point3D g(0.0, 0.0, 0.0);
            fct.Fld_G(obsPoints[p], g);
            totalG += g * (densContrast * fct.dSign);
        }

        size_t r = p / m_cols;
        size_t c = p % m_cols;
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

    computeTotalFields();
}

void ObservationSpace::updateDeltaField(const std::vector<Facet3Pt> &deltaFacets)
{
    if (deltaFacets.empty()) return;
    computeForwardField(deltaFacets, false);
}

void ObservationSpace::computeTotalFields()
{
    Grid *pGx = getModeledGrid(FieldComponent::GX);
    Grid *pGy = getModeledGrid(FieldComponent::GY);
    Grid *pGz = getModeledGrid(FieldComponent::GZ);
    Grid *pGTot = getModeledGrid(FieldComponent::G_TOT);

    if (pGx && pGy && pGz && pGTot) {
        for (size_t r = 0; r < m_rows; ++r) {
            for (size_t c = 0; c < m_cols; ++c) {
                double gx = (*pGx)(r, c);
                double gy = (*pGy)(r, c);
                double gz = (*pGz)(r, c);
                if (pGx->isDummy(r, c) || pGy->isDummy(r, c) || pGz->isDummy(r, c)) {
                    (*pGTot)(r, c) = GRID_DUMMY;
                } else {
                    (*pGTot)(r, c) = std::sqrt(gx * gx + gy * gy + gz * gz);
                }
            }
        }
    }
}

void ObservationSpace::computeDifference(FieldComponent comp, bool removeMean)
{
    Grid *pMod = getModeledGrid(comp);
    Grid *pObs = getObservedGrid(comp);
    Grid *pDif = getDifferenceGrid(comp);

    if (!pMod || !pObs || !pDif) return;

    for (size_t r = 0; r < m_rows; ++r) {
        for (size_t c = 0; c < m_cols; ++c) {
            if (pMod->isDummy(r, c) || pObs->isDummy(r, c)) {
                (*pDif)(r, c) = GRID_DUMMY;
            } else {
                (*pDif)(r, c) = (*pMod)(r, c) - (*pObs)(r, c);
            }
        }
    }

    if (removeMean) {
        double meanVal = pDif->getMean();
        *pMod -= meanVal;
        for (size_t r = 0; r < m_rows; ++r) {
            for (size_t c = 0; c < m_cols; ++c) {
                if (!pDif->isDummy(r, c)) {
                    (*pDif)(r, c) -= meanVal;
                }
            }
        }
    }
}

FitResult ObservationSpace::fitDensity(
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

    Grid *pObs = getObservedGrid(comp);
    if (!pObs || pObs->empty()) {
        return FitResult{};
    }

    // 1. Compute unit response (contrast = 1.0)
    auto obsPoints = getObservationPoints(comp);
    Grid grdUnit(m_rows, m_cols, m_x0, m_y0, m_dx, m_dy, m_rotDeg);

    for (size_t p = 0; p < obsPoints.size(); ++p) {
        double unitGz = 0.0;
        for (const auto &fct : bodyFacets) {
            Point3D g(0, 0, 0);
            fct.Fld_G(obsPoints[p], g);
            unitGz += g.z * fct.dSign;
        }
        size_t r = p / m_cols;
        size_t c = p % m_cols;
        grdUnit(r, c) = unitGz;
    }

    // Objective function for differential density x = (dens - m_refDensity)
    auto objFunc = [&](double x) -> double {
        double sumSq = 0.0;
        size_t count = 0;

        for (size_t r = 0; r < m_rows; ++r) {
            for (size_t c = 0; c < m_cols; ++c) {
                if (!pObs->isDummy(r, c) && !grdUnit.isDummy(r, c)) {
                    double calcVal = x * grdUnit(r, c);
                    double diff = calcVal - (*pObs)(r, c);
                    sumSq += diff * diff;
                    count++;
                }
            }
        }
        return count > 0 ? std::sqrt(sumSq / count) : 0.0;
    };

    double dens0 = initialDensity - m_refDensity;
    double startA = dens0;
    double startB = dens0 + 10.0;

    FitResult res = Inversion1D::optimize(
        startA, startB, objFunc, method, tol, 100, "Density inversion");

    double optimalDiffDensity = res.optimalParameter;
    double optimalDensity = optimalDiffDensity + m_refDensity;
    pBody->SetDensity(optimalDensity);
    res.optimalParameter = optimalDensity;

    return res;
}

FitResult ObservationSpace::fitVertex(
    Model &model,
    int row, int col, int index,
    FieldComponent comp,
    double tol,
    FitMethod method)
{
    Grid *pObs = getObservedGrid(comp);
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

        computeForwardField(currentFacets, true);
        const Grid *pMod = getModeledGrid(comp);

        double sumSq = 0.0;
        size_t count = 0;
        for (size_t r = 0; r < m_rows; ++r) {
            for (size_t c = 0; c < m_cols; ++c) {
                if (!pObs->isDummy(r, c) && !pMod->isDummy(r, c)) {
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
