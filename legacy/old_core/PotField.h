#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "Point3D.h"
#include <afxtempl.h>

typedef enum Formula{
	POHANKA,
	GUPTASARMA_SINGH
};

#define PI 3.1415926535897932384626433832795

// general routines
void FacetField(CPoint3D &v_r, CPoint3D *pts, int n, CPoint3D v_M, CPoint3D &v_Mag, CPoint3D &v_Grv, int nFormula);
void FacetField(CPoint3D *pts, int n, CPoint3D v_M, CPoint3D &v_Mag, CPoint3D &v_Grv, int nFormula);

CPoint3D FacetPohanka(CPoint3D &v_r, CPoint3D *pts, int n);
CPoint3D FacetPohanka(CPoint3D *pts, int n);
CPoint3D FacetPohankaPnt(CPoint3D &v_r, CPoint3D **pts, int n);
CPoint3D FacetPohankaPnt(CPoint3D **pts, int n);

void	FacetGS(CPoint3D *pts, int n, CPoint3D v_M, CPoint3D &v_Mag, CPoint3D &v_Grv);
void	FacetGS(CPoint3D &v_r, CPoint3D *pts, int n, CPoint3D v_M, CPoint3D &v_Mag, CPoint3D &v_Grv);
void	FacetGSPnt(CPoint3D **pts, int n, CPoint3D v_M, CPoint3D &v_Mag, CPoint3D &v_Grv);
void	CFacetGSPnt(CPoint3D &v_r, CPoint3D **pts, int n, CPoint3D v_M, CPoint3D &v_Mag, CPoint3D &v_Grv);

// aditional routines
double	SolidAngle(CPoint3D *pts, int n, CPoint3D &v_u);
double	SolidAnglePnt(CPoint3D **pts, int n, CPoint3D &v_u);

// magnetization routines
CPoint3D	MagnetizationVector(double susc=0.01, double iI=60, double iD=0, double iS=50000, double rI=0, double rD=0, double rS=0);
CPoint3D	AmbientField(double iI=60, double iD=0, double iS=50000);
double		Mag_dT(CPoint3D v_mag, CPoint3D v_af, double hInt);
double		Mag_dT(double mx, double my, double mz, double afx, double afy, double afz, double hInt);
