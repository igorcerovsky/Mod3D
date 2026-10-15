#include "mod3d/Inversion.h"
#include <cmath>
#include <algorithm>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace mod3d {

void Inversion1D::bracket_minimum(
    double &ax, double &bx, double &cx,
    double &fa, double &fb, double &fc,
    const std::function<double(double)> &func,
    double glimit)
{
    constexpr double GOLD = 1.618034;
    constexpr double TINY = 1.0e-15;

    fa = func(ax);
    fb = func(bx);
    if (fb > fa) {
        std::swap(ax, bx);
        std::swap(fa, fb);
    }
    cx = bx + GOLD * (bx - ax);
    fc = func(cx);

    while (fb > fc) {
        double r = (bx - ax) * (fb - fc);
        double q = (bx - cx) * (fb - fa);
        double diff = q - r;
        double denom = 2.0 * (diff >= 0.0 ? std::max(std::fabs(diff), TINY) : -std::max(std::fabs(diff), TINY));
        double u = bx - ((bx - cx) * q - (bx - ax) * r) / denom;
        double ulim = bx + glimit * (cx - bx);
        double fu = 0.0;

        if ((bx - u) * (u - cx) > 0.0) {
            fu = func(u);
            if (fu < fc) {
                ax = bx;
                bx = u;
                fa = fb;
                fb = fu;
                return;
            } else if (fu > fb) {
                cx = u;
                fc = fu;
                return;
            }
            u = cx + GOLD * (cx - bx);
            fu = func(u);
        } else if ((cx - u) * (u - ulim) > 0.0) {
            fu = func(u);
            if (fu < fc) {
                bx = cx;
                cx = u;
                u = cx + GOLD * (cx - bx);
                fb = fc;
                fc = fu;
                fu = func(u);
            }
        } else if ((u - ulim) * (ulim - cx) >= 0.0) {
            u = ulim;
            fu = func(u);
        } else {
            u = cx + GOLD * (cx - bx);
            fu = func(u);
        }

        ax = bx;
        bx = cx;
        cx = u;
        fa = fb;
        fb = fc;
        fc = fu;
    }
}

double Inversion1D::golden_section_search(
    double ax, double bx, double cx,
    const std::function<double(double)> &func,
    double tol,
    double *xmin,
    std::vector<FitIterationRecord> *history,
    int max_iter)
{
    constexpr double R = 0.61803399;
    constexpr double C = 1.0 - R;

    double x0 = ax;
    double x3 = cx;
    double x1, x2;

    if (std::fabs(cx - bx) > std::fabs(bx - ax)) {
        x1 = bx;
        x2 = bx + C * (cx - bx);
    } else {
        x2 = bx;
        x1 = bx - C * (bx - ax);
    }

    double f1 = func(x1);
    double f2 = func(x2);

    int iter = 0;
    while (std::fabs(x3 - x0) > tol * (std::fabs(x1) + std::fabs(x2)) && iter < max_iter) {
        iter++;
        double current_best = (f1 < f2 ? x1 : x2);
        double current_val = (f1 < f2 ? f1 : f2);
        if (history) {
            history->push_back({iter, current_best, current_val, 0.0, current_val, 0.0});
        }
        if (f2 < f1) {
            x0 = x1;
            x1 = x2;
            x2 = R * x1 + C * x3;
            f1 = f2;
            f2 = func(x2);
        } else {
            x3 = x2;
            x2 = x1;
            x1 = R * x2 + C * x0;
            f2 = f1;
            f1 = func(x1);
        }
    }

    if (f1 < f2) {
        if (xmin) *xmin = x1;
        return f1;
    } else {
        if (xmin) *xmin = x2;
        return f2;
    }
}

double Inversion1D::brent_search(
    double ax, double bx, double cx,
    const std::function<double(double)> &func,
    double tol,
    double *xmin,
    std::vector<FitIterationRecord> *history,
    int max_iter)
{
    constexpr double CGOLD = 0.3819660;
    constexpr double ZEPS = 1.0e-15;

    double a = (ax < cx ? ax : cx);
    double b = (ax > cx ? ax : cx);
    double x = bx, w = bx, v = bx;
    double fx = func(x);
    double fw = fx;
    double fv = fx;
    double d = 0.0;
    double e = 0.0;

    for (int iter = 1; iter <= max_iter; ++iter) {
        double xm = 0.5 * (a + b);
        double tol1 = tol * std::fabs(x) + ZEPS;
        double tol2 = 2.0 * tol1;

        if (history) {
            history->push_back({iter, x, fx, 0.0, fx, 0.0});
        }

        if (std::fabs(x - xm) <= (tol2 - 0.5 * (b - a))) {
            if (xmin) *xmin = x;
            return fx;
        }

        if (std::fabs(e) > tol1) {
            double r = (x - w) * (fx - fv);
            double q = (x - v) * (fx - fw);
            double p = (x - v) * q - (x - w) * r;
            q = 2.0 * (q - r);
            if (q > 0.0) p = -p;
            q = std::fabs(q);
            double etemp = e;
            e = d;
            if (std::fabs(p) >= std::fabs(0.5 * q * etemp) || p <= q * (a - x) || p >= q * (b - x)) {
                e = (x >= xm ? a - x : b - x);
                d = CGOLD * e;
            } else {
                d = p / q;
                double u = x + d;
                if (u - a < tol2 || b - u < tol2) {
                    d = (xm - x >= 0.0 ? tol1 : -tol1);
                }
            }
        } else {
            e = (x >= xm ? a - x : b - x);
            d = CGOLD * e;
        }

        double u = (std::fabs(d) >= tol1 ? x + d : x + (d >= 0.0 ? tol1 : -tol1));
        double fu = func(u);

        if (fu <= fx) {
            if (u >= x) a = x; else b = x;
            v = w; w = x; x = u;
            fv = fw; fw = fx; fx = fu;
        } else {
            if (u < x) a = u; else b = u;
            if (fu <= fw || w == x) {
                v = w; w = u;
                fv = fw; fw = fu;
            } else if (fu <= fv || v == x || v == w) {
                v = u;
                fv = fu;
            }
        }
    }

    if (xmin) *xmin = x;
    return fx;
}

FitResult Inversion1D::optimize(
    double start_a, double start_b,
    const std::function<double(double)> &func,
    FitMethod method,
    double tol,
    int max_iter,
    const std::string &title)
{
    auto start_time = std::chrono::high_resolution_clock::now();

    double a = start_a;
    double b = start_b;
    double c = 0.0;
    double fa = 0.0, fb = 0.0, fc = 0.0;

    std::ostringstream log;
    log << title << "\n\nBracketing minimum ...\n";

    bracket_minimum(a, b, c, fa, fb, fc, func);

    auto bracket_time = std::chrono::high_resolution_clock::now();
    double bracket_sec = std::chrono::duration<double>(bracket_time - start_time).count();

    log << "Bracketing elapsed time: " << bracket_sec << "s\n\n";
    log << "Fitting ...\n";
    log << (method == FitMethod::golden ? "Golden section method\n" : "Brent method\n");

    FitResult result;
    double min_param = 0.0;
    double min_obj = 0.0;

    if (method == FitMethod::golden) {
        min_obj = golden_section_search(a, b, c, func, tol, &min_param, &result.history, max_iter);
    } else {
        min_obj = brent_search(a, b, c, func, tol, &min_param, &result.history, max_iter);
    }

    auto finish_time = std::chrono::high_resolution_clock::now();
    result.elapsed_time_sec = std::chrono::duration<double>(finish_time - start_time).count();

    for (const auto &rec : result.history) {
        log << std::setprecision(6) << rec.parameter << " "
            << std::setprecision(10) << rec.value << "\n";
    }
    log << "Fitting elapsed time: " << result.elapsed_time_sec << "s\n";

    result.optimal_parameter = min_param;
    result.minimum_objective = min_obj;
    result.iterations = static_cast<int>(result.history.size());
    result.converged = (result.iterations < max_iter);
    result.log = log.str();

    return result;
}

} // namespace mod3d
