#pragma once

#include "Model.h"
#include "Mod3DDoc.h"
#include "gl/gl.h"
#include "gl/glu.h"
#include "GlBitmap.h"
#include "glsurface.h"

#include "View3dSheet.h"


typedef enum CoordTransformation {
	TRC_FRW=1,			// forward transformation: x,y,z to device coordinates
	TRC_INV=-1,			// inverse transformation: device to world coordinates
	TRC_INVSP=2,		// inverse special transformation: device to world using world coordinates for depth value
};

typedef enum Projection3D{
	PRJ_ORTHO, 
	PRJ_PERSPECTIVE
};

typedef enum ModelSolidWireframe{
	SHD_WIREFRAME = 0, 
	SHD_FILLEDPOLY = 1
};

typedef enum TextTypeGL{
	GLTXT_NONE = 0,
	GLTXT_2D = 1,
	GLTXT_3D = 2,
};
// CViewGL3D view


class CViewGL3D : public CView
{
DECLARE_DYNCREATE(CViewGL3D)

enum glDisplayLists {
	glListAll = 1,
	glListCoord = 2,
	glListBodies = 3,
	glListBitmaps = 3,
	glListPrfPln = 4,
	glListRelief = 5,
	glListFld = 6,
};

protected:
	CViewGL3D();           // protected constructor used by dynamic creation
	virtual ~CViewGL3D();

	int		SetupLights(BOOL bEnable=TRUE);
	void	Properties(BOOL bInit = FALSE);
	void	SetProjectionMatrix(int w, int h);
	void	SetTransformations();
	int		CreateDisplayList(void);
	void	DrawScene();

//***********************************************************************************
private:
	CDC*	m_pDC;	// WinGDI Device Context
	HGLRC	m_hRC;	// OpenGL Rendering Context
	BOOL	m_bExternGLCall;
	BOOL	m_bExternDispListCall;
	BOOL	bSetupPixelFormat();

	CDC* GetCDC() {return m_pDC;};
//***********************************************************************************

protected:
	CMod3DDoc*	m_pDoc;
	CModel*		m_mod;

	int			m_nProjection;
	double		m_dOrthoScale;

	CView3dSheet* m_pSheetProperties;		// for apply buttun from property sheet

	double	m_xSc, m_ySc;
	double	m_xSize, m_ySize;
	int		m_xOff, m_yOff;
	DWORD	m_nRow, m_nCol;

	// Model extent in GL coordinates
	GLdouble m_minX, m_maxX;
	GLdouble m_maxY, m_minY;		
	GLdouble m_minZ, m_maxZ;		
	GLdouble m_midX, m_midY, m_midZ;		// model middle point
	GLdouble m_lenX, m_lenY, m_lenZ;		// axis model lengths
	GLdouble	m_diag;						// body diagonal
	// text labels in GL coordinates
	GLdouble m_minXl, m_minYl, m_minZl, m_stepXl, m_stepYl, m_stepZl;	
	GLuint	m_decXl, m_decYl, m_decZl;
	int		m_nTextType;
	// grid in Open GL coordinates
	BOOL	m_bGrdLineUpX, m_bGrdLineUpZ;
	BOOL	m_bGrdLineDwnX, m_bGrdLineDwnZ;
	BOOL	m_bGrdLineVrt;

	// Model scaling in GL coordinates
	double m_dScX, m_dScY, m_dScZ;

	// Model rotation - in degrees !
	float m_fRotX, m_fRotY, m_fRotZ;

	// Centered-transformation
	GLdouble m_tX, m_tY, m_tZ;

	// Constants for motion and rotation
	float m_mX,	m_mY, m_mZ;	
	float m_rX, m_rY, m_rZ; 

	// Camera position
	GLdouble m_dCameraX , m_dCameraY , m_dCameraZ;
	GLdouble m_dCameraEyeZ;
	GLdouble m_dProjAngle;								// perspective projection angle


	// --- plane fight --------------
	// Constants for plane flight motion and rotation
	// in GL coordinates
	BOOL	m_bPlaneSimulator;
	double	m_plX,	m_plY, m_plZ;							// PLane x, y, z position
	double	m_pld;											// PLane differences x, y, z position
	double	m_plRotX, m_plRotY, m_plRotZ, m_plRotV;			// rotations
	double	m_plRotdX, m_plRotdY, m_plRotdZ, m_plRotdV;		// rotations
	int		m_nTimer;

	// lights
	BOOL	m_bLightsEnabled;
	BOOL	m_bSetupLights;

	BOOL	m_bTransparentBodies;
	float	m_fAlphaBody;

	BOOL	m_bProfilePlanes;
	BOOL	m_bProfilePlanesSolid;
	float	m_fAlphaProf;

	BOOL	m_bDrawBitmaps;
	float	m_fAlphaBmp;

	// darken inside facets
	BOOL	m_bDarkenInside;
	float	m_fDarkenInside;

	// viewed part of model
	int		m_nRowStart, m_nRowEnd;
	int		m_nColStart, m_nColEnd;
	int		m_nColMax, m_nRowMax;

	// show properties
	BOOL	m_bOutFct;		// show model sides
	BOOL	m_bVrtFct;		// show horizontal facets
	BOOL	m_bCullFacet;	// culls the back or front facets
	int		m_nCullFacet;	// cull facet mode eithre GL_FRONT, GL_BACK

	// moving & rotating scene with mouse, temporary variables
	CPoint	m_ptOld;	// old mouse position in device coordinates (points)

	CRect	m_oldRect;
	int		m_glErrorCode;

	// Texture
	int		m_TextureName;
	int		m_nTextureSize;			// ! Must be a power of 2 ! (64, 128, 256, ...)
	bool	m_bTexCreated;

	// texture 1D
	DoubleArray	m_texRel1D;
	GLuint		m_nTexRel1D;

	DoubleArray	m_texFld1D;
	GLuint		m_nTexFld1D;

	DoubleArray	m_texFldDif1D;
	GLuint		m_nTexFldDif1D;

	// list of bitmaps
	BOOL			m_bUpdateBitmaps;
	MyObPtrArray	m_objArray;

	// surfaces
	BOOL		m_bBodySolid;
	CGlSurface	m_srfRel;

	// field
	CGlSurface	m_srfFld;
	double		m_dFldMinY, m_dFldMaxY;				// minimum & maximum field Y GL coordinate
	float		m_fAlphaFld;
	BOOL		m_bFldOnRel;

	// leading horizons
	BOOL		m_bHorizon;
	BOOL		m_bHorizonSolid;
	float		m_fHorizonAlpha;
	MyObPtrArray	m_objHrz;

	// common display list
	GLuint		m_nDispLstCoord;

	// fonts
	GLuint		m_charsetDListBase2D;				// Base Display List For The Font Set
	GLuint		m_charsetDListBase3D;				// Base Display List For The Font Set
	int			m_nChars;
	LOGFONT		m_lf;
	GLYPHMETRICSFLOAT* m_gmfvector;

public:
	virtual void OnDraw(CDC* pDC);      // overridden to draw this view
	CMod3DDoc* GetDocument();

#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:
	DECLARE_MESSAGE_MAP()

protected:
	void			SetModelDimensions(void);
	void			DrawCoordinates(void);
	virtual void	OnInitialUpdate();

	void			OnCreateGL();
	afx_msg int		OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void	OnDestroy();
	afx_msg void	OnSize(UINT nType, int cx, int cy);
	afx_msg BOOL	OnEraseBkgnd(CDC* pDC);

	afx_msg void	OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void	OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void	OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void	OnRButtonDown(UINT nFlags, CPoint point);

	virtual void OnUpdate(CView* /*pSender*/, LPARAM /*lHint*/, CObject* /*pHint*/);
public:
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	void	GetScaleShift(float& tx, float& ty, float& tz, float& sc);
	void	DrawProfilePlanes(void);
	void	DrawBodies(void);
	void	DrawImportedBitmaps(void);

	void DrawTest(void);

	// 2D text
	GLvoid	PrepareCharset2D(const LOGFONT* pLf);
	GLvoid	Text2D(CString str);

	// 3D text
	void	PrepareCharset3D(const LOGFONT* pLf, float extrusion, BOOL uselines=false, float precision=0.01f);
	float	Text3D(CString str);

	// loads images from document
	void LoadTextures(void);
	void UpdateTexturesShow(void);
	// // loads grid from document to this view structures
	void LoadGridSurface(CGrid* pGrd, CGlSurface* pSrf, int nClrType=CTBL_FLD, double min=1.0f, double max=1.0f, BOOL bRelief=FALSE);
	// draws small coordinatte sysem
	void DrawCoordSys(void);
	// draw orientation overview in plane simulator mode
	void DrawOrientation(void);
	void InitTextLabels(void);
	void FindLabels(GLdouble& min, GLdouble max, GLdouble& step, GLuint& dec);
	// draws axis labels 2D or 3D text
	void DrawAxisLabels(void);
	void DrawGridLines(void);
	// loads leading horizons from document to GLSurface
	void LoadLeadingHorizons(void);
	void DrawLeadingHorizons(void);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//****** field toolbar ***********************************************************************************************************************
protected:
	int			m_nFldTbl;			// field toolbar type
	int			m_nFld;				// displayed field

public:
	afx_msg void OnViewFieldModelG();
	afx_msg void OnUpdateViewFieldModelG(CCmdUI *pCmdUI);
	afx_msg void OnViewFieldModelGx();
	afx_msg void OnUpdateViewFieldModelGx(CCmdUI *pCmdUI);
	afx_msg void OnViewFieldModelGy();
	afx_msg void OnUpdateViewFieldModelGy(CCmdUI *pCmdUI);
	afx_msg void OnViewFieldModelGz();
	afx_msg void OnUpdateViewFieldModelGz(CCmdUI *pCmdUI);
	afx_msg void OnViewFieldModelM();
	afx_msg void OnUpdateViewFieldModelM(CCmdUI *pCmdUI);
	afx_msg void OnViewFieldModelMx();
	afx_msg void OnViewFieldModelMy();
	afx_msg void OnUpdateViewFieldModelMx(CCmdUI *pCmdUI);
	afx_msg void OnUpdateViewFieldModelMy(CCmdUI *pCmdUI);
	afx_msg void OnViewFieldModelMz();
	afx_msg void OnUpdateViewFieldModelMz(CCmdUI *pCmdUI);
	afx_msg void OnModelTxx();
	afx_msg void OnUpdateModelTxx(CCmdUI *pCmdUI);
	afx_msg void OnModelTxy();
	afx_msg void OnUpdateModelTxy(CCmdUI *pCmdUI);
	afx_msg void OnModelTxz();
	afx_msg void OnUpdateModelTxz(CCmdUI *pCmdUI);
	afx_msg void OnModelTyy();
	afx_msg void OnUpdateModelTyy(CCmdUI *pCmdUI);
	afx_msg void OnModelTyz();
	afx_msg void OnUpdateModelTyz(CCmdUI *pCmdUI);
	afx_msg void OnModelTzz();
	afx_msg void OnUpdateModelTzz(CCmdUI *pCmdUI);
	afx_msg void OnViewMeasuredField();
	afx_msg void OnUpdateViewMeasuredField(CCmdUI *pCmdUI);
	afx_msg void OnViewModeledField();
	afx_msg void OnUpdateViewModeledField(CCmdUI *pCmdUI);
	afx_msg void OnViewDifferenceField();
	afx_msg void OnUpdateViewDifferenceField(CCmdUI *pCmdUI);

	void	FindFieldIndex(int &n);
	void	CheckModField(int n);
	void	UpdateViewFieldMod(CCmdUI* pCmdUI, int n);
//****** end field toolbar *******************************************************************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//****** selection ***************************************************************************************************************************
protected:
	BOOL	m_bBottom;				// draw bottom boundary?
	BOOL	m_bSelecting;			// indicates that GL is in selection mode
	int		m_nMode;				// GL mode can be GL_RENDER || GL_SELECT
	int		m_nSelId;				// id of selected body
	int		m_nSelRow;				// row of selected point
	int		m_nSelCol;				// column -||-
	int		m_nSelIndex;			// index of point in model points array
	int		m_nSelIndexZ;			// depth index of point
	int		m_nSelIndexGL;			// index in GL array
	double	m_dSelZ;				// z value returned from selection
	float	m_fSelSensitivity;		// selection sensitivity in points

	double	m_modelMatrix[16];
	double	m_projMatrix[16];
	int		m_viewport[4];
	BOOL	m_bMatrix;

protected:
	void DrawSelectionPoints(int nMode=GL_RENDER);
	void SelectVertex(int nFlags, int x, int y);
	void ProcessHits (GLint hits, GLuint buffer[]);
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//****** end selection ***********************************************************************************************************************
public:
	BOOL CheckGlError(void);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	void DPtoGL(double x, double y, double z, double& gx, double& gy, double& gz, int tag=TRC_INV);
	// applies changes in properties
	afx_msg LRESULT OnPropertiesChange(WPARAM wParam, LPARAM lParam);
	void PropertiesChange(CView3dSheet* pSheet);
	void PropertiesInit(CView3dSheet* pSheet);
	afx_msg LRESULT OnSaveSettings(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnLoadSettings(WPARAM wParam, LPARAM lParam);
	void DrawWell(void);
	void DrawLegend(void);
	void DrawLegend(int xl, int xr, int yt, int yb, double minData, double maxData, int nTex, CString strName);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnViewPlanesimulator();
	afx_msg void OnUpdateViewPlanesimulator(CCmdUI *pCmdUI);
	void MapImage(void);
};

#ifndef _DEBUG  // debug version in Mod3DView.cpp
inline CMod3DDoc* CViewGL3D::GetDocument()
   { return (CMod3DDoc*)m_pDocument; }
#endif


