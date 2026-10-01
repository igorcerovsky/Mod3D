// Mod3DDoc.h : interface of the CMod3DDoc class
//
/////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Grid.h"
#include "Point3D.h"
#include "PotField.h"
#include "Body.h"
#include "Messages.h"
#include <comdef.h>
#include <afxstr.h>
#include "bmpreferenced.h"
#include "MyObject.h"
#include "CheckPoint.h"
#include "Model.h"
#include "Well.h"
#include "Data3D.h"
#include "VersionType.h"


#define MOD_COMPONENTS = 14;

typedef enum FitMethod
{
	FIT_GOLDEN = 0,
	FIT_BRENT = 1,					
};

typedef enum ModificationTag
{
	MODF_MODEL = 1,					// model modification is enabled
};

typedef enum UpdateTag
{
	UPDATE_DEFAULT = 0,
	UPDATE_NEW_MODEL = 1,
	UPDATE_MOVE_VERTEX = 2,
	UPDATE_MOVE_EDGE = 3,
	UPDATE_COMPUTATION_FINISHED = 4,
	UPDATE_PROPERTIES_REDRAW = 5,
	UPDATE_PROPERTIES_RECOMPUTE = 6,
	UPDATE_MOVE_PROFILE = 7,
	UPDATE_OBJECT_MANAGER_SHOW = 8,
	UPDATE_PROPERTIES_BODY_REDRAW_2D = 9,				// drawing properties changed
	UPDATE_PROPERTIES_BODY_REDRAW = 10,					// drawing properties changed
	UPDATE_PROPERTIES_BODY_RECOMPTUTE = 11,				// physical properties of body changed
	UPDATE_BODY_EDIT_DELETE = 12,						// body deleted from list
	UPDATE_PROPERTIES_GRID_COLOR = 13,
	UPDATE_COPY_PROFILE = 14,
	UPDATE_INSERT_BODY = 15,
	UPDATE_DELETE_BODY = 16,
	UPDATE_REMOVE_BODY = 17,
	UPDATE_REMOVE_BODY_FROM_PROF = 18,
	UPDATE_BITMAP_DELETE = 19,							// object is deleted from array
	UPDATE_BITMAP_NEW = 20,								// new bitmap
	UPDATE_BITMAP_CAHNGED = 21,							// change in bitmap properties
	UPDATE_LEADHRZ_NEW = 22,							// new leading horizon
	UPDATE_LEADHRZ_DELETE = 23,							// deleted leading horizon
	UPDATE_LEADHRZ_CAHNGED = 24,						// change in leading horizon
	UPDATE_DATA3D_NEW = 25,								// new 3D DATA horizon
	UPDATE_DATA3D_DELETE = 26,							// deleted 3D DATA horizon
	UPDATE_DATA3D_CHANGED = 27,							// change in 3D DATA horizon
};

typedef enum ProfileTypeA{
	PRF_HRZ=0,		// vertical profile
	PRF_VRT=1,		// horizontal profile
	PRF_ARB=2,		// arbitrary oriented profile
};

typedef enum ReferenceModelType{
	RDM_NORMAL = 0,	// reference model with linearly variable density
	RDM_LINEAR = 1,	// reference model with linearly variable density
};

typedef enum FacetDivisionType{
	FCTDIV_NORMAL = 1,	
	FCTDIV_OPOSIT = 10,	
};

// type of fittting caracteristic
typedef enum FitType{
	FIT_RMS = 0,		// use RMS of the residual field
	FIT_DRV = 1,		// use deriative == smootness of the residual field
	FIT_DRV_LOCAL = 2,	// use deriative in local point== smootness of the residual field
	FIT_DRV_SECOND = 3,	// use second deriative in local point== smootness of the residual field
};

// observation header info
typedef struct  ObsHdrTag{
	CString	strName;
	int		nCols;		// number of columns
	int		nRows;		// number of rows 
	double	x0;
	double	y0;
	double	xSize;
	double	ySize;
	double	dMinX;
	double	dMaxX;
	double	dMinY;
	double	dMaxY;
	double	dMinZ;
	double	dMaxZ;
	//COMPROWCOL cmpRC;	// computed rows and collumns
} OBSHDR;


typedef struct  CmpFldTag{
	LPVOID			pParam;
	CDocument		*pDoc;
	OBSHDR			*pObsHdr;	// header information
	BodyList		*pLst;		// pointer to list of bodies
	CPoint3D		*p_vIndFld;	// inducing field
	double			**pObs;		// observation
	double			dUnits;
	// ouput field
	double			**pGx;		
	double			**pGy;		
	double			**pGz;		
	double			**pMx;		
	double			**pMy;		
	double			**pMz;		
	double			**pG;		
	double			**pM;	
	
	BOOL	bExtend;
	double  dExtend;
	BOOL	bMag;
	BOOL	bGrv;
} CMPFLDSTRUCT;

typedef struct BodyVrtxMoveTag{
	// vertex moving
	BOOL	bConstTop;	// leave top constant = dTop
	double  dTop;		
	BOOL	bConstBot;	// leave bottom const = dBot
	double	dBot;
	int		nFlag;
	// body moving
	BOOL	bMoveTop;
	BOOL	bMoveBot;
	BOOL	bFixRel;
	BOOL	bFixBot;
	BOOL	bCurrentProfile;
} BODYVRTXMOVE;

typedef struct imggrf {
	int		nType;							// map, profile
	BOOL	bVisible;						
	CImage	img;
	double	top, left, hight, width;		// map coordinates coordinates
	int		nTop, nLeft, nHight, nWidth;	// screen logical coordinates
	int		nProfOrient;					// vertival, horizontal, other
	int		nProf;							// profile number
	CString strName;						// name of the profile
	HTREEITEM hTreeItem;
} IMGGRF;

typedef struct GravTensor {
	BOOL	bCompute;
	int		nTag;
	double	dFlightElev;	// flight elevation
	double	dHeight;		// height over relief
	double  dUnits;			// multiply units by this number
 } GRVTENS;

// settings for profile view
typedef struct ProfViewSettings {
	char	strSettingsName[250];
	int		nPrfType;
	int		nFldExtrem;
	int		nRow;
	int		nCol;
	BOOL	bProfPrev;		// show previous profile
	BOOL	bProfNext;		// show next profile
	BOOL	bScaleEq;		// equal scale for x & y axes
	BOOL	bf[60];			// show field 
	int		nFldTbl;		// field toolbar type
	BOOL	bAxis;			// axis toolbar active
} PRFVIEWSET;

typedef CArray<PRFVIEWSET, PRFVIEWSET&> PrfSetArray;

// settings for 3D View
typedef struct View3DSettings {
	char	strSettingsName[250];
	double 	minXl, minYl, minZl, stepXl, stepYl, stepZl;
	UINT	decXl, decYl, decZl;
	int		nTextType;

	// grid in Open GL coordinates
	BOOL	bGrdLineUpX, bGrdLineUpZ;
	BOOL	bGrdLineDwnX, bGrdLineDwnZ;
	BOOL	bGrdLineVrt;

	// Model rotation - in degrees !
	float fRotX , fRotY , fRotZ;

	// Constants for differntial motion and rotation
	float mX, mY, mZ;
	float rX, rY, rZ;

	// Camera position
	double dCameraX , dCameraY , dCameraZ;
	double dCameraEyeZ;
	double dProjAngle;								// perspective projection angle

	// lights
	BOOL	bLightsEnabled;

	BOOL	bTransparentBodies;
	float	fAlphaBody;

	BOOL	bProfilePlanes;
	BOOL	bProfilePlanesSolid;
	float	fAlphaProf;

	BOOL	bDrawBitmaps;
	float	fAlphaBmp;

	BOOL	bRelief;
	BOOL	bRelSolid;
	BOOL	bField;
	BOOL	bFldSolid;
	float	fAlphaRel;
	float	fAlphaSrf;

	// darken inside facets
	BOOL	bDarkenInside;
	float	fDarkenInside;

	// viewed part of model
	int		nRowStart, nRowEnd;
	int		nColStart, nColEnd;

	// show properties
	BOOL	bOutFct;		// show model sides
	BOOL	bVrtFct;		// show horizontal facets
	BOOL	bCullFacet;	// culls the back or front facets
	int		nCullFacet;	// cull facet mode eithre GL_FRONT, GL_BACK
	BOOL	bBodySolid;

	float	fAlphaFld;

	// leading horizons
	BOOL	bHorizon;
	BOOL	bHorizonSolid;
	float	fHorizonAlpha;

	int		nFldTbl;			// field toolbar type
	int		nFld;				// displayed field

	int		nMode;				// GL mode can be GL_RENDER || GL_SELECT
	float	fSelSensitivity;		// selection sensitivity in points

} VIEW3DSET;

typedef CArray<VIEW3DSET, VIEW3DSET&> View3dSet;


////////////////////////////////////////////////////////////
//enums

typedef enum Mod3DGrids {
	GRDMOD_GX	= 0,
	GRDMOD_GY	= 1,
	GRDMOD_GZ	= 2,
	GRDMOD_G	= 3,
	GRDMOD_GXX	= 4,
	GRDMOD_GYY	= 5,
	GRDMOD_GZZ	= 6,
	GRDMOD_GXY	= 7,
	GRDMOD_GXZ	= 8,
	GRDMOD_GYZ	= 9,
	GRDMOD_MX	= 10,
	GRDMOD_MY	= 11,
	GRDMOD_MZ	= 12,
	GRDMOD_M	= 13,
	GRDMOD_MXX	= 14,
	GRDMOD_MYY	= 15,
	GRDMOD_MZZ	= 16,
	GRDMOD_MXY	= 17,
	GRDMOD_MXZ	= 18,
	GRDMOD_MYZ	= 19,

	GRDDIF_GX	= 20,
	GRDDIF_GY	= 21,
	GRDDIF_GZ	= 22,
	GRDDIF_G	= 23,
	GRDDIF_GXX	= 24,
	GRDDIF_GYY	= 25,
	GRDDIF_GZZ	= 26,
	GRDDIF_GXY	= 27,
	GRDDIF_GXZ	= 28,
	GRDDIF_GYZ	= 29,
	GRDDIF_MX	= 30,
	GRDDIF_MY	= 31,
	GRDDIF_MZ	= 32,
	GRDDIF_M	= 33,
	GRDDIF_MXX	= 34,
	GRDDIF_MYY	= 35,
	GRDDIF_MZZ	= 36,
	GRDDIF_MXY	= 37,
	GRDDIF_MXZ	= 38,
	GRDDIF_MYZ	= 39,

	GRDMES_GX	= 40,
	GRDMES_GY	= 41,
	GRDMES_GZ	= 42,
	GRDMES_G	= 43,
	GRDMES_GXX	= 44,
	GRDMES_GYY	= 45,
	GRDMES_GZZ	= 46,
	GRDMES_GXY	= 47,
	GRDMES_GXZ	= 48,
	GRDMES_GYZ	= 49,
	GRDMES_MX	= 50,
	GRDMES_MY	= 51,
	GRDMES_MZ	= 52,
	GRDMES_M	= 53,
	GRDMES_MXX	= 54,
	GRDMES_MYY	= 55,
	GRDMES_MZZ	= 56,
	GRDMES_MXY	= 57,
	GRDMES_MXZ	= 58,
	GRDMES_MYZ	= 59,

};

typedef enum ObservationEnum {
	OBS_SENS_HEIGHT = 0,
	OBS_FLIGHT_ELEVATON = 1,
	OBS_GRID = 2
};

typedef enum ComputationType
{
	CMP_NONE = 0,
	CMP_REALTIME = 1,
};

typedef enum FldToolbar{
	FLDTLB_MODELED	= 0,
	FLDTLB_DIFFERENCE = 2,
	FLDTLB_MEASURED = 1,
};

typedef CTypedPtrList< CPtrList, IMGGRF* >	ImageList;
typedef CArray<double, double>		DoubleArray;
typedef CArray<float, float>		FloatArray;
typedef CArray<int, int>			IntArray;



UINT ComputeFldThread(LPVOID pParam);
//BOOL ComputeGravityVlado(const CPoint3D &v_r, CFacet3Pt &facet, CPoint3D &fGrv, double dRefDensity, int nTag, CPoint3D vRefDensGrad, double dRefDensOrg);



class CVrtxMove : public CObject
{
public:
	int		nBodyID;
	int		nRow;
	int		nCol;
	int		nIndex;
	double	dNewZ;
};

class CEdgeMove : public CObject
{
public:
	int		nBodyID;
	int		nRow;
	int		nCol;
	int		nIndexTop;
	int		nIndexBot;
	int		nTag;		//0:destry; 1:create
};


class CMod3DDoc : public CDocument
{
protected: // create from serialization only
	CMod3DDoc();
	virtual ~CMod3DDoc();
	DECLARE_DYNCREATE(CMod3DDoc)

// Attributes
public:

// Operations
public:
	CModel*	GetModel();

// Overrides
public:
	virtual void Serialize(CArchive& ar);
	void	SerializeLoad(CArchive& ar, int nVersion);
	void	Serialize_20031113(CArchive& ar);
	void	Serialize_20031105(CArchive& ar);
	void	Serialize_20031029(CArchive& ar);
	void	Serialize_20031021(CArchive& ar);
	void	Serialize_20030921(CArchive& ar);
	void	SerializeOld(CArchive& ar);

	virtual BOOL OnNewDocument();
	virtual void OnCloseDocument();
	virtual BOOL OnOpenDocument(LPCTSTR lpszPathName);

// Implementation
protected:
	int			m_nVersion;			// document version
	CModel		m_model;			// 3D model definition class

	int			m_nID;				// ID for objects
	CString		m_strName;			// observation name
	OBSHDR		m_obsHdr;			// observation header

	// observation grids
	CGrid		m_ObsRel;			// observations relief
	CGrid		m_ObsGrv;			// gravity observations
	CGrid		m_ObsMag;			// magnetics observations
	CGrid		m_ObsTns;			// gravity tensor observations

	// computations
	int			m_nComputationType;	// computation type see enum ComputationType
	BOOL		m_bSherComp;		// spherical computation active

	// gravity field
	int			m_nGrvFormula;		// formula for gravity computing
	int			m_nGrvObsTag;		// defines observation grid type
	double		m_dGrvDensRef;		// reference density
	double		m_dGrvSens;			// gravity sensor height
	double		m_dGrvElev;			// constant sensor elevation
	double		m_dGrvUnits;		// units constant
	CPoint3D	m_vGrvDensGrad;		// variable density gradient
	CPoint3D	m_vGrvDensOrigo;	// origo for variable density gradient
	GravTensor	m_gvrTens;			// information about grav tensor computation
	BOOL		m_bRemoveMeanGrv;	// remove difference field mean from modeled field
	BOOL		m_bRemoveMeanTns;	// remove difference field mean from modeled field

	//magnetic field
	int			m_nMagFormula;		// formula for magnetics computing
	int			m_nMagObsTag;		// defines observation grid type
	double		m_dMagUnits;		// units constant
	double		m_dMagSens;			// sensor high over relief
	double		m_dMagElev;			// constantsensor elevaion
	CPoint3D	m_vMagIndFld;		// inducing field .x= inclination; .y=declimation; .z= intensity
	CPoint3D	m_vMagH;			// ambient magnetic field
	BOOL		m_bRemoveMeanMag;	// remove difference field mean from modeled field

	// field grids
	CGrid		m_grd[60];			// all grids see Mod3DGrids Enum
	BOOL		m_grdActiveCompute[60];	// tells if grid is active for computation && updating !!!
	int			m_nGrids;			// number of grids
	int			m_nIndicatorFld;	// field for indicator
	
	// see Mod3DGrids Enum
	int			m_nAllComponents;	// mumber of all grids
	int			m_nModComponents;	// end_index+1 of modeled components
	int			m_nDifComponents;	// end_index+1 of difference components

	// current row and column
	int			m_nRowCur;			// current row
	int			m_nColCur;			// current column
	double		m_x, m_y, m_z;		// current coordinates
	
	// compute thread
	CWinThread	*m_pCompThread;

	// image
	MyObPtrArray	m_objPtrArray;

	// settings for views
	PrfSetArray m_viewPrfSet;
	View3dSet	m_view3dSet;

	// wells
	//WellPtrArray	m_wells;

public:
	PrfSetArray*	GetProfViewSettingsArray()				{ return &m_viewPrfSet;	}
	View3dSet*		GetView3dSettingsArray()				{ return &m_view3dSet;	}

	// computation
	int			ComputeField(FacetList* pFctLst, BOOL bZeroVal=TRUE);

	//COMPROWCOL	GetCompRC()								{ return m_obsHdr.cmpRC;	}
	void		SetSpherComp(BOOL bSphComp = FALSE)		{ m_bSherComp = bSphComp;	}
	BOOL		GetSpherComp()							{ return m_bSherComp;		}
	CBody*		IsPtInBody(int i, int j, double val);
	BOOL		VerifyInsertPosition(int i, int j, double val);
	void		EditBodies(CBody *pSelBody=NULL);
	void		ChangeVerticalRange();
	void		ComputeTotalField();
	CPoint3D	GetAmbientField()		{return m_vMagH;}
	CPoint3D	GetInducedField()		{return m_vMagIndFld;}
	void		GetObsMinMax(double & min, double &max);
	int			GetGrvFormula()						{ return m_nGrvFormula;	}
	int			GetMagFormula()						{ return m_nMagFormula;	}
	void		GetCurrentPos(int &row, int &col);
	void		SetCurrentPos(int row, int col);
	OBSHDR		GetObservationHeader()				{ return m_obsHdr;		}
	OBSHDR		GetObservationHeaderGL();
	double		GetGravTUnits()						{ return m_gvrTens.dUnits;	}
	double		GetGravUnits()						{ return m_dGrvUnits;	}
	double		GetMagUnits()						{ return m_dMagUnits;	}
	double		GetMagSensHeight()					{ return m_dMagSens;	}
	double		GetGrvSensHeight()					{ return m_dGrvSens;	}
	CGrid*		GetObservationGrid()				{ return &m_ObsRel;		}
	CGrid*		GetObservGrv()						{ return &m_ObsGrv;		}
	CGrid*		GetObservMag()						{ return &m_ObsMag;		}
	CGrid*		GetObservTns()						{ return &m_ObsTns;		}
	CGrid*		GetReliefGrid()						{ return &m_ObsRel;		}
	CPoint3D	GetDensityGradient()				{ return m_vGrvDensGrad;}
	double		GetRefDensAtOrigo()					{ return (m_dGrvDensRef - m_vGrvDensGrad*m_vGrvDensOrigo);}
	double		GetReferenceDensity()				{ return m_dGrvDensRef;	}
	GRVTENS		GetGradStruct()						{ return m_gvrTens;	}
	int			GetComputationType()				{ return m_nComputationType;		}
	void		ComputeRelativePaths();
	int			GetComponentsNumber()				{ return m_nAllComponents;}
	int			GetComponentsMod()					{ return m_nModComponents;}
	int			GetComponentsDif()					{ return m_nDifComponents;}
	int			ExportFacetLst(FacetList& fctLst);


	double**	GetObservationData();
	BOOL		IsEmpty();
	OBSHDR		GetObservInfo();
	MyObPtrArray* GetObjArray()						{ return &m_objPtrArray;}
	//WellPtrArray* GetWells()						{ return &m_wells;}

#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

	
// Generated message map functions
protected:
	afx_msg void OnComputeCompute();
	afx_msg void OnModelDefineObservation();
	afx_msg void OnUpdateModelDefineObservation(CCmdUI* pCmdUI);
	afx_msg void OnModelImportObservation();
	afx_msg void OnUpdateModelImportObservation(CCmdUI* pCmdUI);
	afx_msg void OnModelInducingField();
	afx_msg void OnModelChangeVerticalRange();
	afx_msg void OnBodyEdit();
	afx_msg void OnModelImportMeasuredField();
	afx_msg void OnComputeProperties();
	afx_msg void OnViewEditPicture();
	afx_msg void OnComputeActiveField();
	afx_msg void OnViewSetfieldindicator();
	afx_msg void OnUpdateViewSetfieldindicator(CCmdUI *pCmdUI);
	DECLARE_MESSAGE_MAP()

protected: // functions
	BOOL	MakeObservations();
	void	ComputeAmbientField();
	void	UpdateMinMaxFldVal();
	void	UpdateMinMaxFldValModified();
	CString CreateID(CString str);

public:
	// Update document after thread finishes
	void	ComputationFinished(void);
	void	ComputationCanceled(void);

	CGuideMark* NewGuideMark(void);
	CWell*	NewWell(void);
	CData3D* NewData3D(void);
	int		InitializeProfileCheckMarks(int profType, double xy);
	void	Save() { OnFileSave(); }
	void	ZeroModelGrids(void);

	int		UpdateField(BOOL bUpdateViews=TRUE);
	//int		UpdateField(FacetList* pFctLst, BOOL bUpdateViews=FALSE);
	int		GenerateModelFacets(FacetList& fctLst, int nRowStart, int nRowEnd, int nColStart, int nColEnd, BOOL bOuterFacets = FALSE);
	void	SetModifiedFlagModFld(int bModified = TRUE);
	CString GetFieldString(int n);
	void	CreateCheckMark();	
	double** GetGridData(int nFld);
	double** GetGridDataComp(int nFld);
	CGrid*	GetGrid(int nTag);
	int		ComputeDifferenceField(void);
	int		RemoveMeanDif(void);
	int		SetIndicators(int row, int col, BOOL bElev=FALSE);

	CMyObject*		GetObject(int nIndex);
	afx_msg void	OnComputeRealTime();
	afx_msg void	OnUpdateComputeRealTime(CCmdUI *pCmdUI);
	afx_msg void	OnModelProperties();
	afx_msg void	OnImportBitmap();
	afx_msg void	OnUpdateImportBitmap(CCmdUI *pCmdUI);
	afx_msg void	OnEditGuidelines();
	afx_msg void	OnUpdateEditGuidelines(CCmdUI *pCmdUI);
	//afx_msg void	OnEditObjectManager();
	//afx_msg void	OnUpdateEditObjectManager(CCmdUI *pCmdUI);
	afx_msg void	OnImportBody();
	afx_msg void	OnUpdateImportBody(CCmdUI *pCmdUI);
	afx_msg void	OnUpdateImportGuideLine(CCmdUI *pCmdUI);
	afx_msg void	OnImportMeasuredField();
	afx_msg void	OnUpdateImportMeasuredField(CCmdUI *pCmdUI);
	afx_msg void	OnImportGuideLine();
	afx_msg void	OnExportGuideLine();
	afx_msg void	OnUpdateExportGuideLine(CCmdUI *pCmdUI);
	afx_msg void	OnImportGrid();
	afx_msg void	OnExportModeledField();
	afx_msg void	OnExportBody();
	afx_msg void	OnUpdateExportBody(CCmdUI *pCmdUI);
	void			ExportBody(int nID=-1);

	// returns names of grids which are not empty devided by semicolon ";"
	void	GetGridsNonEmptyString(CStringArray& strA);
	void	GetFldStringArray(CStringArray& strA, int nFrom=-1, int nTo=-1, BOOL bEmpty=FALSE, BOOL bPlanar=FALSE);
	CGrid*	GetGrid(CString str, int& nFld);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// --------  fitting funtion ------------------------------------------------------------------------------------------------
protected:
	int		m_nFitVrtxMeth;		
	int		m_nFitVrtxMaxIter;		// maximum iterations allowed
	int		m_nFitVrtxChar;			// fit characteristic one of the enum types
	int		m_nFitFld;				// field used for fitting evalution - MUST be difference field; m_nFldFit==-1 not initialized 
	double	m_dFitVrtxTol;
	double	m_dFitVrtxEps;
	double	m_dFitVrtxEpsAuto;		// eps = m_dFitVrtxEpsAuto*(m_dFitVrtxZU-m_dFitVrtxZD); percent of interval length
	double	m_dFitVrtxZU;
	double	m_dFitVrtxZD;
	BOOL	m_bFitVrtxLog;			// create fitting log file
	CString	m_strFitVrtxLog;		// log file path
	BOOL	m_bFitVrtxEpsAuto;		// automatic epsilon setting
	CString m_strFitLog;

public:
	CWinThread	*m_pThreadFit;
public:
	void	FitVertexProperties();
	int		FitVertex(int nIndex, int row, int col);
private:
	double** GetGridDataInv(int nFld, BOOL bActiveOnly=TRUE);
	double	FuncFit1D(double z, int nIndex, int nRow, int nCol);
	double	Brent(double a, double b, double c, double tol, double *xmin, int nIndex, int nRow, int nCol);
	double	Golden(double a, double b, double c, double tol, double *xmin, int nIndex, int nRow, int nCol);
	void	MNbrak(double *a, double *b, double *c, double *fa, double *fb, double *fc, int nIndex, int nRow, int nCol);
	int		FitFieldUpdate(int row, int col, double dSign);
	afx_msg void OnComputeFit1d();

//---- scalar inversion ----------------------------------------------------------------------------------------------------
protected:
	CGrid	m_grdInv[20];			// field for sclarar inversion - unit field for given body (density or susceptibility)
	CGrid	m_fitGrdFld;			// temporary field for given inversion component (i.e. gz) without field of inverted body
	CGrid	m_fitGrdDens;			// temporary grid used in density inversion
	int		m_nFitDensChar;			// characteristic used in density inversion either RMS or DRV
	double	m_dFitDensEps;			// epsilon in initial bracketing
	int		m_nFitDensMeth;			// method used for fitting either Brent or Golden section
	int		m_nFitDensMaxIter;		// maximum iterations allowed
	double	m_dFitDensTol;
	BOOL	m_bFitDensLog;			// create fitting log file
	CString	m_strFitDensLog;		// fitting log file
	double	FuncFitDens1D(double dens);
	double	Brent(double a, double b, double c, double tol, double *xmin);
	double	Golden(double a, double b, double c, double tol, double *xmin);
	void	MNbrak(double *a, double *b, double *c, double *fa, double *fb, double *fc);
	int		ComputeFieldInv(FacetList* pFctLst);	// computes field of facets in 'fctLst' and adds the field to m_grdInv[20
public:
	int		InvertDensity(int nBodyID);
	afx_msg void OnComputeDensityInversionProperties();
	void	DensityInversionProperties(void);
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

public:
	BOOL IsEnabled(int nTag);
	void BodyPropertiesChanged(int nBodyID, CPropertySheet* pSheet);
	void NewDifferenceField(int nMeasuredField);


	__forceinline BOOL CheckDemo()
	{
		// for DEMO allow only 64x64 grid
		#ifndef MOD3D_FULL
			if( m_obsHdr.nCols > 64 || m_obsHdr.nRows > 64)
			{
				AfxMessageBox("This is demo version only :-(\nMaximum 64x64 observation points.\nClosing.", MB_OK | MB_ICONSTOP);
				SetModifiedFlag( FALSE );
				AfxGetApp()->GetMainWnd()->PostMessage(WM_CLOSE);
				return FALSE;
			}
		#endif

		#ifdef MOD3D_ROMAN
			if( m_obsHdr.nCols > 128 || m_obsHdr.nRows > 128)
			{
				AfxMessageBox("This is demo version only :-(\nMaximum 128x128 observation points.\nClosing.", MB_OK | MB_ICONSTOP);
				SetModifiedFlag( FALSE );
				AfxGetApp()->GetMainWnd()->PostMessage(WM_CLOSE);
				return FALSE;
			}
		#endif

		return TRUE;
	}


	afx_msg void OnModelBlankGrid();
	afx_msg void OnModelObservationProperties();
	afx_msg void OnImportWell();
	afx_msg void OnEditFieldGrid();
	int AddObject(CMyObject* pObj);
	afx_msg void OnImport3ddata();
	afx_msg void OnComputeActivateGrid();
};
