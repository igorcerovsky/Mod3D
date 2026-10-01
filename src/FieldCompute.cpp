#include "mod3d/FieldCompute.h"
#include <thread>
#include <algorithm>
#include <cassert>

namespace mod3d {

namespace {

size_t DetermineThreadCount(size_t numPoints, size_t requestedThreads) {
    if (requestedThreads > 0) {
        return std::min(requestedThreads, std::max(size_t(1), numPoints));
    }
    const unsigned int hw = std::thread::hardware_concurrency();
    const size_t available = (hw > 0) ? hw : 2;
    const size_t minPerThread = 25;
    const size_t maxThreads = (numPoints + minPerThread - 1) / minPerThread;
    return std::max(size_t(1), std::min(available, maxThreads));
}

} // anonymous namespace

void FieldCompute::ComputeGzSerial(
    const std::vector<Facet3Pt> &facets,
    const std::vector<Point3D> &points,
    std::vector<double> &outGz)
{
    outGz.assign(points.size(), 0.0);
    for (size_t p = 0; p < points.size(); ++p) {
        double gz = 0.0;
        for (const auto &fct : facets) {
            fct.Fld_Gz(points[p], gz);
        }
        outGz[p] = gz;
    }
}

void FieldCompute::ComputeGzParallel(
    const std::vector<Facet3Pt> &facets,
    const std::vector<Point3D> &points,
    std::vector<double> &outGz,
    size_t numThreads)
{
    const size_t nPts = points.size();
    outGz.assign(nPts, 0.0);
    if (nPts == 0) return;

    const size_t threads = DetermineThreadCount(nPts, numThreads);
    if (threads <= 1) {
        ComputeGzSerial(facets, points, outGz);
        return;
    }

    const size_t chunkSize = (nPts + threads - 1) / threads;
    std::vector<std::thread> workers;
    workers.reserve(threads);

    for (size_t t = 0; t < threads; ++t) {
        const size_t start = t * chunkSize;
        const size_t end = std::min(start + chunkSize, nPts);
        if (start >= end) break;

        workers.emplace_back([&facets, &points, &outGz, start, end]() {
            for (size_t p = start; p < end; ++p) {
                double gz = 0.0;
                for (const auto &fct : facets) {
                    fct.Fld_Gz(points[p], gz);
                }
                outGz[p] = gz;
            }
        });
    }

    for (auto &w : workers) {
        if (w.joinable()) {
            w.join();
        }
    }
}

void FieldCompute::ComputeGSerial(
    const std::vector<Facet3Pt> &facets,
    const std::vector<Point3D> &points,
    std::vector<Point3D> &outG)
{
    outG.assign(points.size(), Point3D(0.0, 0.0, 0.0));
    for (size_t p = 0; p < points.size(); ++p) {
        Point3D g(0.0, 0.0, 0.0);
        for (const auto &fct : facets) {
            fct.Fld_G(points[p], g);
        }
        outG[p] = g;
    }
}

void FieldCompute::ComputeGParallel(
    const std::vector<Facet3Pt> &facets,
    const std::vector<Point3D> &points,
    std::vector<Point3D> &outG,
    size_t numThreads)
{
    const size_t nPts = points.size();
    outG.assign(nPts, Point3D(0.0, 0.0, 0.0));
    if (nPts == 0) return;

    const size_t threads = DetermineThreadCount(nPts, numThreads);
    if (threads <= 1) {
        ComputeGSerial(facets, points, outG);
        return;
    }

    const size_t chunkSize = (nPts + threads - 1) / threads;
    std::vector<std::thread> workers;
    workers.reserve(threads);

    for (size_t t = 0; t < threads; ++t) {
        const size_t start = t * chunkSize;
        const size_t end = std::min(start + chunkSize, nPts);
        if (start >= end) break;

        workers.emplace_back([&facets, &points, &outG, start, end]() {
            for (size_t p = start; p < end; ++p) {
                Point3D g(0.0, 0.0, 0.0);
                for (const auto &fct : facets) {
                    fct.Fld_G(points[p], g);
                }
                outG[p] = g;
            }
        });
    }

    for (auto &w : workers) {
        if (w.joinable()) {
            w.join();
        }
    }
}

void FieldCompute::ComputeGzLinearParallel(
    const std::vector<Facet3Pt> &facets,
    const std::vector<Point3D> &points,
    const Point3D &gradRho,
    double rho0,
    std::vector<double> &outGz,
    size_t numThreads)
{
    const size_t nPts = points.size();
    outGz.assign(nPts, 0.0);
    if (nPts == 0) return;

    const size_t threads = DetermineThreadCount(nPts, numThreads);
    if (threads <= 1) {
        for (size_t p = 0; p < nPts; ++p) {
            double gz = 0.0;
            for (const auto &fct : facets) {
                fct.Fld_Gz(points[p], gradRho, rho0, gz);
            }
            outGz[p] = gz;
        }
        return;
    }

    const size_t chunkSize = (nPts + threads - 1) / threads;
    std::vector<std::thread> workers;
    workers.reserve(threads);

    for (size_t t = 0; t < threads; ++t) {
        const size_t start = t * chunkSize;
        const size_t end = std::min(start + chunkSize, nPts);
        if (start >= end) break;

        workers.emplace_back([&facets, &points, &outGz, gradRho, rho0, start, end]() {
            for (size_t p = start; p < end; ++p) {
                double gz = 0.0;
                for (const auto &fct : facets) {
                    fct.Fld_Gz(points[p], gradRho, rho0, gz);
                }
                outGz[p] = gz;
            }
        });
    }

    for (auto &w : workers) {
        if (w.joinable()) {
            w.join();
        }
    }
}

} // namespace mod3d
