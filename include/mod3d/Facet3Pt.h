#pragma once

#include "mod3d/Point3D.h"
#include "mod3d/Body.h"
#include "mod3d/Units.h"
#include "pfld/facet.hpp"
#include <vector>
#include <array>
#include <memory>
#include <span>
#include <algorithm>
#include <type_traits>

namespace mod3d {

class Body;

using Triangle3D = std::array<Point3D, 3>;

enum class FacetType {
    FCT_NULL = 0,
    FCT_NORMAL = 1,
    FCT_SIDE = 2,
    FCT_OUTER = 3,
    FCT_EXTENDED = 4,
    FCT_TOP = 5,
    FCT_BOT = 6,
    FCT_OUTERBOUNDARY = 7,
    FCT_NULLOPOSIT = 8
};


/**
 * @brief Triangular facet element in 3D space.
 * 
 * Represents a planar triangle with outward normal, edge vectors, physical properties (density, magnetization),
 * and potential field computations (Vlado Pohanka, Guptasarma-Singh, and modern pfld integration).
 */
class Facet3Pt {
public:
    using Vertices = std::array<Point3D, 3>;

    // ========================================================================
    // Lifecycle & Rule of 5
    // ========================================================================
    constexpr Facet3Pt() noexcept = default;
    Facet3Pt(const Point3D &pt0, const Point3D &pt1, const Point3D &pt2, Body *primaryBody = nullptr, Body *oppositeBody = nullptr);
    explicit Facet3Pt(std::span<const Point3D, 3> points, Body *primaryBody = nullptr, Body *oppositeBody = nullptr);
    explicit Facet3Pt(const Vertices &points, Body *primaryBody = nullptr, Body *oppositeBody = nullptr);
    ~Facet3Pt() = default;

    Facet3Pt(const Facet3Pt &) = default;
    Facet3Pt &operator=(const Facet3Pt &) = default;
    Facet3Pt(Facet3Pt &&) noexcept = default;
    Facet3Pt &operator=(Facet3Pt &&) noexcept = default;

    // ========================================================================
    // Modern C++20 API (STL snake_case convention, noexcept, [[nodiscard]])
    // ========================================================================

    // Vertex Element Access
    [[nodiscard]] Point3D &operator[](size_t index) noexcept { return pfld_[index]; }
    [[nodiscard]] const Point3D &operator[](size_t index) const noexcept { return pfld_[index]; }
    [[nodiscard]] Vertices &points() noexcept { return pfld_.points(); }
    [[nodiscard]] const Vertices &points() const noexcept { return pfld_.points(); }
    [[nodiscard]] static constexpr size_t size() noexcept { return 3; }
    [[nodiscard]] static constexpr bool empty() noexcept { return false; }

    // Facet Attributes & Geometry
    [[nodiscard]] FacetType type() const noexcept { return nType; }
    void set_type(FacetType t) noexcept { nType = t; }

    [[nodiscard]] double sign() const noexcept { return dSign; }
    void set_sign(double s) noexcept { dSign = s; }

    [[nodiscard]] bool is_null() const noexcept { return nType == FacetType::FCT_NULL; }
    [[nodiscard]] bool is_outer() const noexcept { return nType == FacetType::FCT_OUTER; }
    [[nodiscard]] bool is_opposite(const Facet3Pt &fct) const noexcept;

    [[nodiscard]] double mean_elevation() const noexcept;
    [[nodiscard]] units::Meters mean_elevation_qty() const noexcept { return units::Meters(mean_elevation()); }

    [[nodiscard]] Point3D centroid() const noexcept;
    [[nodiscard]] Point3D center() const noexcept {
        const auto &pts = pfld_.points();
        return (pts[0] + pts[1] + pts[2]) / 3.0;
    }
    [[nodiscard]] double area() const noexcept;
    [[nodiscard]] units::SquareMeters area_qty() const noexcept { return units::SquareMeters(area()); }

    [[nodiscard]] const Point3D &normal() const noexcept { return pfld_.normal(); }

    [[nodiscard]] bool contains_vertex(const Point3D &pt) const noexcept;
    void reverse();

    [[nodiscard]] double solid_angle(std::span<const Point3D, 3> spts) const;
    [[nodiscard]] double solid_angle(const Point3D *spts) const;

    // Modern pfld Integration & Unit-Aware Evaluators
    [[nodiscard]] const pfld::Facet<double, 3> &pfld_facet() const noexcept { return pfld_; }
    [[nodiscard]] pfld::Facet<double, 3> &pfld_facet() noexcept { return pfld_; }
    operator const pfld::Facet<double, 3> &() const noexcept { return pfld_; }
    operator pfld::Facet<double, 3> &() noexcept { return pfld_; }
    [[nodiscard]] double field_gz(const Point3D &r) const { return pfld_.field_gz(r); }
    [[nodiscard]] Point3D field_g(const Point3D &r) const { return pfld_.field_g(r); }

    [[nodiscard]] units::SI_Acceleration field_gz_si(const Point3D &r) const { return pfld_.gravity_z(r); }
    [[nodiscard]] units::MilliGal field_gz_mgal(const Point3D &r) const {
        return units::SI_Acceleration(field_gz(r)).as<units::tag::MilliGal>();
    }
    [[nodiscard]] units::GravityUnit field_gz_gu(const Point3D &r) const {
        return units::SI_Acceleration(field_gz(r)).as<units::tag::GravityUnit>();
    }

    template <units::UnitType DensityU = units::tag::KgPerM3>
        requires units::SameDimension<DensityU, units::tag::KgPerM3>
    [[nodiscard]] GravityGradientTensor gravity_gradient_tensor(
        const Point3D &r,
        const units::Quantity<DensityU>& ref_density) const {
        double gxx{0}, gyy{0}, gzz{0}, gxy{0}, gxz{0}, gyz{0};
        fld_vlado_grd(r, ref_density.template in<units::tag::KgPerM3>(), gxx, gyy, gzz, gxy, gxz, gyz);
        return GravityGradientTensor{
            .gxx = units::SI_Gradient(gxx).template as<units::tag::Eotvos>(),
            .gyy = units::SI_Gradient(gyy).template as<units::tag::Eotvos>(),
            .gzz = units::SI_Gradient(gzz).template as<units::tag::Eotvos>(),
            .gxy = units::SI_Gradient(gxy).template as<units::tag::Eotvos>(),
            .gxz = units::SI_Gradient(gxz).template as<units::tag::Eotvos>(),
            .gyz = units::SI_Gradient(gyz).template as<units::tag::Eotvos>()
        };
    }

    // Comparisons
    [[nodiscard]] bool operator==(const Facet3Pt &fct) const noexcept {
        return pfld_ == fct.pfld_;
    }
    [[nodiscard]] bool operator!=(const Facet3Pt &fct) const noexcept {
        return !(*this == fct);
    }

    // Initialization / reinitialization
    void init() noexcept {}
    void init(const Point3D *ppts);
    void init(std::span<const Point3D, 3> ppts);
    void init(const Vertices &ppts);
    void init(const std::vector<Point3D> &ppts);
    void init(const Point3D &pt0, const Point3D &pt1, const Point3D &pt2, Body *primaryBody = nullptr, Body *oppositeBody = nullptr);

    // Analytical potential field formulas
    void fld_vlado(const Point3D &v_r, Point3D &v_grv) const;
    void fld_vlado(const Point3D &v_r, Point3D &v_grv, Point3D ro, double ro0) const;
    void fld_vlado_grd(const Point3D &v_r, double ref_density,
                       double &gxx, double &gyy, double &gzz,
                       double &gxy, double &gxz, double &gyz) const;

    void fld_gs(const Point3D &v_r, Point3D v_m, Point3D &v_mag, Point3D &v_grv) const;
    void fld_gs(const Point3D &v_r, Point3D v_m, Point3D &v_mag, double sign_multiplier = 1.0) const;

    void fld_spher_vlado(const Point3D &v_r, Point3D &v_grv) const;
    void fld_spher_gs(const Point3D &v_r, Point3D v_m, Point3D &v_mag, Point3D &v_grv) const;

    // Field evaluation wrappers
    void fld_g(const Point3D &v_r, Point3D &v_grv) const { fld_vlado(v_r, v_grv); }
    void fld_g(const Point3D &v_r, Point3D ro, double ro0, Point3D &v_grv) const { fld_vlado(v_r, v_grv, ro, ro0); }
    void fld_gz(const Point3D &v_r, double &gz) const {
        Point3D g_vec(0, 0, 0);
        fld_vlado(v_r, g_vec);
        gz += g_vec.z;
    }
    void fld_gz(const Point3D &v_r, Point3D ro, double ro0, double &gz) const {
        Point3D g_vec(0, 0, 0);
        fld_vlado(v_r, g_vec, ro, ro0);
        gz += g_vec.z;
    }
    void fld_gs_gz(const Point3D &v_r, double &gz) const {
        Point3D m(0, 0, 0), g_vec(0, 0, 0);
        fld_gs(v_r, Point3D(0, 0, 0), m, g_vec);
        gz += g_vec.z;
    }

    // High-level field calculation with reference model and body interaction
    void compute(
        double *gx, double *gy, double *gz,
        double *gxx, double *gyy, double *gzz, double *gxy, double *gxz, double *gyz,
        double *mx, double *my, double *mz,
        double &ref_density, double &ref_density_org, Point3D &v_ref_dens, int n_rdm,
        Point3D &v_r_grv, Point3D &v_r_ten, Point3D &v_r_mag,
        double &unit_grv, double &unit_mag, double &unit_tns);

    // Body & Density Forwarders
    [[nodiscard]] Body *body() const noexcept { return primary; }
    void set_body(Body *b) noexcept { primary = b; }
    [[nodiscard]] Body *opposite_body() const noexcept { return opposite; }
    void set_opposite_body(Body *b) noexcept { opposite = b; }

    [[nodiscard]] double density() const noexcept { return primary ? primary->density() : 0.0; }
    [[nodiscard]] double opposite_density() const noexcept { return opposite ? opposite->density() : 0.0; }

    [[nodiscard]] units::KgPerM3 density_qty() const noexcept { return units::KgPerM3(density()); }
    [[nodiscard]] units::KgPerM3 opposite_density_qty() const noexcept { return units::KgPerM3(opposite_density()); }

public:
    FacetType nType{FacetType::FCT_NORMAL};

    Body *primary{nullptr};
    Body *opposite{nullptr};

    double dSign{1.0}; // +1 to add, -1 to subtract (delta updating)

    // Modern header-only pfld facet representation
    pfld::Facet<double, 3> pfld_;
};

static_assert(!std::is_polymorphic_v<Facet3Pt>, "Facet3Pt must not be polymorphic");

using FacetList = std::vector<Facet3Pt>;
using facetvec = std::vector<Facet3Pt>; // Alias for pfld_UnitTest compatibility

} // namespace mod3d
