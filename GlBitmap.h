#pragma once

#include "myobject.h"
#include "BmpReferenced.h"
#include "gl/gl.h"
#include "gl/glu.h"
#include "Grid.h"

class CGlBitmap :
	public CMyObject
{
public:
enum glBitmapType	{
	glBmp_Vert,
	glBmp_Hrz,
	glBmp_Map,
	glBmp_Other
};

public:
	CGlBitmap(void);
	~CGlBitmap(void);

	int		GetIdTex()			{ return m_nIdTex;	}
	void	SetID(int id)		{ m_nIdTex = id;	}

	BOOL	CompileDisplayList(CImgReferenced& img);
	void	DrawGL();
	void	DrawGL(float fAlpha);
	int		GetType()				{ return m_nType;	}
	void	SetType(int nType)		{ m_nType = nType;	}

	void	SetSourceID(CString strID)	{ m_strIdSource = strID;}
	CString	GetSourceID()				{ return m_strIdSource; }

	void	SetBoundRect(float xt, float yt, float zt, float xb, float yb, float zb);

	BOOL	DrawOnrelief(CGrid* pRel, double fAlpha=0.6, double z=0);
	void	CreateTexture2D(CImgReferenced& img);

public:
	CString	m_strIdSource;	// source image ID
	int		m_nType;		// one of the glBitmapType enum
	GLuint	m_nIdTex;
	GLuint	m_nDispLst;
	BYTE*	m_tex;
	int		m_nFormat;

	int		m_w, m_h;
	float	m_xt;		// GL coordinates
	float	m_yt;
	float	m_zt;
	float	m_xb;
	float	m_yb;
	float	m_zb;

	double aE, aW, aS, aN;		//aspects

	float	m_fWidthAspect;
	float	m_fHeightAspect;
};
