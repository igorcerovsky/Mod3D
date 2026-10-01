#pragma once

#include <functional>
#include <string>
#include <vector>

namespace mod3d {

enum class FitMethod {
    Golden = 0,
    Brent = 1
};

enum class FitCharacteristic {
    RMS = 0,
    DRV = 1,        // Standard deviation
    DRV_LOCAL = 2   // Local residual
};

struct FitIterationRecord {
    int iteration{0};
    double parameter{0.0};
    double value{0.0};
    double drv{0.0};
    double rms{0.0};
    double drvPt{0.0};
};

struct FitResult {
    double optimalParameter{0.0};
    double minimumObjective{0.0};
    int iterations{0};
    bool converged{false};
    double elapsedTimeSec{0.0};
    std::string log;
    std::vector<FitIterationRecord> history;
};

/**
 * @brief 1D Optimization & Geophysical Inversion Engine.
 * 
 * Faithfully preserves the authentic Golden Section and Brent parabolic
 * minimization algorithms and log formats from legacy Mod3D.
 */
class Inversion1D {
public:
    // Core 1D minimization algorithms
    static void mnbrak(
        double &ax, double &bx, double &cx,
        double &fa, double &fb, double &fc,
        const std::function<double(double)> &func,
        double glimit = 100.0);

    static double golden(
        double ax, double bx, double cx,
        const std::function<double(double)> &func,
        double tol,
        double *xmin,
        std::vector<FitIterationRecord> *history = nullptr,
        int maxIter = 100);

    static double brent(
        double ax, double bx, double cx,
        const std::function<double(double)> &func,
        double tol,
        double *xmin,
        std::vector<FitIterationRecord> *history = nullptr,
        int maxIter = 100);

    // High-level 1D optimization runner with bracketing and logging
    static FitResult optimize(
        double startA, double startB,
        const std::function<double(double)> &func,
        FitMethod method = FitMethod::Brent,
        double tol = 1e-4,
        int maxIter = 100,
        const std::string &title = "1D Optimization");
};

} // namespace mod3d
