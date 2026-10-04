#pragma once

#include "mod3d/Point3D.h"
#include "pfld/facet.hpp"
#include <vector>
#include <array>
#include <memory>
#include <span>
#include <algorithm>

namespace mod3d {

class Body;

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
    // ========================================================================
    // Lifecycle & Rule of 5
    // ========================================================================
    constexpr Facet3Pt() noexcept = default;
    Facet3Pt(const Point3D &pt0, const Point3D &pt1, const Point3D &pt2);
    Facet3Pt(const Point3D &pt0, const Point3D &pt1, const Point3D &pt2, double densityCCW, double densityCW = 0.0);
    explicit Facet3Pt(std::span<const Point3D, 3> points);
    explicit Facet3Pt(const std::array<Point3D, 3> &points);
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
    [[nodiscard]] constexpr std::array<Point3D, 3> &points() noexcept { return pts; }
    [[nodiscard]] constexpr const std::array<Point3D, 3> &points() const noexcept { return pts; }
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

    // ========================================================================
    // Legacy API (Compatibility wrappers and legacy potential field functions)
    // ========================================================================
    void Init();
    void Init(const Point3D *ppts);
    void Init(std::span<const Point3D, 3> ppts);
    void Init(const std::array<Point3D, 3> &ppts);
    void Init(const std::vector<Point3D> &ppts);
    void Init(const Point3D &pt0, const Point3D &pt1, const Point3D &pt2);
    void Init(const Point3D &pt0, const Point3D &pt1, const Point3D &pt2, double densityCCW, double densityCW = 0.0);
    void Init(const Point3D &pt0, const Point3D &pt1, const Point3D &pt2, double densityCCW, Point3D v_densGradCCW);

    void SetOpositDensity(double dDensity, Point3D v_grad = Point3D());

    // Analytical potential field formulas
    void FldVlado(const Point3D &v_r, Point3D &v_Grv) const;
    void FldVlado(const Point3D &v_r, Point3D &v_Grv, Point3D ro, double ro0) const;
    void FldVladoGrd(const Point3D &v_r, double refDensity,
                     double &gxx, double &gyy, double &gzz,
                     double &gxy, double &gxz, double &gyz) const;

    void FldGS(const Point3D &v_r, Point3D v_M, Point3D &v_Mag, Point3D &v_Grv) const;
    void FldGS(const Point3D &v_r, Point3D v_M, Point3D &v_Mag, double dSign = 1.0) const;

    void FldSpherVlado(const Point3D &v_r, Point3D &v_Grv) const;
    void FldSpherGS(const Point3D &v_r, Point3D v_M, Point3D &v_Mag, Point3D &v_Grv) const;

    // Direct compatibility wrappers for pfld_UnitTest and legacy Mod3D
    void Fld_G(const Point3D &v_r, Point3D &v_Grv) const { FldVlado(v_r, v_Grv); }
    void Fld_G(const Point3D &v_r, Point3D ro, double ro0, Point3D &v_Grv) const { FldVlado(v_r, v_Grv, ro, ro0); }
    void Fld_Gz(const Point3D &v_r, double &gz) const {
        Point3D g_vec(0, 0, 0);
        FldVlado(v_r, g_vec);
        gz += g_vec.z;
    }
    void Fld_Gz(const Point3D &v_r, Point3D ro, double ro0, double &gz) const {
        Point3D g_vec(0, 0, 0);
        FldVlado(v_r, g_vec, ro, ro0);
        gz += g_vec.z;
    }
    void FldGS_Gz(const Point3D &v_r, double &gz) const {
        Point3D m(0, 0, 0), g_vec(0, 0, 0);
        FldGS(v_r, Point3D(0, 0, 0), m, g_vec);
        gz += g_vec.z;
    }

    [[nodiscard]] bool IsOposit(const Facet3Pt &fct) const noexcept { return is_opposite(fct); }
    [[nodiscard]] bool IsNull() const noexcept { return is_null(); }
    [[nodiscard]] bool IsOuter() const noexcept { return is_outer(); }

    void SetType(FacetType type) noexcept { set_type(type); }
    [[nodiscard]] FacetType GetType() const noexcept { return type(); }

    [[nodiscard]] double GetSign() const noexcept { return sign(); }
    void SetSign(double s) noexcept { set_sign(s); }

    [[nodiscard]] double SolidAngle(const Point3D *spts) const { return solid_angle(spts); }
    [[nodiscard]] double SolidAngle(std::span<const Point3D, 3> spts) const { return solid_angle(spts); }

    [[nodiscard]] double GetMeanElevation() const noexcept { return mean_elevation(); }
    [[nodiscard]] Point3D Centroid() const noexcept { return centroid(); }

    [[nodiscard]] const Point3D *ContainsVertex(const Point3D *pt) const noexcept;
    void Reverse() { reverse(); }

    [[nodiscard]] const Point3D &Normal() const noexcept { return normal(); }

    // High-level field calculation with reference model and body interaction
    void Compute(
        double *gx, double *gy, double *gz,
        double *gxx, double *gyy, double *gzz, double *gxy, double *gxz, double *gyz,
        double *mx, double *my, double *mz,
        double &dRefDens, double &dRefDensOrg, Point3D &v_refDens, int nRdm,
        Point3D &v_rGrv, Point3D &v_rTen, Point3D &v_rMag,
        double &dUnitGrv, double &dUnitMag, double &dUnitTns);

public:
    FacetType nType{FacetType::FCT_NORMAL};
    std::array<Point3D, 3> pts{};
    std::array<Point3D, 3> v_L{};
    std::array<Point3D, 3> v_mi{};
    std::array<Point3D, 3> v_ni{};
    Point3D v_n{};
    std::array<double, 3> len{0.0, 0.0, 0.0};
    std::array<double, 9> g{0.0};

    double density{1000.0};
    Point3D v_densGrad{0, 0, 0};
    bool bLin{false};

    double densityOpos{0.0};
    Point3D v_densGradOpos{0, 0, 0};
    bool bLinOpos{false};

    double dSign{1.0}; // +1 to add, -1 to subtract (delta updating)

    // Body associations
    Body *pBody{nullptr};
    Body *pBodyOpos{nullptr};

    // Modern header-only pfld facet representation
    pfld::Facet<double> pfld_;
};

using FacetList = std::vector<Facet3Pt>;
using facetvec = std::vector<Facet3Pt>; // Alias for pfld_UnitTest compatibility

} // namespace mod3d
