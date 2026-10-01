#pragma once

#include "mod3d/Point3D.h"
#include <vector>

namespace mod3d {

enum class Formula {
    POHANKA = 0,
    GUPTASARMA_SINGH = 1
};

constexpr double PI_VAL = 3.1415926535897932384626433832795;
// Gravitational constant G in SI units (m^3 kg^-1 s^-2)
// Updated in pfld_UnitTest to CODATA modern value 6.673848e-11
constexpr double GRAV_CONST = 6.673848e-11;
constexpr double GRAV_CONST_LEGACY_1999 = 6.67259e-11;
constexpr double EPS_VAL = 1.0e-12;

// General facet field computation
void FacetField(Point3D &v_r, Point3D *pts, int n, Point3D v_M, Point3D &v_Mag, Point3D &v_Grv, Formula nFormula);
void FacetField(Point3D *pts, int n, Point3D v_M, Point3D &v_Mag, Point3D &v_Grv, Formula nFormula);

// Pohanka polygon gravity field
Point3D FacetPohanka(Point3D &v_r, Point3D *pts, int n);
Point3D FacetPohanka(Point3D *pts, int n);
Point3D FacetPohankaPnt(Point3D &v_r, Point3D **pts, int n);
Point3D FacetPohankaPnt(Point3D **pts, int n);

// Singh & Guptasarma magnetic and gravity field
void FacetGS(Point3D *pts, int n, Point3D v_M, Point3D &v_Mag, Point3D &v_Grv);
void FacetGS(Point3D &v_r, Point3D *pts, int n, Point3D v_M, Point3D &v_Mag, Point3D &v_Grv);

// Solid angle computation
double SolidAngle(Point3D *pts, int n, Point3D &v_u);

// Ambient field and magnetization
Point3D AmbientField(double iI = 60.0, double iD = 0.0, double iS = 50000.0);
Point3D MagnetizationVector(double susc = 0.01, double iI = 60.0, double iD = 0.0, double iS = 50000.0,
                            double rI = 0.0, double rD = 0.0, double rS = 0.0);

// Total field anomaly delta T
double Mag_dT(Point3D v_mag, Point3D v_af, double hInt);
double Mag_dT(double mx, double my, double mz, double afx, double afy, double afz, double hInt);

} // namespace mod3d
