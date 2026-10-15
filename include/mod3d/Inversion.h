#pragma once

#include <functional>
#include <string>
#include <vector>

namespace mod3d {

enum class FitMethod {
    golden = 0,
    brent = 1,
    Golden = golden,
    Brent = brent
};

enum class FitCharacteristic {
    rms = 0,
    drv = 1,        // Standard deviation
    drv_local = 2,  // Local residual
    RMS = rms,
    DRV = drv,
    DRV_LOCAL = drv_local
};

struct FitIterationRecord {
    int iteration{0};
    double parameter{0.0};
    double value{0.0};
    double drv{0.0};
    double rms{0.0};
    double drv_pt{0.0};
};

struct FitResult {
    double optimal_parameter{0.0};
    double minimum_objective{0.0};
    int iterations{0};
    bool converged{false};
    double elapsed_time_sec{0.0};
    std::string log;
    std::vector<FitIterationRecord> history;
};

/**
 * @brief 1D Optimization & Geophysical Inversion Engine.
 * 
 * Preserves the authentic Golden Section and Brent parabolic
 * minimization algorithms and log formats from legacy Mod3D in modern C++20.
 */
class Inversion1D {
public:
    // Core 1D minimization algorithms
    static void bracket_minimum(
        double &ax, double &bx, double &cx,
        double &fa, double &fb, double &fc,
        const std::function<double(double)> &func,
        double glimit = 100.0);

    static double golden_section_search(
        double ax, double bx, double cx,
        const std::function<double(double)> &func,
        double tol,
        double *xmin,
        std::vector<FitIterationRecord> *history = nullptr,
        int max_iter = 100);

    static double brent_search(
        double ax, double bx, double cx,
        const std::function<double(double)> &func,
        double tol,
        double *xmin,
        std::vector<FitIterationRecord> *history = nullptr,
        int max_iter = 100);

    // High-level 1D optimization runner with bracketing and logging
    static FitResult optimize(
        double start_a, double start_b,
        const std::function<double(double)> &func,
        FitMethod method = FitMethod::brent,
        double tol = 1e-4,
        int max_iter = 100,
        const std::string &title = "1D Optimization");
};

} // namespace mod3d
