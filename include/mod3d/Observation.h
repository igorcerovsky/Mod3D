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

/**
 * @brief Geophysical Observation Space & Forward Modeling / Inversion Coordinator.
 * 
 * Manages observation geometry, flight levels, modeled/observed/difference grids,
 * forward field simulation, real-time delta updates, and 1D inversions.
 */
class ObservationSpace {
public:
    ObservationSpace();
    ObservationSpace(size_t rows, size_t cols, double x0, double y0, double dx, double dy, double rotDeg = 0.0);

    void initGeometry(size_t rows, size_t cols, double x0, double y0, double dx, double dy, double rotDeg = 0.0);

    // Dimensions & Geometry
    size_t getRows() const { return m_rows; }
    size_t getCols() const { return m_cols; }
    double getX0() const { return m_x0; }
    double getY0() const { return m_y0; }
    double getDx() const { return m_dx; }
    double getDy() const { return m_dy; }
    double getRotDeg() const { return m_rotDeg; }

    // Height & Elevation Settings
    void setGravityObservation(ObservationMode mode, double heightVal);
    void setGravityObservationGrid(const Grid &grid);
    ObservationMode getGravityMode() const { return m_grvMode; }
    double getGravityHeight() const { return m_grvHeight; }
    const Grid &getGravityElevGrid() const { return m_grvElevGrid; }

    void setMagneticObservation(ObservationMode mode, double heightVal);
    void setMagneticObservationGrid(const Grid &grid);
    ObservationMode getMagneticMode() const { return m_magMode; }
    double getMagneticHeight() const { return m_magHeight; }
    const Grid &getMagneticElevGrid() const { return m_magElevGrid; }

    void setTensorObservation(ObservationMode mode, double heightVal);
    void setTensorObservationGrid(const Grid &grid);
    ObservationMode getTensorMode() const { return m_tensorMode; }
    double getTensorHeight() const { return m_tensorHeight; }
    const Grid &getTensorElevGrid() const { return m_tensorElevGrid; }

    void setSurfaceRelief(const Grid &relief);
    const Grid &getSurfaceRelief() const { return m_reliefGrid; }

    // Ambient Geomagnetic Field Parameters
    void setAmbientFieldParams(double inclinationDeg, double declinationDeg, double intensityNT);
    Point3D getAmbientFieldVector() const;
    double getInclinationDeg() const { return m_incDeg; }
    double getDeclinationDeg() const { return m_decDeg; }
    double getMagneticIntensity() const { return m_magIntensity; }

    // Reference Density
    void setReferenceDensity(double refDens) { m_refDensity = refDens; }
    double getReferenceDensity() const { return m_refDensity; }

    // Grid Maps
    const std::map<FieldComponent, Grid> &getModeledGrids() const { return m_modeledGrids; }
    const std::map<FieldComponent, Grid> &getObservedGrids() const { return m_observedGrids; }
    const std::map<FieldComponent, Grid> &getDifferenceGrids() const { return m_differenceGrids; }

    // Point Evaluation
    std::vector<Point3D> getObservationPoints(FieldComponent comp) const;

    // Grid Management
    Grid *getModeledGrid(FieldComponent comp);
    const Grid *getModeledGrid(FieldComponent comp) const;

    Grid *getObservedGrid(FieldComponent comp);
    const Grid *getObservedGrid(FieldComponent comp) const;
    void setObservedGrid(FieldComponent comp, const Grid &grid);

    Grid *getDifferenceGrid(FieldComponent comp);
    const Grid *getDifferenceGrid(FieldComponent comp) const;

    // Forward Modeling & Difference Computation
    void computeForwardField(const std::vector<Facet3Pt> &facets, bool zeroFirst = true);
    void updateDeltaField(const std::vector<Facet3Pt> &deltaFacets);
    void computeTotalFields();
    void computeDifference(FieldComponent comp, bool removeMean = false);

    // Automated 1D Inversion
    FitResult fitDensity(
        Body *pBody,
        const std::vector<Facet3Pt> &bodyFacets,
        FieldComponent comp = FieldComponent::GZ,
        double initialDensity = 2670.0,
        double tol = 1e-4,
        FitMethod method = FitMethod::Brent);

    FitResult fitVertex(
        Model &model,
        int row, int col, int index,
        FieldComponent comp = FieldComponent::GZ,
        double tol = 1e-4,
        FitMethod method = FitMethod::Brent);

private:
    void ensureGridAllocated(std::map<FieldComponent, Grid> &gridMap, FieldComponent comp);
    double getElevationAt(ObservationMode mode, double heightVal, const Grid &elevGrid, size_t r, size_t c) const;

    size_t m_rows{0};
    size_t m_cols{0};
    double m_x0{0.0};
    double m_y0{0.0};
    double m_dx{100.0};
    double m_dy{100.0};
    double m_rotDeg{0.0};

    Grid m_reliefGrid;

    ObservationMode m_grvMode{ObservationMode::SensorHeight};
    double m_grvHeight{0.0};
    Grid m_grvElevGrid;

    ObservationMode m_magMode{ObservationMode::SensorHeight};
    double m_magHeight{0.0};
    Grid m_magElevGrid;

    ObservationMode m_tensorMode{ObservationMode::SensorHeight};
    double m_tensorHeight{0.0};
    Grid m_tensorElevGrid;

    double m_incDeg{60.0};
    double m_decDeg{0.0};
    double m_magIntensity{50000.0};
    double m_refDensity{2670.0};

    std::map<FieldComponent, Grid> m_modeledGrids;
    std::map<FieldComponent, Grid> m_observedGrids;
    std::map<FieldComponent, Grid> m_differenceGrids;
};

} // namespace mod3d
