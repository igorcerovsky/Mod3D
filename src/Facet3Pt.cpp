#include "mod3d/Facet3Pt.h"
#include "mod3d/Body.h"
#include <algorithm>
#include <cmath>
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

Facet3Pt::Facet3Pt(const Point3D &pt0, const Point3D &pt1, const Point3D &pt2,
                   Body *primaryBody, Body *oppositeBody)
    : primary{primaryBody}, opposite{oppositeBody} {
  init(pt0, pt1, pt2, primaryBody, oppositeBody);
}

Facet3Pt::Facet3Pt(std::span<const Point3D, 3> points, Body *primaryBody,
                   Body *oppositeBody)
    : primary{primaryBody}, opposite{oppositeBody} {
  init(points);
}

Facet3Pt::Facet3Pt(const Vertices &points, Body *primaryBody,
                   Body *oppositeBody)
    : primary{primaryBody}, opposite{oppositeBody} {
  init(points);
}

void Facet3Pt::init(const Point3D &pt0, const Point3D &pt1, const Point3D &pt2,
                    Body *primaryBody, Body *oppositeBody) {
  const std::array<Point3D, 3> ppts{pt0, pt1, pt2};
  pfld_.Init(std::span<const Point3D, 3>(ppts));
  if (primaryBody)
    primary = primaryBody;
  if (oppositeBody)
    opposite = oppositeBody;
}

void Facet3Pt::init(const Point3D *ppts) {
  if (ppts) {
    pfld_.Init(std::span<const Point3D, 3>(ppts, 3));
  }
}

void Facet3Pt::init(std::span<const Point3D, 3> ppts) { pfld_.Init(ppts); }

void Facet3Pt::init(const Vertices &ppts) {
  pfld_.Init(std::span<const Point3D, 3>(ppts));
}

void Facet3Pt::init(const std::vector<Point3D> &ppts) {
  if (ppts.size() >= 3) {
    pfld_.Init(ppts);
  }
}

// Gravity field of a polygonal facet (Pohanka / Vlado) with constant density
void Facet3Pt::fld_vlado(const Point3D &v_r, Point3D &v_grv) const {
  pfld_.Fld_G(v_r, v_grv);
}

// Gravity field for variable (linear) density gradient
void Facet3Pt::fld_vlado(const Point3D &v_r, Point3D &v_grv, Point3D ro,
                         double ro0) const {
  pfld_.Fld_G(v_r, ro, ro0, v_grv);
}

// Full gravity gradient tensor
void Facet3Pt::fld_vlado_grd(const Point3D &v_r, double ref_density,
                             double &gxx, double &gyy, double &gzz, double &gxy,
                             double &gxz, double &gyz) const {
  pfld_.FldVladoGrd(v_r, ref_density, gxx, gyy, gzz, gxy, gxz, gyz,
                    opposite_density_val(), density_val());
}

void Facet3Pt::fld_gs(const Point3D &v_r, Point3D v_m, Point3D &v_mag,
                      Point3D &v_grv) const {
  pfld_.FldGS(v_r, v_m, v_mag, v_grv);
}

void Facet3Pt::fld_gs(const Point3D &v_r, Point3D v_m, Point3D &v_mag,
                      double sign_multiplier) const {
  Point3D mag(0, 0, 0);
  pfld_.FldGS_M(v_r, v_m, mag);
  v_mag += mag * sign_multiplier;
}

double Facet3Pt::solid_angle(const Point3D *spts) const {
  if (!spts)
    return 0.0;
  return pfld::Facet<double, 3>::SolidAngle(std::span<const Point3D>(spts, 3),
                                            normal() * spts[1], 3);
}

double Facet3Pt::solid_angle(std::span<const Point3D, 3> spts) const {
  return pfld::Facet<double, 3>::SolidAngle(spts, normal() * spts[1], 3);
}

void Facet3Pt::fld_spher_vlado(const Point3D &v_r, Point3D &v_grv) const {
  const auto &p = pfld_.points();
  std::array<Point3D, 3> spts;
  for (size_t i = 0; i < 3 && i < p.size(); ++i) {
    spts[i] = sph(p[i], -v_r);
  }

  pfld::Facet<double, 3> fctS(spts, true);
  fctS.Fld_G(Point3D(0, 0, 0), v_grv);
}

void Facet3Pt::fld_spher_gs(const Point3D &v_r, Point3D v_m, Point3D &v_mag,
                            Point3D &v_grv) const {
  const auto &p = pfld_.points();
  std::array<Point3D, 3> spts;
  for (size_t i = 0; i < 3 && i < p.size(); ++i) {
    spts[i] = sph(p[i], -v_r);
  }

  pfld::Facet<double, 3> fctS(spts, true);
  fctS.FldGS(Point3D(0, 0, 0), v_m, v_mag, v_grv);
}

double Facet3Pt::mean_elevation() const noexcept {
  const auto &p = pfld_.points();
  if (p.size() < 3)
    return 0.0;
  return (p[0].z + p[1].z + p[2].z) / 3.0;
}

Point3D Facet3Pt::centroid() const noexcept {
  const auto &p = pfld_.points();
  if (p.size() < 3)
    return Point3D(0, 0, 0);
  return p[0] + p[1] + p[2];
}

double Facet3Pt::area() const noexcept {
  const auto &p = pfld_.points();
  if (p.size() < 3)
    return 0.0;
  return 0.5 * (p[1] - p[0]).cross(p[2] - p[0]).norm();
}

bool Facet3Pt::contains_vertex(const Point3D &pt) const noexcept {
  const auto &p = pfld_.points();
  for (const auto &v : p) {
    if (v == pt)
      return true;
  }
  return false;
}

bool Facet3Pt::is_opposite(const Facet3Pt &fct) const noexcept {
  const auto &p1 = pfld_.points();
  const auto &p2 = fct.pfld_.points();
  if (p1.size() < 3 || p2.size() < 3)
    return false;
  Point3D c1 = p1[0] + p1[2] + p1[1];
  Point3D c2 = p2[0] + p2[1] + p2[2];
  return (c1 == c2);
}

void Facet3Pt::reverse() {
  auto &p = pfld_.points();
  if (p.size() >= 3) {
    std::swap(p[0], p[2]);
    pfld_.Init(p);
  }
  std::swap(primary, opposite);
}

static bool ComputeGravityVlado(const Point3D &v_r, const Facet3Pt &facet,
                                Point3D &f_grv, double ref_density, int n_tag,
                                Point3D v_ref_dens_grad, double ref_dens_org) {
  const Body *pBody = facet.primary;
  const Body *pBodyOpos = facet.opposite;
  Point3D fb(0, 0, 0), fOb(0, 0, 0);

  if (pBodyOpos == nullptr) {
    if (!pBody) {
      return false;
    }
    if (pBody->density_gradient().is_zero()) {
      if (n_tag == 1) {
        facet.fld_vlado(v_r, fb);
        fb = fb * pBody->density().value();
        facet.fld_vlado(v_r, fOb, v_ref_dens_grad, ref_dens_org);
        f_grv = fb - fOb;
      } else {
        facet.fld_vlado(v_r, f_grv);
        f_grv = f_grv * (pBody->density().value() - ref_density);
      }
    } else {
      facet.fld_vlado(v_r, fb, pBody->density_gradient(),
                      pBody->density_at_origin().value());
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
        fb = fb * pBody->density().value();
      } else {
        facet.fld_vlado(v_r, fb, pBody->density_gradient(),
                        pBody->density_at_origin().value());
      }
    }

    if (pBodyOpos->density_gradient().is_zero()) {
      facet.fld_vlado(v_r, fOb);
      fOb = fOb * pBodyOpos->density().value();
    } else {
      facet.fld_vlado(v_r, fOb, pBodyOpos->density_gradient(),
                      pBodyOpos->density_at_origin().value());
    }
    f_grv += fb - fOb;
  }
  return true;
}

void Facet3Pt::compute(double *gx, double *gy, double *gz, double *gxx,
                       double *gyy, double *gzz, double *gxy, double *gxz,
                       double *gyz, double *mx, double *my, double *mz,
                       double &ref_density, double &ref_density_org,
                       Point3D &v_ref_dens, int n_rdm, Point3D &v_r_grv,
                       Point3D &v_r_ten, Point3D &v_r_mag, double &unit_grv,
                       double &unit_mag, double &unit_tns) {
  if (is_null())
    return;

  Point3D fGrv(0, 0, 0), fMag(0, 0, 0);
  double txx = 0, tyy = 0, tzz = 0, txy = 0, txz = 0, tyz = 0;

  // Gravity field
  if ((primary && (primary->density().value() != 0.0 || ref_density != 0.0)) ||
      (opposite &&
       (opposite->density().value() != 0.0 || ref_density != 0.0))) {
    if (gxx || gyy || gzz || gxy || gxz || gyz) {
      fld_vlado_grd(v_r_ten, ref_density, txx, tyy, tzz, txy, txz, tyz);
      if (gxx != nullptr)
        *gxx += unit_tns * dSign * txx;
      if (gyy != nullptr)
        *gyy += unit_tns * dSign * tyy;
      if (gzz != nullptr)
        *gzz += unit_tns * dSign * tzz;
      if (gxy != nullptr)
        *gxy += unit_tns * dSign * txy;
      if (gxz != nullptr)
        *gxz += unit_tns * dSign * txz;
      if (gyz != nullptr)
        *gyz += unit_tns * dSign * tyz;
    }
    if (gx || gy || gz) {
      fGrv.zero();
      ComputeGravityVlado(v_r_grv, *this, fGrv, ref_density, n_rdm, v_ref_dens,
                          ref_density_org);
      if (gx != nullptr)
        *gx += unit_grv * dSign * fGrv.x;
      if (gy != nullptr)
        *gy += unit_grv * dSign * fGrv.y;
      if (gz != nullptr)
        *gz += unit_grv * dSign * fGrv.z;
    }
  }

  // Magnetic field
  if (mx || my || mz) {
    fMag.zero();
    if (primary && !primary->magnetization_vector().is_zero() &&
        primary->is_active()) {
      fld_gs(v_r_mag, primary->magnetization_vector(), fMag, 1.0);
    }
    if (opposite && !opposite->magnetization_vector().is_zero() &&
        opposite->is_active()) {
      fld_gs(v_r_mag, opposite->magnetization_vector(), fMag, -1.0);
    }
    if (mx != nullptr)
      *mx -= unit_mag * dSign * fMag.x;
    if (my != nullptr)
      *my -= unit_mag * dSign * fMag.y;
    if (mz != nullptr)
      *mz -= unit_mag * dSign * fMag.z;
  }
}

} // namespace mod3d
