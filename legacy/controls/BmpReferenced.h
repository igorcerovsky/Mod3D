#pragma once
#include "myobject.h"
#include <atlimage.h>


typedef enum ImageType{
	IMG_NON=-1,
	IMG_MAP=0,		// map image
	IMG_PRF=1,		// profile image
	IMG_ARB=2,		// arbitrary oriented profile
};

class CImgReferenced : public CMyObject
{
DECLARE_SERIAL( CImgReferenced )
public:
	CImgReferenced(void);
	~CImgReferenced(void);
	virtual void Serialize(CArchive& ar);
	virtual int	Properties(void);
	BOOL	Open(void);
	BOOL	Flip();
	BYTE	GetOpacity()	{ return m_nOpacity;}
	int		GetType()		{ return m_nType;	}

	CImage		m_img;				// image itself		
	CImage		m_imgFlipH;			// image horizontaly flipped
	CString		m_strFilePath;		// file pathe to image
	CString		m_strFilePathRelative;		// file pathe to image
	int			m_nType;			// map, profile
	BOOL		m_bVisibleRow;		// by type IMG_PRF the image is visible on the row profile, othervise ignored
	BOOL		m_bVisibleCol;		// by type IMG_PRF the image is visible on the column profile, othervise ignored

	double		m_xs, m_ys, m_zs;	// top corner one side
	double		m_xe, m_ye, m_ze;	// bottom corner another side
	double		m_h, m_w;			// height, width

	int			m_nProfOrient;		// vertical, horizontal, other
	int			m_nProf;			// profile number
	BYTE		m_nOpacity;

	// font
	LOGFONT		m_logFont;
	LOGPEN		m_logPen;
	CPen		m_pen;

	// temporary
	HTREEITEM	m_hTreeItem;

public:
	int		DrawProfile(CDC* pDC, double xSc, double xOff, double ySc, double yOff, int nProfType, double xy);
	int		DrawMap(CDC* pDC, double xSc, double xOff, double ySc, double yOff);
	// finds if this picture belongs to profile
	BOOL	IsOnProfile(int nProfType, double xy, double x0, double y0, double xs, double ys);
	int		FindIntersection(int nProfType, double xy, double& x, double& y);
	void	GetTopLeft(double& t, double& l);
	void	GetBotRight(double& b, double& r);

	void	GetGlDimensionsPow2(int& w, int& h);
	void	GetPixels(BYTE* pPix, int w, int h, int nFormat);
};
