// 3dDefView.h : interface of the CViewProf class
//


#pragma once

#include "ScaleAxis.h"

// structures
typedef struct CaptureStruct {
	int nTag;					// CaptureTag
	int nBodyID;				// ID of body which is captured
	int nBodyIndex;
	int nPrfIndexOld;			// old profile index	
	CPoint	ptPrev;				// previously clicked point
} CAPTR;

typedef struct BodyPlg {
	int			id;				// body ID
	int			nPen;			// pen index
	int			nBrush;			// brush index
	BOOL		bFill;			// is filled
	CPoint		plg[4];			// polygon for given column
} BODYPLG, *PBODYPLG;

typedef CArray<BodyPlg, BodyPlg&> BodyPlgArray;				// polygons for given column
typedef CArray<BodyPlgArray, BodyPlgArray&> ClmnPlgArray;	// polygons for given profile
typedef CArray<CPen, CPen&> PenArray;						// CPen for bodies; array index is similar to BodyArray index !!!
typedef CArray<CBrush, CBrush&> BrushArray;					// CBrush for bodies; array index is similar to BodyArray index !!!
typedef	CArray<CPoint, CPoint&> PointArray;					// array of points in Logical coordinates

// enums
typedef enum CursorType {
	CRS_NORMAL = 0,				// CuRSor
	CRS_OVER_LINE,
	CRS_OVER_VERTEX,
	CRS_OVER_VERTEX_LOCKED,
	CRS_OVER_BODY,
	CRS_OVER_BODY_LINE,
	CRS_OVER_SCBAR_H,
	CRS_OVER_SCBAR_V,
	CRS_OVER_SCBAR_FLD,
	CRS_OVER_DIV,
	CRS_OVER_OBJECT,
	CRS_ZOOM,
	CRS_ZOOM_IN,
	CRS_ZOOM_OUT,
	CRS_OVER_DIV_RIGHT,
	CRS_OVER_DIV_LEFT,
	CRS_OVER_DIV_TOP,
	CRS_OVER_DIV_BOTTOM,
};

typedef enum CaptureRTag {
	CPT_NONE = 0,				
	CPT_MOVE_POINT,
	CPT_MOVE_EDGE,
	CPT_MOVE_SB_VRT,
	CPT_MOVE_SB_HRZ,
	CPT_MOVE_SB_FLD,
	CPT_MOVE_DIV,
	CPT_ZOOM,
	CPT_ZOOM_IN,
	CPT_ZOOM_OUT,
	CPT_MOVE_DIV_RIGHT,
	CPT_MOVE_DIV_LEFT,
	CPT_MOVE_DIV_TOP,
	CPT_MOVE_DIV_BOTTOM,
	CPT_MOVE_BODY_V,
};

typedef enum ProfileType {
	PROF_X = 0,				// profile parallel to x axis
	PROF_Y = 1,				// profile parallel to y axis
};

typedef enum InsBoby {
	INSBD_NEW,				
	INSBD_EXISTING,
	INSBD_EXTEND,
};

typedef enum ProfPrevNext {
	PPN_CURRENT=0,				
	PPN_PREV=-1,
	PPN_NEXT=1,
};




class CViewProf : public CView
{
protected: // create from serialization only
	CViewProf();
	DECLARE_DYNCREATE(CViewProf)

// Attributes
public:
	CMod3DDoc* GetDocument() const;
	CMod3DDoc* m_pDoc;

// Operations
public:

// Overrides
	public:
	virtual void OnDraw(CDC* pDC);  // overridden to draw this view
virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
protected:
	virtual BOOL OnPreparePrinting(CPrintInfo* pInfo);
	virtual void OnBeginPrinting(CDC* pDC, CPrintInfo* pInfo);
	virtual void OnEndPrinting(CDC* pDC, CPrintInfo* pInfo);

// Implementation
public:
	virtual ~CViewProf();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:
	CToolTipCtrl	m_toolTip;
	BOOL			m_bToolTip;
	CString			m_strToolTip;
	int				m_nToolTip;

	// drawing background DC
	BOOL	m_bBmp; 
	CBitmap	m_bmp;
	CDC		m_dc;
	CRect	m_clientRectOld;

	// profile type
	int		m_nPrfType;
	
	// model
	CModel*	mod;
	
	// extrem field
	int		m_nFldExtrem;	// field used to jump to field extrem

	CAPTR	m_cpt;

	BOOL	m_bInit;		// if TRUE, the view new initialization
	BOOL	m_bScale;		// if TRUE, the view needs rescaling
	CRect	m_drwRect;
	CRect	m_drwRectFld;
	CSize	m_szMargin;		// in device coordinates
	CSize	m_szLeftTop;	// top left margin width in points
	CSize	m_szRightBot;	// right bottom margin width in points
	CSize	m_szCross;		// in device coocrdinates
	float	m_fResize;		// window size ratio <0.1, 0.9>

	double	m_xSc, m_xOff;
	double	m_ySc, m_yOff;

	int		m_nCursor;
	int		m_nRow;			// current row
	int		m_nCol;			// current column
	int		m_nRC;			// number of rows or columns for given view
	int		m_nRcIndex;		// current profile index, either row or column depending on view
	int		m_nRowOld;		// old row index
	int		m_nColOld;		// old column index
	int		m_nIndex;		// current point index !!!
	int		m_nIndexID;		// current point ID !!!
	int		m_nPrfIndex;	// perpendicular profile index == current row OR column -> depends on view !!!
	int		m_nCrsIndex;	// perpendicular cusor profile index == current row OR column -> depends on view !!!

	// profile
	double	m_xy0;			// profile coordinate x0, drawing x0 or y0
	double	m_cs;			// cell size x or y, depends os profile type

	int		m_nObsPt;

	CPoint	m_ptFloatMenu;

	LOGFONT	m_lf;

	CPoint	m_bdCreatPoly[4];

	CArray <CGuideMark, CGuideMark&> m_guideMarks;		// array of points in Logical coordinates

	// body polygons draw
	ClmnPlgArray	m_plg;			// polygons array for given profile
	BOOL			m_bProfPrev;	// show previous profile
	ClmnPlgArray	m_plgPrev;		// polygons array for previous profile
	BOOL			m_bProfNext;	// show next profile
	ClmnPlgArray	m_plgNext;		// polygons array for next profile
	PenArray		m_pen;			// generated pens for polygons
	PenArray		m_penPrevNext;	// generated pens for polygons for next and previous profile
	BrushArray		m_brs;			// generated brushes for polygons 
	CBrush			m_brushHollow;	// hoolow default brush
	BOOL			m_bModelGL;		// show vertical model guideline

	// axes
	CScaleAxis	m_axH;				// horizontal scale axis; x-in logical coordinates
	CScaleAxis	m_axV;				// vertical scale axis;   y-in logical coordinates
	CScaleAxis	m_axFldVirtual;		// vertical field scale axis;
	CScaleAxis	m_axFld;			// vertical field scale axis;
	BOOL		m_bInitAxis;
	BOOL		m_bAxRange;			// takes maximum range for measued and modeled field

	// field
	CPen		m_penFld[60];		// field pens
	CPen		m_penRelief;		// relief pen

	// show field
	BOOL		m_bScaleEq;			// equal scale for x & y axes
	BOOL		m_bf[60];			// show field 
	int			m_nFldTbl;			// field toolbar type
	BOOL		m_bAxis;			// axis toolbar active
	int			m_nFldAxis;			// axis for fld scale
	BOOL		m_bUpdateFldAxis;

	// RMS picture
	HICON		m_iconRmsUp;		// increasing
	HICON		m_iconRmsDwn;		// decreasing

	// zooming
	BOOL	m_bZoomig;
	CRect	m_zoomRect;				// zooming rectangle in device coordinates

	// show derivative or RMS values
	BOOL	m_bShowRms;
	BOOL	m_bShowDrv;

	// printing
	CRect	m_rcPrn;
	CRect	m_rcPrnMargin;
	CPoint	m_ptPaperSize;

// Generated message map functions
protected:
	DECLARE_MESSAGE_MAP()

public:
	int GetRow()		{return m_nRow;}
	int GetCol()		{return m_nCol;}
	int	GetProfType()	{return m_nPrfType;}

protected:
	virtual void OnPrepareDC(CDC* pDC, CPrintInfo* pInfo = NULL);
	virtual void OnInitialUpdate();

	// update functions
	virtual void OnUpdate(CView* /*pSender*/, LPARAM /*lHint*/, CObject* /*pHint*/);
	void	UpdatePlg(ClmnPlgArray& plg, int row, int col, int startIncr=-1, int endIncr=1,int nTag=PPN_CURRENT);
	void	UpdatePlgAll();

	// drawing functions
	void	DrawInit(CDC* pDC) ;
	void	Draw(CDC* pDC);
	void	DrawField(CDC *pDC);
	void	DrawGrdLines(CDC* pDC);
	void	DrawPlg(CDC* pDC);
	void	CreatePenBrush(void);
	void	InitPenFld();
	void	DrawImages(CDC* pDC);
	void	DrawRelief(CDC* pDC);
	void	DrawCheckMarks(CDC* pDC);
	void	DrawWells(CDC* pDC);

	// coordinate transform functions
	void	Scale(CDC *pDC);
	void	ScaleFieldAxis();
	void	UpdateScale(void);
	inline void WPtoLP(int& x, int& y, double xw, double yw);		// world coordinates to DC logical coordinates
	inline void WPtoLP(CPoint& pt, double xw, double yw);			// world coordinates to DC logical coordinates
	inline void LPtoWP(double& xw, double& yw, int xl, int yl);		// DC logical coordinates to world coordinates 
	inline void WPtoDP(int& x, int& y, double xw, double yw);		// world coordinates to device coordinates (points)
	inline void DPtoWP(CPoint pt, double &xw, double &yw);			// device coordinates to world coordinates
	inline void WPtoLP(CPoint &pt, int nIndex, int row, int col);
	inline void WPtoDP(CPoint& pt, double xw, double yw);
	virtual void DPtoLP(CPoint& pt);

	BOOL			IsPointInPolygon(CPoint pt, CPoint* plg, int n);
	inline	void	FindIndex(CPoint pt);											// find profile index
	inline	double	GetXYd(int n);													// get x or y world draw coordinate for given profile
	inline	void	GetXZd(int nIndex, int row, int col, double& x, double& z);		// get x or y world draw coordinate for given profile and z coordinate for given index
	inline	void	GetRowCol(int &row, int &col, int nPrfIndexOld) ;

	// cursor over....;
	//BOOL IsCursorOverPoint(CPoint &pt, int row, int col);
	int IsCursorOverLine(CPoint &pt, int row, int col);
	int IsCursorOverScaleBar(CPoint &pt);
	int IsCursorOverDiv(CPoint pt);


// message handlers
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnSize(UINT nType, int cx, int cy);

protected:
	void	SetVariables();
	void	MoveProfile(int incr);
	void	MoveProfileTo(int nPrf);
	void	CopyProfile(int nIncr);
	void	MouseMoveCaptured(UINT nFlags, CPoint point);
	int		MovePoint(CPoint point, int nFlags);
	int		MoveEdge(CPoint point, int nFlags);
	void	MoveDiv(CPoint point);
	void	MoveBody(CPoint ptMoveTo, int nFlags);

public:
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	// floating body
	void	FloatMenuBody(CPoint pt);
	void	FloatMenuBodyLine(CPoint pt);
	void	FloatMenuProfile(CPoint pt);
	afx_msg void OnBodyActive();
	afx_msg void OnBodyCopyToNextProfile();
	afx_msg void OnBodyCopyToPreviousProfile();
	afx_msg void OnBodyEdit();
	afx_msg void OnBodyFill();
	afx_msg void OnBodyInsertExisting();
	afx_msg void OnBodyInsertNew();
	afx_msg void OnBodyLock();
	afx_msg void OnBodyProperties();
	afx_msg void OnBodyRemove();
	afx_msg void OnBodyRemoveFromProfile();
	afx_msg void OnBodyShow();
	afx_msg void OnModelBodyCreationProperties();
	afx_msg void OnViewNextProfile();
	afx_msg void OnViewPreviousProfile();

	// Field toolbar
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
	afx_msg void OnViewAxisField();
	afx_msg void OnUpdateViewAxisField(CCmdUI *pCmdUI);
	afx_msg void OnViewMeasuredField();
	afx_msg void OnUpdateViewMeasuredField(CCmdUI *pCmdUI);
	afx_msg void OnViewModeledField();
	afx_msg void OnUpdateViewModeledField(CCmdUI *pCmdUI);
	afx_msg void OnViewDifferenceField();
	afx_msg void OnUpdateViewDifferenceField(CCmdUI *pCmdUI);

	void	FindFieldIndex(int &n);
	void	CheckModField(int n);
	void	UpdateViewFieldMod(CCmdUI* pCmdUI, int n);
	void	UpdateViewFieldScale(CCmdUI* pCmdUI, int n);
	void	CheckAxis(int n);

	// zooming
	afx_msg void OnViewZoom();
	afx_msg void OnViewZoomIn();
	afx_msg void OnViewZoomOut();
	afx_msg void OnViewZoomPage();
	void	Zoom(CPoint pt);		
	void	ZoomIn(CPoint pt);
	void	ZoomOut(CPoint pt);

	// profile
	afx_msg void OnProfileEW();
	afx_msg void OnUpdateProfileEW(CCmdUI *pCmdUI);
	afx_msg void OnProfileSN();
	afx_msg void OnUpdateProfileSN(CCmdUI *pCmdUI);
	void	RemoveBody(void);
	void	InsertBody(CPoint pt, int nTag = INSBD_NEW);
	void	RemoveBodyFromProfile(void);

	afx_msg void OnViewEqualScale();
	afx_msg void OnUpdateViewEqualScale(CCmdUI *pCmdUI);
	afx_msg void OnProfileNextShow();
	afx_msg void OnUpdateProfileNextShow(CCmdUI *pCmdUI);
	afx_msg void OnProfilePreviousShow();
	afx_msg void OnUpdateProfilePreviousShow(CCmdUI *pCmdUI);
	int CraeteGuideMarks(void);
	void DrawLeadingHorizon(CDC* pDC);
	afx_msg void OnFilePrintSetup();
	afx_msg void OnProfileShowModelGuideline();
	afx_msg void OnUpdateProfileShowModelGuideline(CCmdUI *pCmdUI);
	int GetMargin(CDC* pDC, CSize& szMargin);
	void GetProfRect(double& xt, double& yt, double& zt, double& xb, double& yb, double& zb);
	afx_msg void OnProfileExtrem();
	afx_msg void OnUpdateProfileExtrem(CCmdUI *pCmdUI);
	afx_msg void OnProfileSetExtremField();
	afx_msg void OnUpdateProfileSetExtremField(CCmdUI *pCmdUI);
	afx_msg void OnProfileJumptomax();
	afx_msg void OnUpdateProfileJumptomax(CCmdUI *pCmdUI);
	afx_msg void OnProfileJumptomin();
	afx_msg void OnUpdateProfileJumptomin(CCmdUI *pCmdUI);
	afx_msg void OnProfileSaveCurrentSettings();
	void LoadProfileSettings(PRFVIEWSET profSet);
	void DrawProfileIntersections(CDC* pDC);
	afx_msg void OnComputeFit1d();
	afx_msg void OnVertexFit();
	afx_msg void OnUpdateVertexFit(CCmdUI *pCmdUI);
	void FloatMenuVertex(CPoint pt);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnDestroy();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	// sets tool tip string
	void SetToolTip(int tag=0);
	afx_msg void OnProfileShowTooltip();
	afx_msg void OnUpdateProfileShowTooltip(CCmdUI *pCmdUI);
	afx_msg void OnProfileShowDrv();
	afx_msg void OnUpdateProfileShowDrv(CCmdUI *pCmdUI);
	afx_msg void OnProfileShowRms();
	afx_msg void OnUpdateProfileShowRms(CCmdUI *pCmdUI);
	afx_msg void OnBodyInvertDensity();
	afx_msg void OnBodyDensityInversionProperties();
	afx_msg void OnViewExtendrange();
	afx_msg void OnUpdateViewExtendrange(CCmdUI *pCmdUI);
	afx_msg void OnBodyMoveboby();
	afx_msg void OnUpdateBodyMoveboby(CCmdUI *pCmdUI);
};



#ifndef _DEBUG  // debug version in 3dDefView.cpp
inline CMod3DDoc* CViewProf::GetDocument() const
   { return reinterpret_cast<CMod3DDoc*>(m_pDocument); }
#endif

