#include "mod3d/Facet3Pt.h"
#include "mod3d/Body.h"
#include "mod3d/PotField.h"
#include <cmath>
#include <algorithm>

namespace mod3d {

namespace {

template<class T>
inline T sign(T d) {
    return ((d == 0) ? 0.0 : ((d < 0) ? -1.0 : 1.0));
}

inline Point3D sph(const Point3D &pt, const Point3D &shf) {
    constexpr double ER = 6375000.0;
    Point3D sPt = pt + shf;
    double r = sPt.z + ER;
    double ro = std::sqrt(sPt.x * sPt.x + sPt.y * sPt.y);
    if (ro != 0.0) {
        double th = ro / r;
        double l = r * std::sin(th);
        sPt.x = (l / ro) * sPt.x;
        sPt.y = (l / ro) * sPt.y;
        sPt.z = sPt.z - r + r * std::cos(th);
    }
    return sPt;
}

} // namespace

Facet3Pt::Facet3Pt()
    : nType(FacetType::FCT_NORMAL), density(1000.0), densityOpos(0.0), dSign(1.0),
      pBody(nullptr), pBodyOpos(nullptr)
{
}

Facet3Pt::Facet3Pt(const Point3D &pt0, const Point3D &pt1, const Point3D &pt2)
    : nType(FacetType::FCT_NORMAL), density(1000.0), densityOpos(0.0), dSign(1.0),
      pBody(nullptr), pBodyOpos(nullptr)
{
    Init(pt0, pt1, pt2);
}

Facet3Pt::Facet3Pt(const Point3D &pt0, const Point3D &pt1, const Point3D &pt2, double densityCCW, double densityCW)
    : nType(FacetType::FCT_NORMAL), density(densityCCW), densityOpos(densityCW), dSign(1.0),
      pBody(nullptr), pBodyOpos(nullptr)
{
    Init(pt0, pt1, pt2, densityCCW, densityCW);
}

void Facet3Pt::Init(const Point3D &pt0, const Point3D &pt1, const Point3D &pt2)
{
    pts[0] = pt0;
    pts[1] = pt1;
    pts[2] = pt2;
    Init();
}

void Facet3Pt::Init(const Point3D &pt0, const Point3D &pt1, const Point3D &pt2, double densityCCW, double densityCW)
{
    pts[0] = pt0;
    pts[1] = pt1;
    pts[2] = pt2;
    density = densityCCW;
    densityOpos = densityCW;
    Init();
}

void Facet3Pt::Init(const Point3D &pt0, const Point3D &pt1, const Point3D &pt2, double densityCCW, Point3D v_densGradCCW)
{
    pts[0] = pt0;
    pts[1] = pt1;
    pts[2] = pt2;
    density = densityCCW;
    v_densGrad = v_densGradCCW;
    Init();
}

void Facet3Pt::Init(const Point3D *ppts)
{
    pts[0] = ppts[0];
    pts[1] = ppts[1];
    pts[2] = ppts[2];
    Init();
}

void Facet3Pt::Init(const std::vector<Point3D> &ppts)
{
    if (ppts.size() >= 3) {
        pts[0] = ppts[0];
        pts[1] = ppts[1];
        pts[2] = ppts[2];
        Init();
    }
}

void Facet3Pt::SetOpositDensity(double dDensity, Point3D v_grad)
{
    densityOpos = dDensity;
    v_densGradOpos = v_grad;
    bLinOpos = v_densGradOpos.IsZero();
}

void Facet3Pt::Init()
{
    // Outward unit normal vector (for triangle in CCW order)
    v_n = (pts[0] - pts[1]) / (pts[1] - pts[2]);
    v_n.Unit();

    int n = 3;
    for (int i = 0; i < n; i++) {
        if (i != n - 1) {
            v_mi[i] = pts[i + 1] - pts[i];
            v_L[i] = v_mi[i];
            len[i] = v_mi[i].Abs();
            v_mi[i].Unit();
        } else {
            v_mi[i] = pts[0] - pts[i];
            v_L[i] = v_mi[i];
            len[i] = v_mi[i].Abs();
            v_mi[i].Unit();
        }
        v_ni[i] = v_mi[i] / v_n;
    }

    bLin = v_densGrad.IsZero();
    bLinOpos = v_densGradOpos.IsZero();
}

// Gravity field of a polygonal facet (Pohanka / Vlado) with constant density
void Facet3Pt::FldVlado(const Point3D &v_r, Point3D &v_Grv) const
{
    int n = 3;
    double z, u, v, w, W2, W, U, V, T, f = 0.0;

    const Point3D *v_a = &pts[0];
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
    f *= GRAV_CONST;

    v_Grv += v_n * f;
}

// Gravity field for variable (linear) density gradient
void Facet3Pt::FldVlado(const Point3D &v_r, Point3D &v_Grv, Point3D ro, double ro0) const
{
    int n = 3;
    double z, u, v, w, W2, W, U, V, T, L, A, Fi, Fi2, d, Z;
    Point3D f(0, 0, 0);

    const Point3D *v_a = &pts[0];
    Z = v_n * (*v_a - v_r);
    z = std::fabs(Z);

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
        d = len[i];
        A = -std::atan((2.0 * w * d) / ((T + d) * std::fabs(T - d) + 2.0 * T * z));
        if (sign(u) == sign(v)) {
            L = sign(v) * std::log((V + std::fabs(v)) / (U + std::fabs(u)));
        } else {
            L = std::log((V + std::fabs(v)) * (U + std::fabs(u)) / (W * W));
        }
        Fi = w * L + 2.0 * z * A;
        Fi2 = (d / 4.0) * ((v + u) * (v + u) / T + T) + W * W * L / 2.0;
        f += v_n * (Fi * (ro0 + ro * v_r + ro * v_n * Z) + ro * v_ni[i] * Fi2) - ro * (Fi * Z / 2.0);
    }
    f = f * GRAV_CONST;
    v_Grv += f;
}

// Full gravity gradient tensor
void Facet3Pt::FldVladoGrd(const Point3D &v_r, double refDensity,
                           double &gxx, double &gyy, double &gzz,
                           double &gxy, double &gxz, double &gyz) const
{
    int n = 3;
    double z, u, v, w, W2, W, U, V, T, L, A, d, Z, e;
    double dens;

    const Point3D *v_a = &pts[0];
    Z = v_n * (*v_a - v_r);
    z = std::fabs(Z);
    e = sign(Z);

    Point3D tmpFldGrd(0, 0, 0);
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
        d = len[i];
        A = -std::atan((2.0 * w * d) / (T * T - (v - u) * (v - u) + 2.0 * T * z));
        if (sign(u) == sign(v)) {
            L = sign(v) * std::log((V + std::fabs(v)) / (U + std::fabs(u)));
        } else {
            L = std::log((V + std::fabs(v)) * (U + std::fabs(u)) / (W * W));
        }
        tmpFldGrd += v_ni[i] * L + v_n * 2.0 * e * A;
    }
    tmpFldGrd = tmpFldGrd * GRAV_CONST;

    if (densityOpos != 0.0)
        dens = density - densityOpos;
    else
        dens = density - refDensity;

    gxx = dens * tmpFldGrd.x * v_n.x;
    gyy = dens * tmpFldGrd.y * v_n.y;
    gzz = dens * tmpFldGrd.z * v_n.z;
    gyz = dens * 0.5 * (tmpFldGrd.y * v_n.z + tmpFldGrd.z * v_n.y);
    gxy = dens * 0.5 * (tmpFldGrd.x * v_n.y + tmpFldGrd.y * v_n.x);
    gxz = dens * 0.5 * (tmpFldGrd.x * v_n.z + tmpFldGrd.z * v_n.x);
}

void Facet3Pt::FldGS(const Point3D &v_r, Point3D v_M, Point3D &v_Mag, Point3D &v_Grv) const
{
    int n = 3;
    Point3D spts[3];
    Point3D shf = v_r * (-1.0);
    for (int i = 0; i < n; i++) {
        spts[i] = pts[i] + shf;
    }

    Point3D v_rr;
    double r, L, b, I, h;
    double P = 0.0, Q = 0.0, R = 0.0;
    double s, d;
    double dOmega;

    dOmega = SolidAngle(spts);
    if (dOmega == 0.0) return;

    for (int i = 0; i < n; i++) {
        v_rr = spts[i];
        r = spts[i].Abs();
        L = len[i];
        b = 2.0 * (v_rr * v_L[i]);
        h = r + b / (2.0 * L);
        if (h != 0.0)
            I = (1.0 / L) * std::log((std::sqrt(L * L + b + r * r) + L + b / (2.0 * L)) / h);
        else
            I = (1.0 / L) * std::log(std::fabs(L - r) / r);

        P += I * v_L[i].x;
        Q += I * v_L[i].y;
        R += I * v_L[i].z;
    }

    Point3D v_f;
    v_f.x = dOmega * v_n.x + Q * v_n.z - R * v_n.y;
    v_f.y = dOmega * v_n.y + R * v_n.x - P * v_n.z;
    v_f.z = dOmega * v_n.z + P * v_n.y - Q * v_n.x;

    s = v_M * v_n;
    v_Mag += v_f * s;

    d = spts[0] * v_n;
    v_Grv += v_f * d * GRAV_CONST;
}

void Facet3Pt::FldGS(const Point3D &v_r, Point3D v_M, Point3D &v_Mag, double dSignMultiplier) const
{
    int n = 3;
    Point3D spts[3];
    Point3D shf = v_r * (-1.0);
    for (int i = 0; i < n; i++) {
        spts[i] = pts[i] + shf;
    }

    Point3D v_rr;
    double r, L, b, I, h;
    double P = 0.0, Q = 0.0, R = 0.0;
    double s;
    double dOmega;

    dOmega = SolidAngle(spts);

    for (int i = 0; i < n; i++) {
        v_rr = spts[i];
        r = spts[i].Abs();
        L = len[i];
        b = 2.0 * (v_rr * v_L[i]);
        h = r + b / (2.0 * L);
        if (h != 0.0)
            I = (1.0 / L) * std::log((std::sqrt(L * L + b + r * r) + L + b / (2.0 * L)) / h);
        else
            I = (1.0 / L) * std::log(std::fabs(L - r) / r);

        P += I * v_L[i].x;
        Q += I * v_L[i].y;
        R += I * v_L[i].z;
    }

    Point3D v_f;
    v_f.x = dOmega * v_n.x + Q * v_n.z - R * v_n.y;
    v_f.y = dOmega * v_n.y + R * v_n.x - P * v_n.z;
    v_f.z = dOmega * v_n.z + P * v_n.y - Q * v_n.x;

    s = dSignMultiplier * (v_M * v_n);
    v_Mag += v_f * s;
}

double Facet3Pt::SolidAngle(const Point3D *spts) const
{
    double Omega = 0.0;
    int n = 3;
    double dInOut = v_n * spts[1];
    if (dInOut == 0.0)
        return 0.0;

    const Point3D *p1 = nullptr, *p2 = nullptr, *p3 = nullptr, *p = nullptr;
    double dFi = 0.0, a, b;
    for (int i = 0; i < n; i++) {
        if (i == 0) {
            p1 = &spts[n - 1];
            p2 = &spts[0];
            p3 = &spts[1];
        } else if (i < (n - 1)) {
            p1 = &spts[i - 1];
            p2 = &spts[i];
            p3 = &spts[i + 1];
        } else {
            p1 = &spts[i - 1];
            p2 = &spts[i];
            p3 = &spts[0];
        }
        if (dInOut > 0.0) {
            p = p1;
            p1 = p3;
            p3 = p;
        }
        Point3D n1 = *p2 / *p1;
        n1.Unit();
        Point3D n2 = *p3 / *p2;
        n2.Unit();
        double dPerp = *p3 * n1;
        b = n1 * n2;
        if (b < -1.0) b = -1.0;
        if (b > 1.0) b = 1.0;
        a = PI_VAL - std::acos(b);
        if (dPerp < 0.0) {
            a = 2.0 * PI_VAL - a;
        }
        dFi += a;
    }
    Omega = dFi - (n - 2) * PI_VAL;
    if (dInOut > 0.0)
        Omega = -Omega;

    return Omega;
}

void Facet3Pt::FldSpherVlado(const Point3D &v_r, Point3D &v_Grv) const
{
    Point3D spts[3];
    for (int i = 0; i < 3; i++) {
        spts[i] = sph(pts[i], v_r * (-1.0));
    }

    Facet3Pt fctS;
    fctS.Init(spts);

    int n = 3;
    double z, u, v, w, W2, W, U, V, T, f = 0.0;
    const Point3D *v_a = &spts[0];
    z = std::fabs(fctS.v_n * (*v_a));

    for (int i = 0; i < n; i++) {
        u = fctS.v_mi[i] * spts[i];
        v = u + fctS.len[i];
        w = fctS.v_ni[i] * spts[i];

        z = z + EPS_VAL;
        W2 = w * w + z * z;
        U = std::sqrt(u * u + W2);
        V = std::sqrt(v * v + W2);
        W = std::sqrt(W2);
        T = U + V;
        f += w * (sign(v) * std::log((V + std::fabs(v)) / W) - sign(u) * std::log((U + std::fabs(u)) / W)) -
             2.0 * z * std::atan((2.0 * w * fctS.len[i]) / ((T + fctS.len[i]) * std::fabs(T - fctS.len[i]) + 2.0 * T * z));
    }
    f *= GRAV_CONST;
    v_Grv += fctS.v_n * f;
}

void Facet3Pt::FldSpherGS(const Point3D &v_r, Point3D v_M, Point3D &v_Mag, Point3D &v_Grv) const
{
    Point3D spts[3];
    for (int i = 0; i < 3; i++) {
        spts[i] = sph(pts[i], v_r * (-1.0));
    }

    Point3D v_L;
    Point3D v_rr;
    Point3D v_sn = (spts[0] - spts[1]) / (spts[1] - spts[2]);
    v_sn.Unit();

    double r, L, b, I, h;
    double P = 0.0, Q = 0.0, R = 0.0;
    double s, d;
    double dOmega;

    dOmega = SolidAngle(spts);
    if (dOmega == 0.0) return;

    int n = 3;
    for (int i = 0; i < n; i++) {
        v_rr = spts[i];
        if (i < (n - 1)) {
            v_L = spts[i + 1] - spts[i];
        } else {
            v_L = spts[0] - spts[i];
        }
        r = spts[i].Abs();
        L = v_L.Abs();
        b = 2.0 * (v_rr * v_L);
        h = r + b / (2.0 * L);
        if (h != 0.0)
            I = (1.0 / L) * std::log((std::sqrt(L * L + b + r * r) + L + b / (2.0 * L)) / h);
        else
            I = (1.0 / L) * std::log(std::fabs(L - r) / r);

        P += I * v_L.x;
        Q += I * v_L.y;
        R += I * v_L.z;
    }

    Point3D v_f;
    v_f.x = dOmega * v_sn.x + Q * v_sn.z - R * v_sn.y;
    v_f.y = dOmega * v_sn.y + R * v_sn.x - P * v_sn.z;
    v_f.z = dOmega * v_sn.z + P * v_sn.y - Q * v_sn.x;

    s = v_M * v_sn;
    v_Mag += v_f * s;

    d = spts[0] * v_sn;
    v_Grv += v_f * d * GRAV_CONST;
}

double Facet3Pt::GetMeanElevation() const
{
    return (pts[0].z + pts[1].z + pts[2].z) / 3.0;
}

Point3D Facet3Pt::Centroid() const
{
    return pts[0] + pts[1] + pts[2];
}

const Point3D *Facet3Pt::ContainsVertex(const Point3D *pt) const
{
    for (int i = 0; i < 3; i++) {
        if (pts[i] == *pt)
            return &pts[i];
    }
    return nullptr;
}

bool Facet3Pt::IsOposit(const Facet3Pt &fct) const
{
    Point3D c1 = pts[0] + pts[2] + pts[1];
    Point3D c2 = fct.pts[0] + fct.pts[1] + fct.pts[2];
    return (c1 == c2);
}

void Facet3Pt::Reverse()
{
    Point3D pt = pts[0];
    pts[0] = pts[2];
    pts[2] = pt;

    Body *pBd = pBody;
    pBody = pBodyOpos;
    pBodyOpos = pBd;

    Init();
}

static bool ComputeGravityVlado(const Point3D &v_r, const Facet3Pt &facet, Point3D &fGrv,
                                double dRefDensity, int nTag, Point3D vRefDensGrad, double dRefDensOrg)
{
    const Body *pBody = facet.pBody;
    const Body *pBodyOpos = facet.pBodyOpos;
    Point3D fb(0, 0, 0), fOb(0, 0, 0);

    if (pBodyOpos == nullptr) {
        if (pBody->GetDensityGradient().IsZero()) {
            if (nTag == 1) {
                facet.FldVlado(v_r, fb);
                fb = fb * pBody->GetDensity();
                facet.FldVlado(v_r, fOb, vRefDensGrad, dRefDensOrg);
                fGrv = fb - fOb;
            } else {
                facet.FldVlado(v_r, fGrv);
                fGrv = fGrv * (pBody->GetDensity() - dRefDensity);
            }
        } else {
            facet.FldVlado(v_r, fb, pBody->GetDensityGradient(), pBody->GetDensityAtOrigin());
            if (nTag == 1) {
                facet.FldVlado(v_r, fOb, vRefDensGrad, dRefDensOrg);
            } else {
                facet.FldVlado(v_r, fOb);
                fOb = fOb * dRefDensity;
            }
            fGrv = fb - fOb;
        }
    } else {
        if (pBody->GetDensityGradient().IsZero()) {
            facet.FldVlado(v_r, fb);
            fb = fb * pBody->GetDensity();
        } else {
            facet.FldVlado(v_r, fb, pBody->GetDensityGradient(), pBody->GetDensityAtOrigin());
        }

        if (pBodyOpos->GetDensityGradient().IsZero()) {
            facet.FldVlado(v_r, fOb);
            fOb = fOb * pBodyOpos->GetDensity();
        } else {
            facet.FldVlado(v_r, fOb, pBodyOpos->GetDensityGradient(), pBodyOpos->GetDensityAtOrigin());
        }
        fGrv += fb - fOb;
    }
    return true;
}

void Facet3Pt::Compute(
    double *gx, double *gy, double *gz,
    double *gxx, double *gyy, double *gzz, double *gxy, double *gxz, double *gyz,
    double *mx, double *my, double *mz,
    double &dRefDens, double &dRefDensOrg, Point3D &v_refDens, int nRdm,
    Point3D &v_rGrv, Point3D &v_rTen, Point3D &v_rMag,
    double &dUnitGrv, double &dUnitMag, double &dUnitTns)
{
    if (GetType() == FacetType::FCT_NULL)
        return;

    Point3D fGrv(0, 0, 0), fMag(0, 0, 0);
    double txx = 0, tyy = 0, tzz = 0, txy = 0, txz = 0, tyz = 0;

    // Gravity field
    if ((pBody && (pBody->GetDensity() != 0.0 || dRefDens != 0.0)) ||
        (pBodyOpos && (pBodyOpos->GetDensity() != 0.0 || dRefDens != 0.0)))
    {
        if (gxx || gyy || gzz || gxy || gxz || gyz) {
            FldVladoGrd(v_rTen, dRefDens, txx, tyy, tzz, txy, txz, tyz);
            if (gxx != nullptr) *gxx += dUnitTns * dSign * txx;
            if (gyy != nullptr) *gyy += dUnitTns * dSign * tyy;
            if (gzz != nullptr) *gzz += dUnitTns * dSign * tzz;
            if (gxy != nullptr) *gxy += dUnitTns * dSign * txy;
            if (gxz != nullptr) *gxz += dUnitTns * dSign * txz;
            if (gyz != nullptr) *gyz += dUnitTns * dSign * tyz;
        }
        if (gx || gy || gz) {
            fGrv.Zero();
            ComputeGravityVlado(v_rGrv, *this, fGrv, dRefDens, nRdm, v_refDens, dRefDensOrg);
            if (gx != nullptr) *gx += dUnitGrv * dSign * fGrv.x;
            if (gy != nullptr) *gy += dUnitGrv * dSign * fGrv.y;
            if (gz != nullptr) *gz += dUnitGrv * dSign * fGrv.z;
        }
    }

    // Magnetic field
    if (mx || my || mz) {
        fMag.Zero();
        if (pBody && !pBody->GetMagnetizationVector().IsZero() && pBody->IsActive()) {
            FldGS(v_rMag, pBody->GetMagnetizationVector(), fMag, 1.0);
        }
        if (pBodyOpos && !pBodyOpos->GetMagnetizationVector().IsZero() && pBodyOpos->IsActive()) {
            FldGS(v_rMag, pBodyOpos->GetMagnetizationVector(), fMag, -1.0);
        }
        if (mx != nullptr) *mx -= dUnitMag * dSign * fMag.x;
        if (my != nullptr) *my -= dUnitMag * dSign * fMag.y;
        if (mz != nullptr) *mz -= dUnitMag * dSign * fMag.z;
    }
}

} // namespace mod3d
