#pragma once
#include "myobject.h"
#include "Point3D.h"

typedef enum CheckMarkType {
	CHM_POINT = 0,
	CHM_LINE = 1,
};

typedef	CArray<CPoint, CPoint&> PointChArray;					// array of points in Logical coordinates


class CGuideMark :
	public CMyObject
{
DECLARE_SERIAL(CGuideMark)

public:
	CGuideMark(void) { InitFonts(); }
	CGuideMark(const char* strID ) : CMyObject( strID ) { InitFonts(); };
	CGuideMark(const char* strID, const char* strName) : CMyObject( strID, strName) { InitFonts(); };
	~CGuideMark(void);
	virtual void Serialize(CArchive& ar);
	virtual void Draw(CDC* pDC, double scx, double offx, double scy, double offy);
	void		Draw(CDC* pDC, double scx, double offx, double scy, double offy, double z);
	void		DrawMap(CDC* pDC, double scx, double offx, double scy, double offy);
	virtual int Properties();

	void		ShowText( BOOL bShow = TRUE )		{ m_bShowText = bShow; }
	void		SetType(int nType)					{ m_nType = nType; }
	void		InitFonts(void);
	int			FindIntersections(int profOrientation, double xy);
	const		CGuideMark& operator=(const CGuideMark& chm);

public:
	int			m_nType;
	int			m_nCol, m_nRow;
	double		m_x, m_y, m_z;
	BOOL		m_bShowText;
	LOGFONT		m_logFont;
	Pt3DArray	m_pt3DArray;
	LOGPEN		m_logPen;
	Pt3DArray	m_pt3DArrayProf;	// temporary for given profile
	int			m_nProfType;		// temporary

};
