#include "mod3d/Inversion.h"
#include <cmath>
#include <algorithm>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace mod3d {

void Inversion1D::mnbrak(
    double &ax, double &bx, double &cx,
    double &fa, double &fb, double &fc,
    const std::function<double(double)> &func,
    double glimit)
{
    const double GOLD = 1.618034;
    const double TINY = 1.0e-15;

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

double Inversion1D::golden(
    double ax, double bx, double cx,
    const std::function<double(double)> &func,
    double tol,
    double *xmin,
    std::vector<FitIterationRecord> *history,
    int maxIter)
{
    const double R = 0.61803399;
    const double C = 1.0 - R;

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
    while (std::fabs(x3 - x0) > tol * (std::fabs(x1) + std::fabs(x2)) && iter < maxIter) {
        iter++;
        double currentBest = (f1 < f2 ? x1 : x2);
        double currentVal = (f1 < f2 ? f1 : f2);
        if (history) {
            history->push_back({iter, currentBest, currentVal, 0.0, currentVal, 0.0});
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

double Inversion1D::brent(
    double ax, double bx, double cx,
    const std::function<double(double)> &func,
    double tol,
    double *xmin,
    std::vector<FitIterationRecord> *history,
    int maxIter)
{
    const double CGOLD = 0.3819660;
    const double ZEPS = 1.0e-15;

    double a = (ax < cx ? ax : cx);
    double b = (ax > cx ? ax : cx);
    double x = bx, w = bx, v = bx;
    double fx = func(x);
    double fw = fx;
    double fv = fx;
    double d = 0.0;
    double e = 0.0;

    for (int iter = 1; iter <= maxIter; ++iter) {
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
    double startA, double startB,
    const std::function<double(double)> &func,
    FitMethod method,
    double tol,
    int maxIter,
    const std::string &title)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    double a = startA;
    double b = startB;
    double c = 0.0;
    double fa = 0.0, fb = 0.0, fc = 0.0;

    std::ostringstream log;
    log << title << "\n\nBracketing minimum ...\n";

    mnbrak(a, b, c, fa, fb, fc, func);

    auto bracketTime = std::chrono::high_resolution_clock::now();
    double bracketSec = std::chrono::duration<double>(bracketTime - startTime).count();

    log << "Bracketing elapsed time: " << bracketSec << "s\n\n";
    log << "Fitting ...\n";
    log << (method == FitMethod::Golden ? "Golden section method\n" : "Brent method\n");

    FitResult result;
    double minParam = 0.0;
    double minObj = 0.0;

    if (method == FitMethod::Golden) {
        minObj = golden(a, b, c, func, tol, &minParam, &result.history, maxIter);
    } else {
        minObj = brent(a, b, c, func, tol, &minParam, &result.history, maxIter);
    }

    auto finishTime = std::chrono::high_resolution_clock::now();
    result.elapsedTimeSec = std::chrono::duration<double>(finishTime - startTime).count();

    for (const auto &rec : result.history) {
        log << std::setprecision(6) << rec.parameter << " "
            << std::setprecision(10) << rec.value << "\n";
    }
    log << "Fitting elapsed time: " << result.elapsedTimeSec << "s\n";

    result.optimalParameter = minParam;
    result.minimumObjective = minObj;
    result.iterations = static_cast<int>(result.history.size());
    result.converged = (result.iterations < maxIter);
    result.log = log.str();

    return result;
}

} // namespace mod3d
