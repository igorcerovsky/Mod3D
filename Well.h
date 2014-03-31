#pragma once

#include <afxtempl.h>
#include "afxwin.h"
#include "arrays.h"
#include "myobject.h"


// CWell command target
typedef struct WellLithology {
	int		nType;
	int		nIndex;
	char	strType[50];
	BYTE	r, g, b;		// color
	double	rd, gd, bd;		// color GL
} WELLIT, *PWELLIT;

typedef CArray <WELLIT, WELLIT&> WelLitArray;

class CWell : public CMyObject
{
DECLARE_SERIAL( CWell )

enum DrawChannel{
	drawLeft=0,
	drawRight=1,
	drawMiddle=2
};

public:
// variables
	int		m_nID;				// unique well ID
	ULONG	m_nVersion;			// version
	CString	m_strLocation;		// location
	CString	m_strDescription;	// location

	// positioning
	double	m_x, m_y, m_z;		// location coordinates

	// lithology
	WelLitArray m_welLit;

	// well data
	int		m_nFormat;			// number of items; data fornat x,y,z, .... other items
	int		m_nChanZ;			// depth channel
	int		m_nChanWidth;		// draw width in pixels
	int		m_nDravTag;			// one of the DrawChannel enum
	int		m_nWellWidth;		// draw width in pixels
	int		m_nWellWidth3D;		// draw width in pixels in 3D

	// drawing
	double	m_dRadiusProf;		// radius to display on profile
	double	m_dRadiusLog;		// radius to display log data on profile
	double	m_dRadiusLog3D;		// radius to display log data in 3D
	int		m_nSamplingRing;	// number of segments per ring
	int		m_nSamplingLog;		// draw every m_nSamplingLog value
	double	m_dLogPieStart;		// log pie start in degrees
	double	m_dLogPieEnd;		// log pie end in degrees
	BOOL	m_bSolidLog3D;		// draw log solid

	CStringArray	m_strData;			// item description
	IntArray		m_showChannel;		// show/hide channel 
	PenArray		m_penArray;			// pens for each channel
	DoubleArray		m_data;				// data itself first two rows is STATISTICS !!!!

	// pen
	LOGPEN	m_logPen;

public:
	CWell();
	virtual ~CWell();
	// serialization history
	virtual void Serialize(CArchive& ar);
	void SerializeOld(CArchive& ar);
	virtual int Properties();
	
	//int		Import(void);
	void	DrawGL(void);
	void	IndexWell(void);
	void	DrawProfile(CDC* pDC, double scx, double offx, double scy, double offy, int nTag, double xy, double r=1.0);
	void	DrawMap(CDC* pDC, double scx, double offx, double scy, double offy);
	void	InitPens(void);
};

typedef CTypedPtrArray <CObArray, CWell*> WellPtrArray;


