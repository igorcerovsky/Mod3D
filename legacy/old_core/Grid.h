// Grid.h: interface for the CGrid class.
//
// opens grid file
// allocates memory
// windows only needed part of a grid
//
// this -> I. Cerovsky 2003
//////////////////////////////////////////////////////////////////////

#pragma once

//#include "fstream.h"
#include <fstream>
#include <afxtempl.h>
#include "Point3D.h"
#include "Arrays.h"
#include "atlimage.h"
#include "math.h"
#include "ClrGrad\gradient.h"

typedef enum DummyExTag{
	DMX_EQUAL = 0,		// ==
	DMX_LESS = 1,		// < vallue is accepted when is less then compare value
	DMX_GRATER = 2,		// >
	DMX_LESSEQ = 3,		// <= vallue is accepted when is less then compare value
	DMX_GRATEREQ = 4	// >=
};

typedef enum GrdType{
	SRF_6 = 0,		// surfer 6.0 grid file
	SRF_7 = 1,		// surfer 7.0 grid file
	GEOS_KX1 = 2	// Geosoft grid kx = 1
};

typedef enum ColorTblTag{
	CTBL_FLD = 0,	// field color table
	CTBL_REL = 1,	// relief
	CTBL_DIF = 2,	// difference
	CTBL_BW = 3,	// black && white
} COLORTBL;

struct GridInfo{
	int		nType;
	CString strFilePath;
	DWORD	nCol;
	DWORD	nRow;
	double	x0;
	double	y0;
	double	xSize;
	double	ySize;
	double	dMinX;
	double	dMinY;
	double	dMinZ;
	double	dMaxX;
	double	dMaxY;
	double	dMaxZ;
	double	dRot;

	BOOL operator==(const GridInfo& gi) {
		return 
			( nCol == gi.nCol && nRow == gi.nRow &&
			x0 == gi.x0 && y0 == gi.y0 &&
			xSize == gi.xSize && ySize == gi.ySize );
	}

	BOOL operator!=(const GridInfo& gi) {
		return !(*this == gi);
	}

};

typedef struct RGBColoTag{
	BYTE r;
	BYTE g;
	BYTE b;
} RGBCOLOR;


typedef struct DblPtTag{
	double x;
	double y;
} DBLPOINT;

typedef struct ContourTag{
	CArray<DBLPOINT, DBLPOINT&> pts;	// array of points
	double		z;						// z value of contour
	COLORREF	color;
} CONTOUR;

typedef CArray<CONTOUR, CONTOUR&> CONTOURS;
typedef CArray<int, int> IntArray;


#define DUMMY  1.7976931348623158e+308 

class CGrid : public CObject  
{
DECLARE_SERIAL( CGrid )

enum  BlankTag{
	bln_inside = 1,
	bln_outside = 0
};

public:		// functions
	CGrid();
	CGrid(DWORD row, DWORD col, double x0, double y0, double xSize, double ySize, double rot=0.0);
	void	Serialize(CArchive &ar);
	void	SerializeOld(CArchive &ar);
	virtual ~CGrid();

	// operators
	void operator+=(double d);
	void operator-=(double d);
	void operator*=(double d);
	void operator/=(double d);

	void operator+=(CGrid* pGrd);
	void operator-=(CGrid* pGrd);
	void operator*=(CGrid* pGrd);
	void operator/=(CGrid* pGrd);

	// coordinate functions
	inline	double	GetXSize()										{return m_xSize;}
	inline	double	GetYSize()										{return m_ySize;}
	inline	double	GetX0()											{return m_x0;}
	inline	double	GetY0()											{return m_y0;}
	inline	void	GetXY0(double &x0, double &y0)					{x0=m_x0; y0=m_y0;}
	inline	double	GetX(int row, int col)							{return	m_x0+(col*m_xSize)*m_dRotCos - (row*m_ySize)*m_dRotSin;}
	inline	double	GetY(int row, int col)							{return	m_y0+(col*m_xSize)*m_dRotSin + (row*m_ySize)*m_dRotCos;}
	inline	void	GetXY(int row, int col, double &x, double&y)	{x=GetX(row, col); y=GetY(row, col);}


	BOOL IsVisible();
	void Show(BOOL bVisible = true)		{m_bVisible = bVisible;}
	void UpdateMinMaxValue();
	void UpdateDerivative();
	void SetModifiedFlag(BOOL bFlag = TRUE);
	void GetMinMaxValue(double &dMin, double &dMax);
	BOOL New( DWORD row, DWORD col, double x0, double y0, double xSize, double ySize, double rot=0.0 );
	BOOL New(CString strFilePath,int nType, DWORD row, DWORD col, double x0, double y0, double z, double xSize, double ySize, double rot=0.0);
	BOOL Open();
	BOOL Open(CString strFilePath, int nType);
	void Close();
	BOOL Save();
	BOOL SaveAs();
	BOOL OpenSrf7Grid(CString strFileName);
	BOOL SaveSrf7Grid(CString strFileName);
	BOOL OpenSrf6Grid(CString strFileName);
	BOOL SaveSrf6Grid(CString strFileName);
	BOOL OpenGeosoftGrid(CString strFileName, double tx, double ty, double cr);
	void SetInfo(CString strFilePath, int nType);
	BOOL IsEmpty();
	double** GetData();
	GridInfo GetInfo();
	GridInfo ReadSrf6Info(CString strFilePath);
	BOOL CGrid::GetInfoBrowse(GridInfo& gi);
	GridInfo GetInfoBrowse();
	GridInfo GetInfo(CString strFilePath, int nType);
	GridInfo ReadSrf7Info(CString strFilePath);
	int		SetColorTable(int nTag);
	double	GetRMS()							{ return	m_dRMS;}
	int		GetRMSTrend()						{ return	( (m_dRMS-m_dRMSold) > 0 ) ? 0 : 1;}
	double	GetDrv()							{ return	m_dDrv;}
	double	GetDrv(int row, int col);
	int		GetDrvTrend()						{ return	( (m_dDrv-m_dDrvOld) > 0 ) ? 0 : 1;}
	BOOL	GetPt3D( int row, int col, CPoint3D& pt);
	BOOL	IsDummy(int row, int col)			{ if( m_pData[row][col]== DUMMY)  return TRUE; return FALSE;}
	int		GetRows()							{ return m_nRow;			}
	int		GetCols()							{ return m_nCol;			}
	CString GetFilePath()						{ return m_strFilePath;		}
	int		GetNodesNumber()					{ return m_nRow*m_nCol;		}
	double	GetMean()							{ return m_dMean;			}
	double	GetMeanP()							{ return m_dMeanP;			}

protected:		// variables
	int		m_nVersion;
	BOOL	m_bAllocated;
	BOOL	m_bModified;			// is modified for saving
	BOOL	m_bModifiedMinMax;		// is min and max valuaes are up to date
	BOOL	m_bModifiedDrv;			// is "derivative" up to date
	BOOL	m_bModifiedContours;	// comtour are up to date
	BOOL	m_bModifiedImage;		// image is up to date
	BOOL	m_bModifiedColorTable;	// color table modified
	BOOL	m_bFlag;
	BOOL	m_bVisible;
	DWORD	m_nRow, m_nCol;			// number of elements in rows and columns -> rectangular grid
	double	m_x0, m_y0;				// coordinates of lower left corner 
	double	m_xSize, m_ySize;		// grid cell size
	double	m_dRot;					// grid rotation in degrees, positive counterclockwise from positive x axis
	double	m_dRotCos, m_dRotSin;	// precomputed sin(), cos()
	double	m_dMinX, m_dMaxX;
	double	m_dMinY, m_dMaxY;
	double	m_dMinZ, m_dMaxZ;
	double	**m_pData;				// grid data
	int		m_nType;				// grid type
	CString m_strFilePath;			// file path
	CString m_strID;
	double	m_dRMS;					// RMS of this grid
	double	m_dDrv;					// fake "derivative" of this grid
	double	m_dDrvOld;				// fake "derivative" of this grid
	double	m_dRMSold;				// RMS of this grid
	double	m_dMean;				// grid arithmetic mean
	double	m_dMeanP;				// grid mean using probability
	int		m_nClrTbl;				// color table for field and contours
	int		m_nColMax, m_nRowMax;	// location of maximum 
	int		m_nColMin, m_nRowMin;	// location of minimum 


	// contouring part
	CONTOURS m_cntrs;		// contours structure
	BOOL	m_bContoured;	// is grid already contoured
	BOOL	m_bShowContour;
	int		m_nContNum;


	// color table
	CGradient	m_clrGrad;
	CArray<RGBQUAD, RGBQUAD> m_clrTbl;	// temporary color table for color division
	CArray<RGBQUAD, RGBQUAD> m_rgbTable;
	CArray<RGBQUAD, RGBQUAD> m_rgbTableRel;
	CArray<RGBQUAD, RGBQUAD> m_rgbTableDif;
	CArray<RGBQUAD, RGBQUAD> m_rgbTableBW;

protected:
	BOOL	Contour();
	GridInfo ReadSrf6GridHeader(CString strFilePath);
	int		ComputeHistogram(void);
	//double	GetHistogramValue(double val);

	// histogram
	BOOL		m_bCustomRange;			// use custom data range
	BOOL		m_bHistClr;				// histogram equalization on/off
	//DoubleArray m_hist;					// histogram
	BOOL		m_bModfHistogram;		// if true histogram needs to be recalculated
	double		m_dMinHst, m_dMinHstCst;// minimum for histogram color maping
	double		m_dMaxHst, m_dMaxHstCst;// minimum for histogram color maping

private:
	BOOL AllocateGrid(BOOL bAllocate = TRUE);

public:
	void	SetCustomRange(double dMin, double dMax)		{ m_dMinHstCst=dMin; m_dMaxHstCst=dMax; };
	void	SetCustomRange(BOOL bActive=TRUE)				{ m_bCustomRange = bActive; };
	BOOL	CreateContourLevels(void);
	BOOL	DrawContours(CDC* pDC, double scx, double scy, double offx, double offy);
	void	ShowContours(BOOL show = true);
	BOOL	IsVisibleContour(void);
	int		CheckDummies(double min, double max, BOOL bReplace=FALSE);
	int		CheckDummies();
	int		FillDummies( double dVal = 0.0, double dTrashMin = -1.7e-308 , double dTrashMax = 1.7e-308 );
	double	GetValue(int row, int col);
	int		CreateImage(CImage &img, int nWidth, int nHeiht, int nTag = CTBL_FLD);
	int		CreateImageLegeng(CImage &img, int nWidth=10, int nHeiht=200, int nTag = 0);
	//BOOL	GetGL(double* ptsArray, double* ptsNormal, float* clrOut, float* clrIn, int nRec, int nRowStart=0, int nRowEnd=0, int nColStart=0, int nColEnd=0);
	BOOL	GetGL(DoubleArray* pData, int nFormat, double min=1.0, double max=1.0);
	BOOL	GetGlRel(DoubleArray* pData, int nFormat, double min=1.0, double max=1.0, CGrid* pGrd=NULL);
	//void	GetColorf(int i, int j, float &r, float &g, float &b, int *h, int nHst, double dv, double th);
	void	SetValue(int i, int j, double val);
	BOOL	IsPlanar(void);
	BOOL	IsSameSize(int rows, int cols, double x0, double y0, double xSize, double ySize);
	BOOL	IsSameSize(GridInfo gi);
	BOOL	IsSameSize(CGrid* pGrd);
	// creates color table of n entries
	int		CreateColorTableGL(FloatArray& fA, int n, int nFormat, int nClrTable=CTBL_FLD, float fAlpha=0.8);
	void	GetHistogramMinMax(double& min, double& max);
	int		ReadType(CString strFilePath);
	void	GetLegendRange(double& min, double& max);
	BOOL	GetRowColMin(int& row, int& col);
	BOOL	GetRowColMax(int& row, int& col);
	void	InitColorTables(void);
	void	InitColorTable();
	void	CopyData(double** pCopyDest);
	void	CopyData(CGrid* pGrdDest);
	double	GetMax()								{return m_dMaxZ;};
	double	GetMin()								{return m_dMinZ;};
	void	SetValue(double val);
	int		SetValueEx(double val, int tag);
	void	SetFilePath(CString strFilePath)		{m_strFilePath = strFilePath;}
	void	SetGridType(int nType)					{m_nType = nType;}
	void	DummyData();
	int		DummyDataEx(CGrid* pGrdCompare, int tag);
	void	ZeroData();
private:
	inline void InitVariables(void);
public:
	BOOL Blank(double* plg, int nLenPlg, int nFormat, int nBlank=bln_inside);
	int Blank(void);
	int Properties(void);
};

