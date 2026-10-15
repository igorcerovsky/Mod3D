#pragma once

#include "mod3d/Point3D.h"
#include "mod3d/Body.h"
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
 * @brief Physical and body parameters on one side of a facet interface.
 */
struct FacetSide {
    double density{0.0};
    Point3D gradient{0, 0, 0};
    bool is_linear{false};
    Body *body{nullptr};

    [[nodiscard]] constexpr bool has_gradient() const noexcept {
        return !gradient.is_zero();
    }

    void sync_from_body() noexcept {
        if (body) {
            density = body->density();
            gradient = body->density_gradient();
            is_linear = !gradient.is_zero();
        }
    }

    [[nodiscard]] constexpr bool operator==(const FacetSide &other) const noexcept = default;
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
    Facet3Pt(const Point3D &pt0, const Point3D &pt1, const Point3D &pt2);
    Facet3Pt(const Point3D &pt0, const Point3D &pt1, const Point3D &pt2, double densityCCW, double densityCW = 0.0);
    explicit Facet3Pt(std::span<const Point3D, 3> points);
    explicit Facet3Pt(const Vertices &points);
    ~Facet3Pt() = default;

    Facet3Pt(const Facet3Pt &) = default;
    Facet3Pt &operator=(const Facet3Pt &) = default;
    Facet3Pt(Facet3Pt &&) noexcept = default;
    Facet3Pt &operator=(Facet3Pt &&) noexcept = default;

    // ========================================================================
    // Modern C++20 API (STL snake_case convention, noexcept, [[nodiscard]])
    // ========================================================================

    // Vertex Element Access
    [[nodiscard]] constexpr Point3D &operator[](size_t index) noexcept { return pts[index]; }
    [[nodiscard]] constexpr const Point3D &operator[](size_t index) const noexcept { return pts[index]; }
    [[nodiscard]] constexpr Vertices &points() noexcept { return pts; }
    [[nodiscard]] constexpr const Vertices &points() const noexcept { return pts; }
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
    [[nodiscard]] Point3D centroid() const noexcept;
    [[nodiscard]] Point3D center() const noexcept { return (pts[0] + pts[1] + pts[2]) / 3.0; }
    [[nodiscard]] double area() const noexcept;
    [[nodiscard]] const Point3D &normal() const noexcept { return v_n; }

    [[nodiscard]] bool contains_vertex(const Point3D &pt) const noexcept;
    void reverse();

    [[nodiscard]] double solid_angle(std::span<const Point3D, 3> spts) const;
    [[nodiscard]] double solid_angle(const Point3D *spts) const;

    // Modern pfld Integration
    [[nodiscard]] const pfld::Facet<double> &pfld_facet() const noexcept { return pfld_; }
    [[nodiscard]] pfld::Facet<double> &pfld_facet() noexcept { return pfld_; }
    operator const pfld::Facet<double> &() const noexcept { return pfld_; }
    operator pfld::Facet<double> &() noexcept { return pfld_; }
    [[nodiscard]] double field_gz(const Point3D &r) const { return pfld_.field_gz(r); }
    [[nodiscard]] Point3D field_g(const Point3D &r) const { return pfld_.field_g(r); }

    // Comparisons
    [[nodiscard]] constexpr bool operator==(const Facet3Pt &fct) const noexcept {
        return (pts[0] == fct.pts[0] && pts[1] == fct.pts[1] && pts[2] == fct.pts[2]);
    }
    [[nodiscard]] constexpr bool operator!=(const Facet3Pt &fct) const noexcept {
        return !(*this == fct);
    }

    // Initialization / reinitialization
    void init();
    void init(const Point3D *ppts);
    void init(std::span<const Point3D, 3> ppts);
    void init(const Vertices &ppts);
    void init(const std::vector<Point3D> &ppts);
    void init(const Point3D &pt0, const Point3D &pt1, const Point3D &pt2);
    void init(const Point3D &pt0, const Point3D &pt1, const Point3D &pt2, double density_ccw, double density_cw = 0.0);
    void init(const Point3D &pt0, const Point3D &pt1, const Point3D &pt2, double density_ccw, Point3D dens_grad_ccw);

    void set_opposite_density(double d_density, Point3D v_grad = Point3D()) noexcept;

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
    [[nodiscard]] Body *body() const noexcept { return primary.body; }
    void set_body(Body *b) noexcept { primary.body = b; }
    [[nodiscard]] Body *opposite_body() const noexcept { return opposite.body; }
    void set_opposite_body(Body *b) noexcept { opposite.body = b; }

    [[nodiscard]] double density() const noexcept { return primary.density; }
    void set_density(double d) noexcept { primary.density = d; }
    [[nodiscard]] double opposite_density() const noexcept { return opposite.density; }

public:
    FacetType nType{FacetType::FCT_NORMAL};
    Vertices pts{};
    Vertices v_L{};
    Vertices v_mi{};
    Vertices v_ni{};
    Point3D v_n{};
    std::array<double, 3> len{0.0, 0.0, 0.0};
    std::array<double, 9> g{0.0};

    FacetSide primary{1000.0};
    FacetSide opposite{0.0};

    double dSign{1.0}; // +1 to add, -1 to subtract (delta updating)

    // Modern header-only pfld facet representation
    pfld::Facet<double> pfld_;
};

static_assert(!std::is_polymorphic_v<FacetSide>, "FacetSide must not be polymorphic");
static_assert(!std::is_polymorphic_v<Facet3Pt>, "Facet3Pt must not be polymorphic");

using FacetList = std::vector<Facet3Pt>;
using facetvec = std::vector<Facet3Pt>; // Alias for pfld_UnitTest compatibility

} // namespace mod3d
