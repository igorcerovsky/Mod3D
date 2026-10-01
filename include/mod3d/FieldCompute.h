#pragma once

#include "mod3d/Facet3Pt.h"
#include "mod3d/Point3D.h"
#include <vector>
#include <cstddef>

namespace mod3d {

/**
 * @brief High-performance potential field computation engine.
 * 
 * Supports both serial and multi-threaded parallel computation of
 * gravitational (vector G and scalar Gz) and magnetic fields across
 * observation point grids, matching the facet engine implementation in pfld_app.
 */
class FieldCompute {
public:
    // Serial Gz computation across points
    static void ComputeGzSerial(
        const std::vector<Facet3Pt> &facets,
        const std::vector<Point3D> &points,
        std::vector<double> &outGz);

    // Multi-threaded parallel Gz computation across points
    static void ComputeGzParallel(
        const std::vector<Facet3Pt> &facets,
        const std::vector<Point3D> &points,
        std::vector<double> &outGz,
        size_t numThreads = 0);

    // Serial full 3D gravity vector (Gx, Gy, Gz) computation across points
    static void ComputeGSerial(
        const std::vector<Facet3Pt> &facets,
        const std::vector<Point3D> &points,
        std::vector<Point3D> &outG);

    // Multi-threaded parallel 3D gravity vector computation across points
    static void ComputeGParallel(
        const std::vector<Facet3Pt> &facets,
        const std::vector<Point3D> &points,
        std::vector<Point3D> &outG,
        size_t numThreads = 0);

    // Parallel Gz computation with linear density gradient
    static void ComputeGzLinearParallel(
        const std::vector<Facet3Pt> &facets,
        const std::vector<Point3D> &points,
        const Point3D &gradRho,
        double rho0,
        std::vector<double> &outGz,
        size_t numThreads = 0);
};

} // namespace mod3d
