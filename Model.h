#pragma once

//------ includes ----------------------
#include <afxtempl.h>

#include "ClmnPt.h"
#include "Body.h"
#include "Grid.h"
#include "Point3D.h"
#include "Facet3Pt.h"
#include "Arrays.h"

#define	IDDUMMY = INT_MIN

//----- structs -------------------------
typedef struct FctGener {
	int n;				// maximum index for given id == bodys position in the ascending order
	int id;				// body ID
	int idPrev[4];		// previous body ID
	int idNext[4];		// next body ID
	int l;				// number of valid points in column
	BOOL b[4];			// which point valid
	CPoint3D top[5];	// top point
	CPoint3D bot[5];	// bottom point
} FCTGENER;

typedef	CArray<FCTGENER, FCTGENER&>	FctGenArray;

typedef struct BodyLineBound {
	int			n;		// index of body
	CPoint3D	pt;		// point
} BDLIN, *PBDLIN;

typedef CArray<BDLIN, BDLIN&> BDLINArray;

typedef CArray<ClmnPtPtrArray, ClmnPtPtrArray&> ModelData;

typedef CArray<FacetList, FacetList&> ModelFacets;

typedef CArray<BDLINArray, BDLINArray&> BodyBoundary;


//----- enums ---------------------------
typedef enum BodyMoveFull {
	BMF_NULL,					// NULL
	BMF_TOP,					// move top
	BMF_BOT						// move bottom	
};

typedef enum BodyMoveEnum {
	BMT_NORMAL = MK_LBUTTON,
	BMT_SPLIT = (MK_LBUTTON | MK_CONTROL),
	BMT_CONSTRAINED								// only within upper and lower boundary, for inversion
};

typedef enum BodyCheck {
	BCH_PREV = 0,
	BCH_NEXT = 1,
	BCH_NONE = 2
};

typedef enum BodyCreate{
	BCR_NONE = 0,
	BCR_JOINTOP = 1,
	BCR_JOINBOT = 2,
	BCR_JOINTOPBOT = 3,
};

typedef enum BodyImport{
	BDI_GRID = 0,
	BDI_CONST = 1,
	BDI_COPY = 2,
	BDI_JOIN = 3,
};

typedef enum ProfType {
	PRF_ROW,
	PRF_COL,
};

typedef enum FgType {
	FG_MID = 0,
	FG_TOP = 1,
	FG_BOT = 2,
	FG_TOPMOST = 3,
	FG_BOTMOST = 4,
};

//struct vrtxNMHDR
//{
//	NMHDR nmhdr;
//	int row;
//	int col;
//	int index;
//};


// CModel command target

class CModel : public CObject
{
// serialization
DECLARE_SERIAL( CModel );

// construction.....
public:
	CModel();
	virtual ~CModel();
	virtual void Serialize(CArchive& ar);


// data
protected:
	BOOL	m_bInitialized;			// is initialized???

	int		m_nID;					// ID
	CString m_strInfo;				// some kind if info

	// model data
	ModelData		m_data;			// model data
	BodyPtrArray	m_bodies;		// bodies
	int				m_nBodyID;		// body ID !!!
	ModelFacets		m_fct;			// model facet array of facet lists for given column

	// !!! update facet list
	// this list stores all facets to update field
	BOOL		m_bComputeRealTime;
	FacetList	m_fctLstUpdate; 

	// model observation area
	int		m_nRows;				// number of rows, first and last rows are extended
	int		m_nCols;				// number of columns, first and last columns are extended
	int		m_nRow;					// current row
	int		m_nCol;					// current column
	double	m_x0, m_xSize;			// "grid" origin, cell size x
	double	m_y0, m_ySize;			// "grid" origin, cell size y
	double	m_xMin, m_xMax;			// range x
	double	m_yMin, m_yMax;			// range y
	double	m_zMin, m_zMax;			// range z

	// model extension to the sides
	BOOL	m_bExtend;				// model extension on/off
	double	m_dExN;					// extend distance North
	double	m_dExS;					// extend distance South
	double	m_dExE;					// extend distance East
	double	m_dExW;					// extend distance West

	// body creation
	int		m_nCreationTag;			// BodyCreate Enum
	BOOL	m_bConstTop;			// constant top boundary
	double	m_dConstTop;
	BOOL	m_bConstBot;			// constant bottom boundary
	double	m_dConstBot;
	double	m_dCreationRatio;		// body cereation ratio

	// body import
	int		m_nImportUB;			// body upper boundary import tag
	int		m_nImportLB;			// body lower boundary import tag
	double	m_dImportUB;			// body upper boundary constant value
	double	m_dImportLB;			// body lower boundary constant value

	// body contours
	BodyBoundary	m_bodyBound;
	BDLINArray*		m_pBodyBoundCol;	// current column for body boundary


// functions
public:
	BOOL	IsInitialized()		{ return m_bInitialized;}

	void	Properties(void);
	void	CreationProperties(void);
	ClmnPtPtrArray*	GetAt(int row, int col);
	CClmnPt*	GetAt(int row, int col, int nIndex);
	void	Add( int row, int col, CClmnPt* pMpt );
	void	InsertAt( int row, int col, int nIndex, CClmnPt* pMpt  );
	void	RemoveAt( int row, int col, int nIndex, int nCount = 1 );
	int		GetCount(int row, int col);
	int		GetUpperBound(int row, int col);
	int		IsEmpty(int row, int col);

	// initialization
	BOOL	Init( int nRows, int nCols, double x0, double y0, double xSize, double ySize, double zMin, double zMax);
	BOOL	Init( CGrid* pGrd, double zMin, double zMax);
	BOOL	IsEmpty()    { return m_data.IsEmpty();}

	// body operations
	BodyPtrArray* GetBodies() {return &m_bodies;}
	CBody*	NewBody(void);
	int		InsertBody(int row, int col, double z, double t=-1.0, BOOL bNew = TRUE, int nBodyID = -1, double zT=0.0, double zB=0.0, BOOL bZ=TRUE);
	int		RemoveBody(int nIndex, int row, int col);
	int		RemoveBodyFromProfile(int nID, int nFrom, int nTag);
	int		DeleteBody(int nID);
	int		SelectBody(void);
	int		CopyBody(int nID, int nFrom, int nTo, int nTag);
	BOOL	IsInsPosOK(int nIndex, int nID, int row, int col);
	BOOL	IsInsPosOK_1(int nTag, int nID, int nIdNext, int row, int col);
	BOOL	IsInsPosOK_2(int nTag, int nID, int nIdNext, int row, int col);
	CBody*	GetBody(int nIndex, int row, int col);
	inline	CBody*	GetBody(int nID);
	inline	int		GetBodyIndex(int nID);
	inline	int		GetBodyTopBot(int nID, int row, int col, double &t, double &b);
	inline	int		GetID(int nIndex, int row, int col);
	inline	int		GetBodyIndex(int nID, int row, int col);
	inline	void	UpdateBodyIndex(void);


	//	points move
	int		MoveVertex(int &nIndex, int row, int col, double z, int nTag = BMT_NORMAL);
	void	SetClmnPt( CClmnPt* cPt, int row, int col, double z);						// set CPoint3D

	// coordinates
	inline double	GetZ(int row, int col, int nIndex);
	inline double	GetZmid(int row, int col, int nIndex);
	inline void		SetZ(int row, int col, int nIndex, double z);
	inline double	GetXd(int col)					{ return m_x0 + (col-1)*m_xSize;}
	inline double	GetXe(int col);
	inline double	GetYd(int row)					{ return m_y0 + (row-1)*m_ySize;}
	inline double	GetYe(int row);
	inline double	GetX2d(int col)					{ return m_x0 + (col-1)*m_xSize + m_xSize/2;}
	inline double	GetX2e(int col);
	inline double	GetY2d(int row)					{ return m_y0 + (row-1)*m_ySize + m_ySize/2;}
	inline double	GetY2e(int row);
	void			GetXZd(int nIndex, int row, int col, double& x, double& z);
	void			GetYZd(int nIndex, int row, int col, double& x, double& z);
	double			GetTopBotLimits(int nIndex, int row, int col, double &zU, double &zD);
	void			UpdateMoveIndex(int &nIndex, int nIndexOld, int row, int col, int bodyID);
	inline int		GetRows()	{return m_nRows;}		// get total number of rows (with extension)
	inline int		GetCols()	{return m_nCols;}		// gt total number of columns (with extension)
	inline int		GetRow()	{return m_nRow;}		// get current row
	inline int		GetCol()	{return m_nCol;}		// get current column
	inline double	GetRelief(int row, int col)		{ return GetZ(row, col, 0);	}
	inline double	GetHell()						{ return m_zMin;			}
	inline double	GetHeaven()						{ return m_zMax;			}
	inline double	GetSizeX()						{ return m_xSize;			}
	inline double	GetSizeY()						{ return m_ySize;			}
	inline double	GetX0()							{ return m_x0;				}
	inline double	GetY0()							{ return m_y0;				}
	inline double	GetZ0()							{ return m_zMin;			}
	inline double	GetZMax()						{ return m_zMax;			}
	inline double	GetLXd()						{ return (m_nCols-1)*m_xSize;	}	// length in x direction for display
	inline double	GetLYd()						{ return (m_nRows-1)*m_ySize;	}	// length in y direction for display
	inline double	GetLX()							{ return (m_nCols-3)*m_xSize;	}	// length in x direction without extension
	inline double	GetLY()							{ return (m_nRows-3)*m_ySize;	}	// length in y direction without extension
	inline double	GetLZ()							{ return m_zMax-m_zMin;		}		// length in z direction
	void	GetVertexPt(int row, int col, int nIndex, CPoint3D& pt);			// fills pt with coordinates of vertex at given position
	double	GetThicknes(int row, int col, int nIndex);
	double	GetThicknesNew(int row, int col, int nIndex);
	double	GetThicknes()							{ return (m_zMax-m_zMin)/10;}		// initial body thickness

// facet generation
public:
	FacetList*	GetFacetList(int row, int col);							// returns facet list for given column
	void		GetFacetListEx(int row, int col, FacetList* fctLst);	// fills fctLst with facets  which contain given facet column
	void		UpdateFacetColumn(int row, int col);
	void		UpdateFacetList(int row, int col);
	void		SetComputeRealTime(BOOL bRealTime=TRUE)	{ m_bComputeRealTime = bRealTime; }
	// updates GL facets araay
	void		UpdateGLFacets(int row, int col);
	void		UpdateGLFacets(void);
	// updates GL selection points araay
	void		UpdateGLSelPt(int row, int col);

	// returns index in 1D array, if failed returns -1
	inline int		GetIndex(int row, int col);
	//int		GetFacets(FacetList& fctLst);
	int			GetFacetsComputation(FacetList& fctLst);
	FacetList*	GetFacetsUpdate()		{return &m_fctLstUpdate;}

	// facets GL
	// one facet is stored as follows
	// {x1,y1,z1, x2,y2,z2, x3,y3,z3, nx,ny,nz, co_r,co_g,co_b,co_a, ci_r,ci_g,ci_b,ci_a }
	// where x,y,z are point coordinates
	//		nx,ny,nz facet normal
	//		c[i,o]_[r,g,b,a] color inside or outside, respectively, with RGBA components
	DDArray		m_glFacets;

	//  GL selection points
	// one selection point is stored as follows
	// {x,y,z, bodyID,row,col,index,indexZ,indexGL, r,g,b,a }
	// where x,y,z:					point coordinates
	//		bodyID,row,col,index, i:	integerbodyID, ..., index: in 'm_data' array, indexZ:	"depth" index
	//									indexGL index in 'm_glSelPt' array
	//		r,g,b,a:				color of upper body point 
	//		13 entries per point => totalSize = 13*numberOfPoints
	DDArray		m_glSelPt;

	DDArray*	GetGLFacets();
	BDLINArray*	GetBodyBoundAt(int row, int col);
	DDArray*	GetGLSelPt();

private:
	void	InitFacetList(void);
	int		GenerateFacets(FacetList& fclLst, int row, int col);
	int		AddFctGen(FctGenArray& fga, int row, int col, int nTag);
	void	FGtoFCT(FctGenArray* fga, FacetList* pFctLst, int i0, int i1);
	void	FGtoFCTbound(FCTGENER *pFg, FacetList* pFctLst);
	void	InitFacet(FCTGENER *pU, FCTGENER *pL, FacetList* pFctLst, int i0, int i1);
	void	InitFacetTBM(FCTGENER *pFg, FacetList* pFctLst, int i0, int i1, int nTag);	// top-, bottom-most facets
	void	InitSideFacets(FCTGENER *pFg, FacetList* pFctLst);
	void	InitSideFacets_2(FCTGENER *pFg, FacetList* pFctLst, int i0, int i1);
	void	InitSideFacets_3(FCTGENER *pFg, FacetList* pFctLst, int i0, int i1);
	void	InitSideFacetsBound(FCTGENER *pFg, FacetList *pFctLst, int i0, int i1);
	void	AddBodyBoundAt(int row, int col, BDLIN pt );

public:
	void	ComputeBodyMagnetizationVector(CPoint3D indFld);
	int		IsBodyArrayEmpty(void);
	void	SetVerticalRange(double min, double max);
	// body properties
	int		PropertiesBody(int nID);
	// finds constrains for constrained vertical vertex motion 
	void	GetConstrainZ(int row, int col, int index, double& zU, double& zD);
	// imports body from grid file
	int		ImportBody(void);
	int		ExportBody(int nID=-1);

	// move whole body
	void	CheckIndex(int &nIndex, int row, int col);
	int		MoveBody(int nID, double dShiftTop, double dShiftBot, int nTag, BOOL bAbsolute=FALSE);
};
