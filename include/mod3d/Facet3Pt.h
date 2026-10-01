#pragma once

#include "mod3d/Point3D.h"
#include <vector>
#include <memory>

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
 * Faithfully preserves the analytical potential field formulations (Vlado Pohanka,
 * Singh-Guptasarma) from legacy Mod3D.
 */
class Facet3Pt {
public:
    Facet3Pt();
    Facet3Pt(const Point3D &pt0, const Point3D &pt1, const Point3D &pt2);
    Facet3Pt(const Point3D &pt0, const Point3D &pt1, const Point3D &pt2, double densityCCW, double densityCW = 0.0);
    virtual ~Facet3Pt() = default;

    Facet3Pt(const Facet3Pt &fct) = default;
    Facet3Pt &operator=(const Facet3Pt &fct) = default;

    bool operator==(const Facet3Pt &fct) const {
        return (pts[0] == fct.pts[0] && pts[1] == fct.pts[1] && pts[2] == fct.pts[2]);
    }

    void Init();
    void Init(const Point3D *ppts);
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

    // Direct compatibility wrappers for pfld_UnitTest
    void Fld_G(const Point3D &v_r, Point3D &v_Grv) const { FldVlado(v_r, v_Grv); }
    void Fld_G(const Point3D &v_r, Point3D ro, double ro0, Point3D &v_Grv) const { FldVlado(v_r, v_Grv, ro, ro0); }
    void Fld_Gz(const Point3D &v_r, double &gz) const {
        Point3D g(0, 0, 0);
        FldVlado(v_r, g);
        gz += g.z;
    }
    void Fld_Gz(const Point3D &v_r, Point3D ro, double ro0, double &gz) const {
        Point3D g(0, 0, 0);
        FldVlado(v_r, g, ro, ro0);
        gz += g.z;
    }
    void FldGS_Gz(const Point3D &v_r, double &gz) const {
        Point3D m(0, 0, 0), g(0, 0, 0);
        FldGS(v_r, Point3D(0, 0, 0), m, g);
        gz += g.z;
    }

    // Facet management
    bool IsOposit(const Facet3Pt &fct) const;
    bool IsNull() const { return nType == FacetType::FCT_NULL; }
    bool IsOuter() const { return nType == FacetType::FCT_OUTER; }
    void SetType(FacetType type) { nType = type; }
    FacetType GetType() const { return nType; }
    double GetSign() const { return dSign; }
    void SetSign(double sign) { dSign = sign; }

    double SolidAngle(const Point3D *spts) const;
    double GetMeanElevation() const;
    Point3D Centroid() const;
    const Point3D *ContainsVertex(const Point3D *pt) const;
    void Reverse();
    const Point3D &Normal() const { return v_n; }

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
    Point3D pts[3];
    Point3D v_L[3];
    Point3D v_mi[3];
    Point3D v_ni[3];
    Point3D v_n;
    double len[3]{0.0, 0.0, 0.0};
    double g[9]{0.0};

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
};

using FacetList = std::vector<Facet3Pt>;
using facetvec = std::vector<Facet3Pt>; // Alias for pfld_UnitTest compatibility

} // namespace mod3d
