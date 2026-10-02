#include "mod3d/PotField.h"
#include "pfld/facet.hpp"
#include <cmath>
#include <span>

namespace mod3d {

namespace {

template<class T>
inline T sign(T d) {
    return ((d == 0) ? 0.0 : ((d < 0) ? -1.0 : 1.0));
}

} // namespace

void FacetField(Point3D &v_r, Point3D *pts, int n, Point3D v_M, Point3D &v_Mag, Point3D &v_Grv, Formula nFormula)
{
    if (nFormula == Formula::POHANKA) {
        v_Grv += FacetPohanka(v_r, pts, n);
        return;
    }
    if (nFormula == Formula::GUPTASARMA_SINGH) {
        FacetGS(v_r, pts, n, v_M, v_Mag, v_Grv);
    }
}

void FacetField(Point3D *pts, int n, Point3D v_M, Point3D &v_Mag, Point3D &v_Grv, Formula nFormula)
{
    if (nFormula == Formula::POHANKA) {
        v_Grv += FacetPohanka(pts, n);
        return;
    }
    if (nFormula == Formula::GUPTASARMA_SINGH) {
        FacetGS(pts, n, v_M, v_Mag, v_Grv);
    }
}

// Singh & Guptasarma 1999, 2000 Geophysics
void FacetGS(Point3D &v_r, Point3D *pts, int n, Point3D v_M, Point3D &v_Mag, Point3D &v_Grv)
{
    Point3D spts[10];
    Point3D shf = v_r * (-1.0);
    for (int i = 0; i < n; i++) {
        spts[i] = pts[i] + shf;
    }
    FacetGS(spts, n, v_M, v_Mag, v_Grv);
}

void FacetGS(Point3D *pts, int n, Point3D v_M, Point3D &v_Mag, Point3D &v_Grv)
{
    Point3D v_L;
    Point3D v_r;
    Point3D v_u;
    double r, L, b, I, h;
    double P = 0.0, Q = 0.0, R = 0.0;
    double s, d;
    double dOmega;

    v_u = (pts[0] - pts[1]) / (pts[1] - pts[2]);
    v_u.Unit();
    dOmega = SolidAngle(pts, n, v_u);

    if (dOmega != 0.0) {
        for (int i = 0; i < n; i++) {
            v_r = pts[i];
            if (i < (n - 1)) {
                v_L = pts[i + 1] - pts[i];
            } else {
                v_L = pts[0] - pts[i];
            }
            r = pts[i].Abs();
            L = v_L.Abs();
            b = 2.0 * (v_r * v_L);
            h = r + b / (2.0 * L);
            if (h != 0.0) {
                I = (1.0 / L) * std::log((std::sqrt(L * L + b + r * r) + L + b / (2.0 * L)) / h);
            } else {
                I = (1.0 / L) * std::log(std::fabs(L - r) / r);
            }
            P += I * v_L.x;
            Q += I * v_L.y;
            R += I * v_L.z;
        }

        Point3D v_f;
        v_f.x = dOmega * v_u.x + Q * v_u.z - R * v_u.y;
        v_f.y = dOmega * v_u.y + R * v_u.x - P * v_u.z;
        v_f.z = dOmega * v_u.z + P * v_u.y - Q * v_u.x;

        // Magnetic field
        s = v_M * v_u;
        v_Mag += v_f * s;

        // Gravity field (6.67e-8 in CGS, kappa*rho = 1000 kg/m3)
        d = pts[0] * v_u;
        v_Grv += v_f * d * 6.67e-8;
    }
}

// Pohanka polygon field
static void PohankaInit(Point3D *pts, int n, Point3D &v_n, Point3D *v_mi, Point3D *v_ni, double *len)
{
    v_n = (pts[2] - pts[1]) / (pts[1] - pts[0]);
    v_n.Unit();

    for (int i = 0; i < n; i++) {
        if (i != n - 1) {
            v_mi[i] = pts[i + 1] - pts[i];
            len[i] = v_mi[i].Abs();
            v_mi[i].Unit();
        } else {
            v_mi[i] = pts[0] - pts[i];
            len[i] = v_mi[i].Abs();
            v_mi[i].Unit();
        }
        v_ni[i] = v_mi[i] / v_n;
    }
}

Point3D FacetPohanka(Point3D &v_r, Point3D *pts, int n)
{
    Point3D v_n;
    Point3D v_mi[10], v_ni[10];
    double len[10];

    PohankaInit(pts, n, v_n, v_mi, v_ni, len);

    double z, u, v, w, W2, W, U, V, T, f = 0.0;
    Point3D *v_a = &pts[0];
    z = std::fabs(v_n * (*v_a - v_r));

    for (int i = 0; i < n; i++) {
        u = v_mi[i] * (pts[i] - v_r);
        v = u + len[i];
        w = v_ni[i] * (pts[i] - v_r);

        z = z + EPS_VAL;
        W2 = w * w + z * z;
        U = std::sqrt(u * u + W2);
        V = std::sqrt(v * v + W2);
        W = std::sqrt(W2);
        T = U + V;
        f += w * (sign(v) * std::log((V + std::fabs(v)) / W) - sign(u) * std::log((U + std::fabs(u)) / W)) -
             2.0 * z * std::atan((2.0 * w * len[i]) / ((T + len[i]) * std::fabs(T - len[i]) + 2.0 * T * z));
    }
    f *= 6.67e-8; // kappa*rho=1000 kg/m3

    return Point3D(f * v_n.x, f * v_n.y, f * v_n.z);
}

Point3D FacetPohanka(Point3D *pts, int n)
{
    Point3D v_n;
    Point3D v_mi[10], v_ni[10];
    double len[10];

    PohankaInit(pts, n, v_n, v_mi, v_ni, len);

    double z, u, v, w, W2, W, U, V, T, f = 0.0;
    Point3D *v_a = &pts[0];
    z = std::fabs(*v_a * v_n);

    for (int i = 0; i < n; i++) {
        u = v_mi[i] * pts[i];
        v = u + len[i];
        w = v_ni[i] * pts[i];

        z = z + EPS_VAL;
        W2 = w * w + z * z;
        U = std::sqrt(u * u + W2);
        V = std::sqrt(v * v + W2);
        W = std::sqrt(W2);
        T = U + V;
        f += w * (sign(v) * std::log((V + std::fabs(v)) / W) - sign(u) * std::log((U + std::fabs(u)) / W)) -
             2.0 * z * std::atan((2.0 * w * len[i]) / ((T + len[i]) * std::fabs(T - len[i]) + 2.0 * T * z));
    }
    f *= 6.67e-8;

    return Point3D(f * v_n.x, f * v_n.y, f * v_n.z);
}

static void PohankaInitPnt(Point3D **pts, int n, Point3D &v_n, Point3D *v_mi, Point3D *v_ni, double *len)
{
    v_n = (*pts[2] - *pts[1]) / (*pts[1] - *pts[0]);
    v_n.Unit();

    for (int i = 0; i < n; i++) {
        if (i != n - 1) {
            v_mi[i] = *pts[i + 1] - *pts[i];
            len[i] = v_mi[i].Abs();
            v_mi[i].Unit();
        } else {
            v_mi[i] = *pts[0] - *pts[i];
            len[i] = v_mi[i].Abs();
            v_mi[i].Unit();
        }
        v_ni[i] = v_mi[i] / v_n;
    }
}

Point3D FacetPohankaPnt(Point3D &v_r, Point3D **pts, int n)
{
    Point3D v_n;
    Point3D v_mi[10], v_ni[10];
    double len[10];

    PohankaInitPnt(pts, n, v_n, v_mi, v_ni, len);

    double z, u, v, w, W2, W, U, V, T, f = 0.0;
    Point3D *v_a = pts[0];
    z = std::fabs(v_n * (*v_a - v_r));

    for (int i = 0; i < n; i++) {
        u = v_mi[i] * (*pts[i] - v_r);
        v = u + len[i];
        w = v_ni[i] * (*pts[i] - v_r);

        z = z + EPS_VAL;
        W2 = w * w + z * z;
        U = std::sqrt(u * u + W2);
        V = std::sqrt(v * v + W2);
        W = std::sqrt(W2);
        T = U + V;
        f += w * (sign(v) * std::log((V + std::fabs(v)) / W) - sign(u) * std::log((U + std::fabs(u)) / W)) -
             2.0 * z * std::atan((2.0 * w * len[i]) / ((T + len[i]) * std::fabs(T - len[i]) + 2.0 * T * z));
    }
    f *= 6.67e-8;

    return Point3D(f * v_n.x, f * v_n.y, f * v_n.z);
}

Point3D FacetPohankaPnt(Point3D **pts, int n)
{
    Point3D v_n;
    Point3D v_mi[10], v_ni[10];
    double len[10];

    PohankaInitPnt(pts, n, v_n, v_mi, v_ni, len);

    double z, u, v, w, W2, W, U, V, T, f = 0.0;
    Point3D *v_a = pts[0];
    z = std::fabs(v_n * *v_a);

    for (int i = 0; i < n; i++) {
        u = v_mi[i] * *pts[i];
        v = u + len[i];
        w = v_ni[i] * *pts[i];

        z = z + EPS_VAL;
        W2 = w * w + z * z;
        U = std::sqrt(u * u + W2);
        V = std::sqrt(v * v + W2);
        W = std::sqrt(W2);
        T = U + V;
        f += w * (sign(v) * std::log((V + std::fabs(v)) / W) - sign(u) * std::log((U + std::fabs(u)) / W)) -
             2.0 * z * std::atan((2.0 * w * len[i]) / ((T + len[i]) * std::fabs(T - len[i]) + 2.0 * T * z));
    }
    f *= 6.67e-8;

    return Point3D(f * v_n.x, f * v_n.y, f * v_n.z);
}

Point3D AmbientField(double iI, double iD, double iS)
{
    Point3D af;
    iI *= PI_VAL / 180.0;
    iD *= PI_VAL / 180.0;

    af.y = std::cos(iI) * std::cos(iD);
    af.x = std::cos(iI) * std::sin(iD);
    af.z = -std::sin(iI);
    af = af * iS;

    return af;
}

Point3D MagnetizationVector(double susc, double iI, double iD, double iS, double rI, double rD, double rS)
{
    Point3D vH, vI, vR;

    vH = AmbientField(iI, iD, iS);
    vI = vH * (susc / (4.0 * PI_VAL));

    rI *= PI_VAL / 180.0;
    rD *= PI_VAL / 180.0;
    vR.x = std::cos(rI) * std::cos(rD);
    vR.y = std::cos(rI) * std::sin(rD);
    vR.z = -std::sin(rI);
    vR = vR * rS;

    return (vI + vR);
}

double Mag_dT(Point3D v_mag, Point3D v_af, double hInt)
{
    double fMagTot = std::sqrt(std::pow(v_mag.x + v_af.x, 2) +
                               std::pow(v_mag.y + v_af.y, 2) +
                               std::pow(v_mag.z + v_af.z, 2));
    return (fMagTot - hInt);
}

double Mag_dT(double mx, double my, double mz, double afx, double afy, double afz, double hInt)
{
    double fMagTot = std::sqrt(std::pow(mx + afx, 2) +
                               std::pow(my + afy, 2) +
                               std::pow(mz + afz, 2));
    return (fMagTot - hInt);
}

double SolidAngle(Point3D *pts, int n, Point3D &v_u)
{
    if (pts == nullptr || n < 3) return 0.0;
    return pfld::Facet<double>::SolidAngle(std::span<const Point3D>(pts, n), v_u * pts[1], n);
}

} // namespace mod3d

