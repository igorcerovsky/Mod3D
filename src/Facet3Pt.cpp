#include "mod3d/Facet3Pt.h"
#include "mod3d/Body.h"
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
    init();
}

Facet3Pt::Facet3Pt(const Point3D &pt0, const Point3D &pt1, const Point3D &pt2, double density_ccw, double density_cw)
    : pts{pt0, pt1, pt2}, primary{density_ccw}, opposite{density_cw}
{
    init();
}

Facet3Pt::Facet3Pt(std::span<const Point3D, 3> points)
    : pts{points[0], points[1], points[2]}
{
    init();
}

Facet3Pt::Facet3Pt(const Vertices &points)
    : pts(points)
{
    init();
}

void Facet3Pt::init(const Point3D &pt0, const Point3D &pt1, const Point3D &pt2)
{
    pts[0] = pt0;
    pts[1] = pt1;
    pts[2] = pt2;
    init();
}

void Facet3Pt::init(const Point3D &pt0, const Point3D &pt1, const Point3D &pt2, double density_ccw, double density_cw)
{
    pts[0] = pt0;
    pts[1] = pt1;
    pts[2] = pt2;
    primary.density = density_ccw;
    opposite.density = density_cw;
    init();
}

void Facet3Pt::init(const Point3D &pt0, const Point3D &pt1, const Point3D &pt2, double density_ccw, Point3D dens_grad_ccw)
{
    pts[0] = pt0;
    pts[1] = pt1;
    pts[2] = pt2;
    primary.density = density_ccw;
    primary.gradient = dens_grad_ccw;
    init();
}

void Facet3Pt::init(const Point3D *ppts)
{
    if (ppts) {
        pts[0] = ppts[0];
        pts[1] = ppts[1];
        pts[2] = ppts[2];
        init();
    }
}

void Facet3Pt::init(std::span<const Point3D, 3> ppts)
{
    pts[0] = ppts[0];
    pts[1] = ppts[1];
    pts[2] = ppts[2];
    init();
}

void Facet3Pt::init(const Vertices &ppts)
{
    pts = ppts;
    init();
}

void Facet3Pt::init(const std::vector<Point3D> &ppts)
{
    if (ppts.size() >= 3) {
        pts[0] = ppts[0];
        pts[1] = ppts[1];
        pts[2] = ppts[2];
        init();
    }
}

void Facet3Pt::set_opposite_density(double d_density, Point3D v_grad) noexcept
{
    opposite.density = d_density;
    opposite.gradient = v_grad;
    opposite.is_linear = opposite.has_gradient();
}

void Facet3Pt::init()
{
    // Outward unit normal vector (for triangle in CCW order)
    v_n = (pts[0] - pts[1]).cross(pts[1] - pts[2]);
    v_n.normalize();

    primary.is_linear = primary.has_gradient();
    opposite.is_linear = opposite.has_gradient();

    // Initialize modern header-only pfld facet representation
    pfld_.Init(std::span<const Point3D, 3>(pts));
}

// Gravity field of a polygonal facet (Pohanka / Vlado) with constant density
void Facet3Pt::fld_vlado(const Point3D &v_r, Point3D &v_grv) const
{
    pfld_.Fld_G(v_r, v_grv);
}

// Gravity field for variable (linear) density gradient
void Facet3Pt::fld_vlado(const Point3D &v_r, Point3D &v_grv, Point3D ro, double ro0) const
{
    pfld_.Fld_G(v_r, ro, ro0, v_grv);
}

// Full gravity gradient tensor
void Facet3Pt::fld_vlado_grd(const Point3D &v_r, double ref_density,
                             double &gxx, double &gyy, double &gzz,
                             double &gxy, double &gxz, double &gyz) const
{
    pfld_.FldVladoGrd(v_r, ref_density, gxx, gyy, gzz, gxy, gxz, gyz, opposite.density, primary.density);
}

void Facet3Pt::fld_gs(const Point3D &v_r, Point3D v_m, Point3D &v_mag, Point3D &v_grv) const
{
    pfld_.FldGS(v_r, v_m, v_mag, v_grv);
}

void Facet3Pt::fld_gs(const Point3D &v_r, Point3D v_m, Point3D &v_mag, double sign_multiplier) const
{
    Point3D mag(0, 0, 0);
    pfld_.FldGS_M(v_r, v_m, mag);
    v_mag += mag * sign_multiplier;
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

void Facet3Pt::fld_spher_vlado(const Point3D &v_r, Point3D &v_grv) const
{
    std::array<Point3D, 3> spts;
    for (size_t i = 0; i < 3; ++i) {
        spts[i] = sph(pts[i], -v_r);
    }

    pfld::Facet<double> fctS(std::span<const Point3D>(spts.data(), 3), true);
    fctS.Fld_G(Point3D(0, 0, 0), v_grv);
}

void Facet3Pt::fld_spher_gs(const Point3D &v_r, Point3D v_m, Point3D &v_mag, Point3D &v_grv) const
{
    std::array<Point3D, 3> spts;
    for (size_t i = 0; i < 3; ++i) {
        spts[i] = sph(pts[i], -v_r);
    }

    pfld::Facet<double> fctS(std::span<const Point3D>(spts.data(), 3), true);
    fctS.FldGS(Point3D(0, 0, 0), v_m, v_mag, v_grv);
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
    std::swap(primary, opposite);
    init();
}

static bool ComputeGravityVlado(const Point3D &v_r, const Facet3Pt &facet, Point3D &f_grv,
                                double ref_density, int n_tag, Point3D v_ref_dens_grad, double ref_dens_org)
{
    const Body *pBody = facet.primary.body;
    const Body *pBodyOpos = facet.opposite.body;
    Point3D fb(0, 0, 0), fOb(0, 0, 0);

    if (pBodyOpos == nullptr) {
        if (!pBody) {
            return false;
        }
        if (pBody->density_gradient().is_zero()) {
            if (n_tag == 1) {
                facet.fld_vlado(v_r, fb);
                fb = fb * pBody->density();
                facet.fld_vlado(v_r, fOb, v_ref_dens_grad, ref_dens_org);
                f_grv = fb - fOb;
            } else {
                facet.fld_vlado(v_r, f_grv);
                f_grv = f_grv * (pBody->density() - ref_density);
            }
        } else {
            facet.fld_vlado(v_r, fb, pBody->density_gradient(), pBody->density_at_origin());
            if (n_tag == 1) {
                facet.fld_vlado(v_r, fOb, v_ref_dens_grad, ref_dens_org);
            } else {
                facet.fld_vlado(v_r, fOb);
                fOb = fOb * ref_density;
            }
            f_grv = fb - fOb;
        }
    } else {
        if (pBody) {
            if (pBody->density_gradient().is_zero()) {
                facet.fld_vlado(v_r, fb);
                fb = fb * pBody->density();
            } else {
                facet.fld_vlado(v_r, fb, pBody->density_gradient(), pBody->density_at_origin());
            }
        }

        if (pBodyOpos->density_gradient().is_zero()) {
            facet.fld_vlado(v_r, fOb);
            fOb = fOb * pBodyOpos->density();
        } else {
            facet.fld_vlado(v_r, fOb, pBodyOpos->density_gradient(), pBodyOpos->density_at_origin());
        }
        f_grv += fb - fOb;
    }
    return true;
}

void Facet3Pt::compute(
    double *gx, double *gy, double *gz,
    double *gxx, double *gyy, double *gzz, double *gxy, double *gxz, double *gyz,
    double *mx, double *my, double *mz,
    double &ref_density, double &ref_density_org, Point3D &v_ref_dens, int n_rdm,
    Point3D &v_r_grv, Point3D &v_r_ten, Point3D &v_r_mag,
    double &unit_grv, double &unit_mag, double &unit_tns)
{
    if (is_null())
        return;

    Point3D fGrv(0, 0, 0), fMag(0, 0, 0);
    double txx = 0, tyy = 0, tzz = 0, txy = 0, txz = 0, tyz = 0;

    // Gravity field
    if ((primary.body && (primary.body->density() != 0.0 || ref_density != 0.0)) ||
        (opposite.body && (opposite.body->density() != 0.0 || ref_density != 0.0)))
    {
        if (gxx || gyy || gzz || gxy || gxz || gyz) {
            fld_vlado_grd(v_r_ten, ref_density, txx, tyy, tzz, txy, txz, tyz);
            if (gxx != nullptr) *gxx += unit_tns * dSign * txx;
            if (gyy != nullptr) *gyy += unit_tns * dSign * tyy;
            if (gzz != nullptr) *gzz += unit_tns * dSign * tzz;
            if (gxy != nullptr) *gxy += unit_tns * dSign * txy;
            if (gxz != nullptr) *gxz += unit_tns * dSign * txz;
            if (gyz != nullptr) *gyz += unit_tns * dSign * tyz;
        }
        if (gx || gy || gz) {
            fGrv.zero();
            ComputeGravityVlado(v_r_grv, *this, fGrv, ref_density, n_rdm, v_ref_dens, ref_density_org);
            if (gx != nullptr) *gx += unit_grv * dSign * fGrv.x;
            if (gy != nullptr) *gy += unit_grv * dSign * fGrv.y;
            if (gz != nullptr) *gz += unit_grv * dSign * fGrv.z;
        }
    }

    // Magnetic field
    if (mx || my || mz) {
        fMag.zero();
        if (primary.body && !primary.body->magnetization_vector().is_zero() && primary.body->is_active()) {
            fld_gs(v_r_mag, primary.body->magnetization_vector(), fMag, 1.0);
        }
        if (opposite.body && !opposite.body->magnetization_vector().is_zero() && opposite.body->is_active()) {
            fld_gs(v_r_mag, opposite.body->magnetization_vector(), fMag, -1.0);
        }
        if (mx != nullptr) *mx -= unit_mag * dSign * fMag.x;
        if (my != nullptr) *my -= unit_mag * dSign * fMag.y;
        if (mz != nullptr) *mz -= unit_mag * dSign * fMag.z;
    }
}

} // namespace mod3d
