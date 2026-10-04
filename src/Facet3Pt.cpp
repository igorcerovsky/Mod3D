#include "mod3d/Facet3Pt.h"
#include "mod3d/Body.h"
#include "mod3d/PotField.h"
#include <cmath>
#include <algorithm>
#include <utility>

namespace mod3d {

namespace {

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

Facet3Pt::Facet3Pt(const Point3D &pt0, const Point3D &pt1, const Point3D &pt2)
    : pts{pt0, pt1, pt2}
{
    Init();
}

Facet3Pt::Facet3Pt(const Point3D &pt0, const Point3D &pt1, const Point3D &pt2, double densityCCW, double densityCW)
    : pts{pt0, pt1, pt2}, density(densityCCW), densityOpos(densityCW)
{
    Init();
}

Facet3Pt::Facet3Pt(std::span<const Point3D, 3> points)
    : pts{points[0], points[1], points[2]}
{
    Init();
}

Facet3Pt::Facet3Pt(const std::array<Point3D, 3> &points)
    : pts(points)
{
    Init();
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
    if (ppts) {
        pts[0] = ppts[0];
        pts[1] = ppts[1];
        pts[2] = ppts[2];
        Init();
    }
}

void Facet3Pt::Init(std::span<const Point3D, 3> ppts)
{
    pts[0] = ppts[0];
    pts[1] = ppts[1];
    pts[2] = ppts[2];
    Init();
}

void Facet3Pt::Init(const std::array<Point3D, 3> &ppts)
{
    pts = ppts;
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
    bLinOpos = v_densGradOpos.is_zero();
}

void Facet3Pt::Init()
{
    // Outward unit normal vector (for triangle in CCW order)
    v_n = (pts[0] - pts[1]).cross(pts[1] - pts[2]);
    v_n.normalize();

    for (size_t i = 0; i < 3; ++i) {
        v_mi[i] = pts[(i + 1) % 3] - pts[i];
        v_L[i] = v_mi[i];
        len[i] = v_mi[i].norm();
        v_mi[i].normalize();
        v_ni[i] = v_mi[i].cross(v_n);
    }

    bLin = v_densGrad.is_zero();
    bLinOpos = v_densGradOpos.is_zero();

    // Initialize modern header-only pfld facet representation
    pfld_.Init(std::span<const Point3D, 3>(pts));
}

// Gravity field of a polygonal facet (Pohanka / Vlado) with constant density
void Facet3Pt::FldVlado(const Point3D &v_r, Point3D &v_Grv) const
{
    pfld_.Fld_G(v_r, v_Grv);
}

// Gravity field for variable (linear) density gradient
void Facet3Pt::FldVlado(const Point3D &v_r, Point3D &v_Grv, Point3D ro, double ro0) const
{
    pfld_.Fld_G(v_r, ro, ro0, v_Grv);
}

// Full gravity gradient tensor
void Facet3Pt::FldVladoGrd(const Point3D &v_r, double refDensity,
                           double &gxx, double &gyy, double &gzz,
                           double &gxy, double &gxz, double &gyz) const
{
    pfld_.FldVladoGrd(v_r, refDensity, gxx, gyy, gzz, gxy, gxz, gyz, densityOpos, density);
}

void Facet3Pt::FldGS(const Point3D &v_r, Point3D v_M, Point3D &v_Mag, Point3D &v_Grv) const
{
    pfld_.FldGS(v_r, v_M, v_Mag, v_Grv);
}

void Facet3Pt::FldGS(const Point3D &v_r, Point3D v_M, Point3D &v_Mag, double dSignMultiplier) const
{
    Point3D mag(0, 0, 0);
    pfld_.FldGS_M(v_r, v_M, mag);
    v_Mag += mag * dSignMultiplier;
}

double Facet3Pt::solid_angle(const Point3D *spts) const
{
    if (!spts) return 0.0;
    return pfld::Facet<double>::SolidAngle(std::span<const Point3D>(spts, 3), v_n * spts[1], 3);
}

double Facet3Pt::solid_angle(std::span<const Point3D, 3> spts) const
{
    return pfld::Facet<double>::SolidAngle(spts, v_n * spts[1], 3);
}

void Facet3Pt::FldSpherVlado(const Point3D &v_r, Point3D &v_Grv) const
{
    std::array<Point3D, 3> spts;
    for (size_t i = 0; i < 3; ++i) {
        spts[i] = sph(pts[i], -v_r);
    }

    pfld::Facet<double> fctS(std::span<const Point3D>(spts.data(), 3), true);
    fctS.Fld_G(Point3D(0, 0, 0), v_Grv);
}

void Facet3Pt::FldSpherGS(const Point3D &v_r, Point3D v_M, Point3D &v_Mag, Point3D &v_Grv) const
{
    std::array<Point3D, 3> spts;
    for (size_t i = 0; i < 3; ++i) {
        spts[i] = sph(pts[i], -v_r);
    }

    pfld::Facet<double> fctS(std::span<const Point3D>(spts.data(), 3), true);
    fctS.FldGS(Point3D(0, 0, 0), v_M, v_Mag, v_Grv);
}

double Facet3Pt::mean_elevation() const noexcept
{
    return (pts[0].z + pts[1].z + pts[2].z) / 3.0;
}

Point3D Facet3Pt::centroid() const noexcept
{
    return pts[0] + pts[1] + pts[2];
}

double Facet3Pt::area() const noexcept
{
    return 0.5 * (pts[1] - pts[0]).cross(pts[2] - pts[0]).norm();
}

const Point3D *Facet3Pt::ContainsVertex(const Point3D *pt) const noexcept
{
    if (!pt) return nullptr;
    for (size_t i = 0; i < 3; ++i) {
        if (pts[i] == *pt)
            return &pts[i];
    }
    return nullptr;
}

bool Facet3Pt::contains_vertex(const Point3D &pt) const noexcept
{
    return (pts[0] == pt || pts[1] == pt || pts[2] == pt);
}

bool Facet3Pt::is_opposite(const Facet3Pt &fct) const noexcept
{
    Point3D c1 = pts[0] + pts[2] + pts[1];
    Point3D c2 = fct.pts[0] + fct.pts[1] + fct.pts[2];
    return (c1 == c2);
}

void Facet3Pt::reverse()
{
    std::swap(pts[0], pts[2]);
    std::swap(pBody, pBodyOpos);
    Init();
}

static bool ComputeGravityVlado(const Point3D &v_r, const Facet3Pt &facet, Point3D &fGrv,
                                double dRefDensity, int nTag, Point3D vRefDensGrad, double dRefDensOrg)
{
    const Body *pBody = facet.pBody;
    const Body *pBodyOpos = facet.pBodyOpos;
    Point3D fb(0, 0, 0), fOb(0, 0, 0);

    if (pBodyOpos == nullptr) {
        if (!pBody) {
            return false;
        }
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
        if (pBody) {
            if (pBody->GetDensityGradient().IsZero()) {
                facet.FldVlado(v_r, fb);
                fb = fb * pBody->GetDensity();
            } else {
                facet.FldVlado(v_r, fb, pBody->GetDensityGradient(), pBody->GetDensityAtOrigin());
            }
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
    if (is_null())
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
