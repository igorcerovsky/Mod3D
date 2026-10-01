#pragma once

#include "myobject.h"
#include "Grid.h"
#include <math.h>
#include "Arrays.h"
#include "gl/gl.h"
#include "gl/glu.h"

class CGlSurface :
	public CMyObject
{
public:
	enum SrfPlacement {
		plcNone=0,		// not used
		plcOnRelief=1,	// on Relief
		plcFltSrf=2,	// floating surface
		plcFltFlat=3	// floating flat
	};

public:
	CGlSurface(void);
	~CGlSurface(void);

// functions
	//void	DrawGL();
	void	SetSize(int nRows, int nCols)		{ m_nRows=nRows; m_nCols=nCols;}
	
	virtual float	GetAlpha()							{ return m_fAlpha;}
	virtual	void	SetAlpha(float fAlpha)				{ m_fAlpha = fAlpha;}
	virtual BOOL	IsSolid()							{ return m_bSolid;}
	virtual void	SetSolid(BOOL bSolid = TRUE)		{ m_bSolid = bSolid;}

	void	SetSourceID(CString strID)		{ m_strIdSource = strID;}
	CString	GetSourceID()					{ return m_strIdSource; }

	void	SetPlacement(int nPlc=plcNone)		{ m_nPlacement=nPlc;}
	void	GetPlacement(int &nPlc)				{ nPlc=m_nPlacement;}
	int		GetPlacement()						{ return m_nPlacement;}
	
	int		GetTexture( )						{ return m_nTex;}

	void	SetDataRange(double min, double max)	{ m_dMin=min; m_dMax=max;}
	void	GetDataRange(double &min, double &max)	{ min=m_dMin; max=m_dMax;}
	
	void	SetLevels(double min, double max)	{ m_dLevelMin=min; m_dLevelMax=max;}
	void	GetLevels(double &min, double &max)	{ min=m_dLevelMin; max=m_dLevelMax;}
	
	
	double	GetH(double rel, double h, double min, double max) {
		switch(m_nPlacement) {
			case plcOnRelief:	return rel; break;
			case plcFltSrf:		return m_dLevelMin+(m_dLevelMax-m_dLevelMin)*(h-min)/(max-min); break;
			case plcFltFlat:	return (m_dLevelMax+m_dLevelMin)/2.0; break;
			default:			return rel;
		}
	}

// data
	CString	m_strIdSource;	// source image ID
	// surface data
	int			m_nRows;
	int			m_nCols;
	DoubleArray	m_srfData;			// array of coordinates, normals, color index {x,y,z, nx,ny,nz, c, ...}
	int			m_nSrfFormat;		// number of entries per point

	// texture
	FloatArray	m_tex1D;			// 1D GL textture - "color table"
	int			m_nTexWidth;		// number of texture rows; must be power of 2
	int			m_nTexFormat;		// texture format GL_RGBA
	GLuint		m_nTex;				// unique texture ID
	GLuint		m_nDispLst;			// display list

	float		m_fAlpha;			// transparency
	
	BOOL		m_bSolid;			// if TRUE solid model, else wireframe

	// surface placement
	int			m_nPlacement;
	double		m_dLevelMin;			// elevation level for floating surface && flat surface
	double		m_dLevelMax;			// elevation level for

	// data range
	double		m_dMin;
	double		m_dMax;

	void	DrawGL(BOOL bRel=FALSE);
	void	UpdateTexture();
	int		IsEmpty(void);

};
