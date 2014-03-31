// Body.h: interface for the CBody class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include "Point3D.h"
#include "PotField.h"
#include <afxtempl.h>
#include "Facet3Pt.h"
#include "Grid.h"

typedef struct CompRowColTag{
	int rowStart;
	int rowEnd;
	int colStart;
	int colEnd;
} COMPROWCOL;

// structure containing pointers to grid data for field computing
struct BodyComp {
	int	nGrdCol;		// number of grid columns
	int	nGrdRow;		// number of grid rows
	int	nRowStep;		// for fast computing
	int	nColStep;
	COMPROWCOL cmpRC;
	double x0, y0, xSize, ySize;
	double **pObs;		// points to grid data where to compute field
	double **pGx;
	double **pGy;
	double **pGz;
	double **pMx;
	double **pMy;
	double **pMz;
	double **pG;
	double **pM;
	BOOL	bExtend;
	double  dExtend;
	double  dUnits;
	BOOL	bGrv;
	BOOL	bMag;
};



/////////////////////////////////////////////////////////////////////////////////////////////////////
//****** CBody class ********************************************************************************
class CBody : public CObject  
{
friend class CMod3DDoc;
DECLARE_SERIAL( CBody )
public:
	CBody();
	virtual ~CBody();
	virtual void Serialize(CArchive &ar);


protected:
	int			m_nID;				// unique ID
	int			m_nIndex;			// index in body collection. !!!temporary

	// body parameters
	CString		m_strName;			// body name
	CString		m_strDescription;	// decription
	BOOL		m_bShow;			// is body visible
	BOOL		m_bLocked;			// is body visible
	BOOL		m_bFill;			// filled
	BOOL		m_bActive;			// active for computation

	// physical properties
	double		m_dDensity;			// density
	CPoint3D	m_vDensGrad;		// magnetization vector
	CPoint3D	m_vDensOrg;			// density origin vector
	double		m_dSusc;			// Susceptibility
	CPoint3D	m_vMagVector;		// magnetization vector
	CPoint3D	m_vMagRem;			// remanent magnetization

	// drawing
	LOGPEN		m_lpProf;			// pen for profile drawing
	LOGPEN		m_lpProfPrev;		// pen for profile drawing, body on next profile
	LOGPEN		m_lpProfNext;		// pen for profile drawing, body on Previous profile
	LOGBRUSH	m_lbProf;			// brush for profile drawing
	LOGPEN		m_lpMap;			// pen for profile drawing
	LOGBRUSH	m_lbMap;			// brush for profile drawing
	float		m_fAlpha;			// transparency
	BOOL		m_bTransparent;		// is body transparent

public:
	inline int		GetID()									{ return m_nID; }
	inline void		SetID(int nID)							{ m_nID = nID; }
	inline int		GetIndex()								{ return m_nIndex;}
	inline void		SetIndex(int nIndex)					{ m_nIndex = nIndex;}
	inline CString	GetStrId()								{ CString strID; strID.Format("Body %d", m_nID); return strID;}
	inline CString	GetStrIdName()							{ CString strID; strID.Format("Body %d - ", m_nID); strID += m_strName; return strID;}

	inline BOOL		IsVisisble()							{return m_bShow;}
	inline int 		Show(BOOL bVisible = TRUE)				{int old = m_bShow; m_bShow = bVisible;	return old;}
	inline int		ShowInv()								{m_bShow = !m_bShow;	return m_bShow;}
	void			Properties();
	void			ComputeMagnetizationVector(CPoint3D	vIndFld);
	inline BOOL		IsEmpty();
	inline CString	GetName()								{ return m_strName;		}
	inline void		SetName(CString strName)				{ m_strName = strName;	}
	inline BOOL		IsLocked()								{ return m_bLocked;		}
	inline void		Lock( BOOL bLock = true)				{ m_bLocked = bLock;	}
	inline int		LockInv()								{ m_bLocked = !m_bLocked;	return m_bLocked;}
	inline int		Fill( int bFill )						{ int old = m_bFill; m_bFill = bFill; return old;		}
	inline int		FillInv()								{ m_bFill = !m_bFill; return m_bFill;		}
	inline int		IsFilled()								{ return m_bFill;										}
	inline int		Activate( int bActive )					{ int old = m_bActive; m_bActive = bActive; return old;	}
	inline int		ActivateInv()							{ m_bActive = !m_bActive; return m_bActive;	}
	inline int		IsActive()								{ return m_bActive;										}

	// physical properties
	inline double	GetSusceptibility()						{ return m_dSusc;										}
	inline double	GetDensity()							{ return (m_bActive ? m_dDensity : 0.0);				}
	inline void		SetDensity(double dens)					{ m_dDensity = dens;					}
	inline void		SetDensityGradient(CPoint3D grad)		{ m_vDensGrad = grad;									}
	inline void		SetDensityOrigo(CPoint3D origo)			{ m_vDensOrg = origo;									}
	inline double	GetDensityAtOrigin()					{ return (m_dDensity - m_vDensGrad*m_vDensOrg);			}
	inline CPoint3D	GetDensityGradient()					{ return m_vDensGrad;									}
	inline CPoint3D	GetDensityOrigo()						{ return m_vDensOrg;									}
	inline CPoint3D	GetMagnetizationVector()				{ return m_vMagVector;									}

	// drawing parameters simple version
	inline COLORREF	GetColor()								{ return m_lpProf.lopnColor;}
	inline void		GetColor(BYTE &r, BYTE &g, BYTE &b)		{ r = GetRValue(m_lpProf.lopnColor);g = GetGValue(m_lpProf.lopnColor);b = GetBValue(m_lpProf.lopnColor);}
	inline void		GetColor(double &r, double &g, double &b)		{ r = GetRValue(m_lpProf.lopnColor)/255.0;g = GetGValue(m_lpProf.lopnColor)/255.0;b = GetBValue(m_lpProf.lopnColor)/255.0;}
	inline void		GetColor(double &r, double &g, double &b, double &a)		{ r = GetRValue(m_lpProf.lopnColor)/255.0;g = GetGValue(m_lpProf.lopnColor)/255.0;b = GetBValue(m_lpProf.lopnColor)/255.0; (m_bShow==FALSE) ? a=0.0 : (m_bTransparent==TRUE ? a = m_fAlpha : a=1.0) ;}
	inline COLORREF	GetColorFill()							{ return m_lbProf.lbColor;}
	inline void		GetColorFill(double &r, double &g, double &b)		{ r = GetRValue(m_lbProf.lbColor)/255.0;g = GetGValue(m_lbProf.lbColor)/255.0;b = GetBValue(m_lbProf.lbColor)/255.0;}
	inline void		GetColorFill(double &r, double &g, double &b, double &a)		{ r = GetRValue(m_lbProf.lbColor)/255.0;g = GetGValue(m_lbProf.lbColor)/255.0;b = GetBValue(m_lbProf.lbColor)/255.0; (m_bShow==FALSE) ? a=0.0 : (m_bTransparent==TRUE ? a = m_fAlpha : a=1.0) ;}
	inline PLOGPEN	GetPLogPen()							{ return &m_lpProf; }
	inline void		SetLogPen(LOGPEN lp)					{ m_lpProf = lp;	}
	inline PLOGPEN	GetPLogPenPrev()						{ return &m_lpProfPrev; }
	inline void		SetLogPenPrev(LOGPEN lp)				{ m_lpProfPrev = lp;	}
	inline PLOGPEN	GetPLogPenNext()						{ return &m_lpProfNext; }
	inline void		SetLogPenNext(LOGPEN lp)				{ m_lpProfNext = lp;	}
	inline PLOGBRUSH	GetPLogBrush()							{ return &m_lbProf; }
	inline void		SetLogBrush(LOGBRUSH lb)				{ m_lbProf = lb;	}
	// drawing parameters profile version
	inline PLOGPEN	GetPLogPenPrf()							{ return &m_lpProf; }
	inline void		SetLogPenPrf(LOGPEN lp)					{ m_lpProf = lp;	}
	inline PLOGBRUSH	GetPLogBrushPrf()						{ return &m_lbProf; }
	inline void		SetLogBrushPrf(LOGBRUSH lb)				{ m_lbProf = lb;	}
	// drawing parameters map version
	inline PLOGPEN	GetPLogPenMap()							{ return &m_lpMap; }
	inline void		SetLogPenMap(LOGPEN lp)					{ m_lpMap = lp;	}
	inline PLOGBRUSH	GetPLogBrushMap()						{ return &m_lbMap; }
	inline void		SetLogBrushMap(LOGBRUSH lb)				{ m_lbMap = lb;	}
	// transparency
	virtual float	GetTransparency()					{ return m_fAlpha;}
	virtual void	SetTransparency( float fAlpha)		{ m_fAlpha = fAlpha;} 
	virtual BOOL	IsTransparent()						{ return m_bTransparent;}
	virtual BOOL	IsVisibleTransparent()				{ return (m_bTransparent && (m_fAlpha!=0.0f) && m_bShow);}
	virtual void	SetTransparent(BOOL bTransparent)	{ m_bTransparent = bTransparent;}

};

typedef CTypedPtrList<CObList, CBody*> BodyList;
typedef	CTypedPtrArray<CObArray, CBody*> BodyPtrArray;
