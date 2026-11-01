#pragma once

#include <cmath>
#include <functional>
#include <limits>
#include <string>
#include <vector>

namespace mod3d {

enum class FitMethod {
    golden = 0,
    brent = 1,
    linear = 2,
    Golden = golden,
    Brent = brent,
    Linear = linear
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

    // Bounded 1D minimization (strictly confined to [ax, bx], no bracketing required)
    static double golden_section_bounded(
        double ax, double bx,
        const std::function<double(double)> &func,
        double tol,
        double *xmin,
        std::vector<FitIterationRecord> *history = nullptr,
        int max_iter = 100);

    static double brent_bounded(
        double ax, double bx,
        const std::function<double(double)> &func,
        double tol,
        double *xmin,
        std::vector<FitIterationRecord> *history = nullptr,
        int max_iter = 100,
        double initial_guess = std::numeric_limits<double>::quiet_NaN());

    static FitResult optimize_bounded(
        double bound_min, double bound_max,
        const std::function<double(double)> &func,
        double initial_guess = std::numeric_limits<double>::quiet_NaN(),
        FitMethod method = FitMethod::brent,
        double tol = 1e-4,
        int max_iter = 100,
        const std::string &title = "1D Bounded Optimization");
};

} // namespace mod3d
