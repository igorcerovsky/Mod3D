#pragma once

#include "mod3d/Facet3Pt.h"
#include "mod3d/Point3D.h"
#include "pfld/pfld_compute.hpp"
#include <vector>
#include <cstddef>

namespace mod3d {

/**
 * @brief High-performance potential field computation engine.
 * Header-only wrapper around the modern pfld parallel compute routines.
 */
class FieldCompute {
public:
    // Serial Gz computation across points
    static void ComputeGzSerial(
        const std::vector<Facet3Pt> &facets,
        const std::vector<Point3D> &points,
        std::vector<double> &outGz)
    {
        pfld::Field_Gz_serial(facets, points, outGz);
    }

    // Multi-threaded parallel Gz computation across points
    static void ComputeGzParallel(
        const std::vector<Facet3Pt> &facets,
        const std::vector<Point3D> &points,
        std::vector<double> &outGz,
        size_t numThreads = 0)
    {
        pfld::Field_Gz(facets, points, outGz, numThreads);
    }

    // Serial full 3D gravity vector (Gx, Gy, Gz) computation across points
    static void ComputeGSerial(
        const std::vector<Facet3Pt> &facets,
        const std::vector<Point3D> &points,
        std::vector<Point3D> &outG)
    {
        pfld::Field_G(facets, points, outG);
    }

    // Multi-threaded parallel 3D gravity vector computation across points
    static void ComputeGParallel(
        const std::vector<Facet3Pt> &facets,
        const std::vector<Point3D> &points,
        std::vector<Point3D> &outG,
        size_t numThreads = 0)
    {
        pfld::Field_G_parallel(facets, points, outG, numThreads);
    }

    // Parallel Gz computation with linear density gradient
    static void ComputeGzLinearParallel(
        const std::vector<Facet3Pt> &facets,
        const std::vector<Point3D> &points,
        const Point3D &gradRho,
        double rho0,
        std::vector<double> &outGz,
        size_t numThreads = 0)
    {
        pfld::Field_Gz_linear_parallel(facets, points, gradRho, rho0, outGz, numThreads);
    }
};

} // namespace mod3d

