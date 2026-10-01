// Facet3Pt.h: interface for the CFacet3Pt class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Point3D.h"

typedef enum FacetType{
	FCT_NULL = 0,			// NULL facet ignored always
	FCT_NORMAL = 1,
	FCT_SIDE = 2,
	FCT_OUTER = 3,
	FCT_EXTENDED = 4,
	FCT_TOP = 5,
	FCT_BOT = 6,
	FCT_OUTERBOUNDARY = 7,
	FCT_NULLOPOSIT = 8
};

class CBody;

class CFacet3Pt : public CObject  
{
public:
	CFacet3Pt();
	// copy constructor
	CFacet3Pt(const CFacet3Pt& fct) 
	{
		nType = fct.nType;
		v_n = fct.v_n;
		pBody = fct.pBody;
		pBodyOpos = fct.pBodyOpos;
		density = fct.density;
		densityOpos = fct.densityOpos;
		v_densGrad = fct.v_densGrad;
		v_densGradOpos = fct.v_densGradOpos;
		bLin = fct.bLin;
		bLinOpos = fct.bLinOpos;
		dSign = fct.dSign;
		for(int i=0; i < 3; i++) {
			pts[i] = fct.pts[i];
			v_L[i] = fct.v_L[i];
			v_mi[i] = fct.v_mi[i];
			v_ni[i] = fct.v_ni[i];
			len[i] =fct.len[i];
		}
	}
	virtual ~CFacet3Pt();

	//!!! empty operator!!!
	const CFacet3Pt& operator=(const CFacet3Pt& fct)
	{
		nType = fct.nType;
		v_n = fct.v_n;
		pBody = fct.pBody;
		pBodyOpos = fct.pBodyOpos;
		density = fct.density;
		densityOpos = fct.densityOpos;
		v_densGrad = fct.v_densGrad;
		v_densGradOpos = fct.v_densGradOpos;
		bLin = fct.bLin;
		bLinOpos = fct.bLinOpos;
		dSign = fct.dSign;
		for(int i=0; i < 3; i++) {
			pts[i] = fct.pts[i];
			v_L[i] = fct.v_L[i];
			v_mi[i] = fct.v_mi[i];
			v_ni[i] = fct.v_ni[i];
			len[i] = fct.len[i];
		}
		return *this;
	}

	BOOL operator==(CFacet3Pt fct) const
	{
		return (pts[0] == fct.pts[0] && pts[1] == fct.pts[1] && pts[2] == fct.pts[2]);
	}

public:
	void	Init();
	void	Init(CPoint3D *ppts);
	void	Init(CPoint3D &pt0, CPoint3D &pt1, CPoint3D &pt2);
	void	Init(CPoint3D &pt0, CPoint3D &pt1, CPoint3D &pt2, double densityCCW, double densityCW = 0.0);
	void	Init(CPoint3D &pt0, CPoint3D &pt1, CPoint3D &pt2, double densityCCW, CPoint3D v_densGradCCW);

	void	SetOpositDensity( double dDensity, CPoint3D v_grad = CPoint3D());

	double*	FldVladoAll(CPoint3D &v_r, CPoint3D &v_rGrad);
	void	FldSpherVlado(CPoint3D &v_r, CPoint3D &v_Grv);
	void	FldSpherGS(CPoint3D &v_r, CPoint3D v_M, CPoint3D &v_Mag, CPoint3D &v_Grv);
	void	FldGS(CPoint3D &v_r, CPoint3D v_M, CPoint3D &v_Mag, CPoint3D &v_Grv);
	void	FldGS(CPoint3D &v_r, CPoint3D v_M, CPoint3D &v_Mag, double dSign = 1.0 );
	void	FldVlado(CPoint3D &v_r, CPoint3D &v_Grv);
	void	FldVlado(CPoint3D &v_r, CPoint3D &v_Grv, CPoint3D ro, double ro0 );
	void	FldVladoGrd(CPoint3D &v_r, double refDensity, double &xx, double &yy, double &zz, double &xy, double &xz, double &yz );
	BOOL	IsOposit(const CFacet3Pt &fct);
	BOOL	IsNull()				{ return nType == FCT_NULL;	}
	BOOL	IsOuter()				{ return nType == FCT_OUTER;}
	void	SetType( int type )		{ nType = type;	}
	int		GetType()				{ return nType;	}
	double	GetSign()				{ return dSign; }
	void	SetSign(double sign)	{ dSign=sign; }

	int			nType;			// facet type
	CPoint3D	pts[3];			// points
	CPoint3D	v_L[3];			// array of vector l vectors; l = pts[i+1] - pts[i]	
	CPoint3D	v_mi[3];		// array of unit vector mi vectors; mi = Unit(pts[i+1] - pts[i])	
	CPoint3D	v_ni[3];		// arrys of unit ni vectors; ni = v_mi[i] / v_n;
	CPoint3D	v_n;			// facet unit normal vector
	double		len[3];			// arry of side lengths
	double		g[9];			// results gx, gy, gz, gxx, gyy, gzz, gxy, gxz, gyz

	// physical parameters
	// under bodyCCW is understand when facet is seen CCW from outside of the body
	double		density;		// density of pBodyCCW
	CPoint3D	v_densGrad;		// density gradien of bodyCCW
	BOOL		bLin;			// if compute withLinear Density
	// bodyCW -> oposit body, facet is ordered CW
	double		densityOpos;	// density of oposit body
	CPoint3D	v_densGradOpos;	// density gradien of bodyCCW
	BOOL		bLinOpos;		// if compute with linear density

	// facet sign for updating
	double		dSign;		// m_dSign == -1 to remove field; m_dSign == 1 to add field

	// temporary variables
	CPoint3D	tmpFld;			// field from body which facet is seen from outside CCW
	CPoint3D	tmpFldGrd;		// gradients
	CPoint3D	tmpFldOpos;		// oposit facet

	// temporary pointers
	CBody*		pBody;			// pointer to body to which facet belongs to
	CBody*		pBodyOpos;		// pointer to "oposit facet body"

protected:
	double SolidAngle(CPoint3D *spts);
public:
	double GetMeanElevation(void);
	CPoint3D Centroid(void);
	// checks if this facet contains  given point
	CPoint3D* ContainsVertex(const CPoint3D* pt);
	// reverses the facet, changes opositBody to normal body
	void Reverse(void);

	void Compute(
		double* gx, double* gy, double* gz,
		double* gxx, double* gyy, double* gzz, double* gxy, double* gxz, double* gyz,
		double* mx, double* my, double* mz,
		double& dRefDens, double& dRefDensOrg, CPoint3D& v_refDens, int nRdm,
		CPoint3D& v_rGrv, CPoint3D& v_rTen, CPoint3D& v_rMag,
		double& dUnitGrv, double& dUnitMag, double& dUnitTns );

};

typedef	CList<CFacet3Pt, CFacet3Pt&> FacetList;
