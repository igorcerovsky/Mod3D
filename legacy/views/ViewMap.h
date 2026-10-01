#pragma once

#include "Model.h"
#include "Mod3DDoc.h"
#include "ScaleAxis.h"


// structures
typedef struct ContourMap{
	CArray<CPoint, CPoint&> pts;	// array of points in Logical coordinates
	double		z;					// z value of contour
	COLORREF	color;
} CNTMAP;

typedef struct GridImgFlg {
	int		nGrid;			// emum describing grid 
	// image
	BOOL	bImg;			// show grid image
	BOOL	bImgUpdate;		// image update tag
	CImage	img;			// current grid image
	CImage	imgLegend;		// legend image
	// contours
	BOOL	bCnt;			// show grid contours
	BOOL	bCntUpdate;		// contours update tag
	CNTMAP	cnt;			// contours in logical coordinates
} GRDIMGFLD;

typedef CTypedPtrArray<CPtrArray, GRDIMGFLD*> ImgGrdArray;

typedef struct CaptureStructMapView {
	int nTag;					// CaptureTag
} CAPTRM;


// enums
typedef enum CursorTypeMapView {
	CRSM_NORMAL,				// CuRSor
	CRSM_OVER_SCBAR_H,
	CRSM_OVER_SCBAR_V,
	CRSM_OVER_OBJECT,
	CRSM_ZOOM,
	CRSM_ZOOM_IN,
	CRSM_ZOOM_OUT,
	CRSM_OVER_DIV_RIGHT,
	CRSM_OVER_DIV_LEFT,
	CRSM_OVER_DIV_TOP,
	CRSM_OVER_DIV_BOTTOM,
};

typedef enum CaptureTagMapView {
	CPTM_NONE,					// Column PoinT
	CPTM_MOVE_SB_VRT,
	CPTM_MOVE_SB_HRZ,
	CPTM_ZOOM,
	CPTM_ZOOM_IN,
	CPTM_ZOOM_OUT,
	CPTM_MOVE_DIV_RIGHT,
	CPTM_MOVE_DIV_LEFT,
	CPTM_MOVE_DIV_TOP,
	CPTM_MOVE_DIV_BOTTOM,
};

typedef enum FieldToolBarImgCntr {
	FTB_IMAGE,
	FTB_CONTOUR
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CViewMap view

class CViewMap : public CView
{
	DECLARE_DYNCREATE(CViewMap)
protected:
	CViewMap();           // protected constructor used by dynamic creation
	virtual ~CViewMap();

protected:
	CModel*	mod;

	CAPTRM	m_cpt;

	BOOL	m_bScale;				// the view needs rescale

	CRect	m_drwRect;
	CSize	m_szMargin;				// margin width
	CSize	m_szLeftTop;			// top left margin width in points
	CSize	m_szRightBot;			// right bottom margin width in points
	CSize	m_szDiv;				// legend margin width in points
	CSize	m_szObs;				// observation cross

	double	m_cx, m_cy;				// cell size
	double	m_x0, m_y0;				// origin
	double	m_xSc, m_xOff;
	double	m_ySc, m_yOff;

	int		m_nCursor;
	int		m_nObject;
	int		m_nRow;			// current row
	int		m_nCol;			// current column
	int		m_nRows;		// number of rows, extended included
	int		m_nCols;		// number of columns, extended included

	CPoint	m_ptFloatMenu;

	CPoint	m_bdCreatPoly[4];	// polygon used when body is extending

	// axes
	BOOL		m_bScaleEq;	// equal scale for x & y axes
	CScaleAxis	m_axX;		// horizontal scale axis; x-in logical coordinates
	CScaleAxis	m_axY;		// vertical scale axis;   y-in logical coordinates

	// observations
	BOOL	m_bObsPoints;	// show observation points
	BOOL	m_bShowObjects;	// show objects as bitmaps, guide lines...
	BOOL	m_bShowProfiles;// show active profiles

	// bodies
	BOOL				m_bBodies;
	CArray<CPen, CPen&> m_bodyPen;

	// Drawing
	LOGFONT	m_lfDefault;
	// Observations points
	CPen	m_penObs;
	// profiles drawing
	CPen	m_penProf;
	CPen	m_penProfCur;
	LOGPEN	m_lpProf;
	LOGPEN	m_lpProfCur;
	LOGFONT	m_lfProf;
	LOGFONT	m_lfProfCur;
	CBrush	m_brushHollow;

	// zoom
	BOOL		m_bZooming;
	CRect		m_zoomRect;

	// field toolbar
	ImgGrdArray	m_imgGrd;			// array of grid images and contours
	int			m_nFldType;			// modeled || measured || difference
	int			m_nFldDisp;			// contours or image
	GRDIMGFLD	m_obs;				// observation grid
	float		m_dImgQ;			// image quality <0, 1>
	int			m_nAlphaImg;
	int			m_nAlphaRel;

	// printing
	CRect	m_rcPrn;
	CRect	m_rcPrnMargin;
	CPoint	m_ptPaperSize;

public:
	virtual void OnDraw(CDC* pDC);      // overridden to draw this view
	CMod3DDoc* GetDocument() const;
	CMod3DDoc* m_pDoc;

#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:
	DECLARE_MESSAGE_MAP()
public:
	virtual void OnInitialUpdate();
protected:
	virtual void OnUpdate(CView* /*pSender*/, LPARAM /*lHint*/, CObject* /*pHint*/);
	void	Scale(BOOL bInit=FALSE, BOOL bPrint = FALSE);

public:
	void SetVariables(bool bInit);
	virtual void OnPrepareDC(CDC* pDC, CPrintInfo* pInfo = NULL);

protected:
	// coordinate transform functions
	inline void DPtoLP(CPoint& pt);
	inline void WPtoLP(int& x, int& y, double xw, double yw);		// world coordinates to DC logical coordinates
	inline void WPtoLP(CPoint& pt, double xw, double yw);			// world coordinates to DC logical coordinates
	inline void LPtoWP(double& xw, double& yw, int xl, int yl);		// DC logical coordinates to world coordinates 
	inline void WPtoDP(int& x, int& y, double xw, double yw);		// world coordinates to device coordinates (points)
	inline void DPtoWP(CPoint pt, double &xw, double &yw);			// device coordinates to world coordinates
	inline void WPtoLP(CPoint &pt, int nIndex, int row, int col);
	inline void WPtoDP(CPoint& pt, double xw, double yw);

	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	int		IsCursorOverScaleBar(CPoint &pt);
	int		IsCursorOverObject(CPoint &pt);
	void	MouseMoveCaptured(UINT nFlags, CPoint point);
	
	// drawing functions
	void	Draw(CDC* pDC);
	void	CreateBodyPen(void);
	void	DrawObservation(CDC* pDC);
	void	DrawBodies(CDC* pDC);
	void	DrawSelectedProfiles(CDC *pDC);
	void	DrawGrid(CDC* pDC);
	void	DrawObjects(CDC* pDC);

public:
	// zooming
	afx_msg void OnViewZoom();
	afx_msg void OnViewZoomIn();
	afx_msg void OnViewZoomOut();
	afx_msg void OnViewZoomPage();
	void	Zoom(CPoint pt);		
	void	ZoomIn(CPoint pt);
	void	ZoomOut(CPoint pt);
	// field toolbar
	afx_msg void OnViewFieldContours();
	afx_msg void OnUpdateViewFieldContours(CCmdUI *pCmdUI);
	afx_msg void OnViewFieldImage();
	afx_msg void OnUpdateViewFieldImage(CCmdUI *pCmdUI);
	afx_msg void OnViewFieldModelGx();
	afx_msg void OnUpdateViewFieldModelGx(CCmdUI* pCmdUI);
	afx_msg void OnViewFieldModelGy();
	afx_msg void OnUpdateViewFieldModelGy(CCmdUI* pCmdUI);
	afx_msg void OnViewFieldModelGz();
	afx_msg void OnUpdateViewFieldModelGz(CCmdUI* pCmdUI);
	afx_msg void OnViewFieldModelG();
	afx_msg void OnUpdateViewFieldModelG(CCmdUI* pCmdUI);
	afx_msg void OnViewFieldModelMx();
	afx_msg void OnUpdateViewFieldModelMx(CCmdUI* pCmdUI);
	afx_msg void OnViewFieldModelMy();
	afx_msg void OnUpdateViewFieldModelMy(CCmdUI* pCmdUI);
	afx_msg void OnViewFieldModelMz();
	afx_msg void OnUpdateViewFieldModelMz(CCmdUI* pCmdUI);
	afx_msg void OnViewFieldModelM();
	afx_msg void OnUpdateViewFieldModelM(CCmdUI* pCmdUI);
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
	void	FindFieldIndex(int &n);
	void	SetField(int n);
	void 	UpdateViewField(CCmdUI* pCmdUI, int n);
	GRDIMGFLD* GetImgGrd(int nFld);
	void	RemoveImgGrd(int nFld);


	afx_msg void OnViewMeasuredField();
	afx_msg void OnUpdateViewMeasuredField(CCmdUI *pCmdUI);
	afx_msg void OnViewModeledField();
	afx_msg void OnUpdateViewModeledField(CCmdUI *pCmdUI);
	afx_msg void OnViewDifferenceField();
	afx_msg void OnUpdateViewDifferenceField(CCmdUI *pCmdUI);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	int FindIndex(CPoint pt);
	afx_msg void OnViewShowObjects();
	afx_msg void OnUpdateViewShowObjects(CCmdUI *pCmdUI);
	afx_msg void OnViewObservationContours();
	afx_msg void OnUpdateViewObservationContours(CCmdUI *pCmdUI);
	afx_msg void OnViewObservationImage();
	afx_msg void OnUpdateViewObservationImage(CCmdUI *pCmdUI);
	afx_msg void OnViewEqualScale();
	afx_msg void OnUpdateViewEqualScale(CCmdUI *pCmdUI);
	afx_msg void OnViewObservationPoints();
	afx_msg void OnUpdateViewObservationPoints(CCmdUI *pCmdUI);
	afx_msg void OnViewBodyBoundaries();
	afx_msg void OnUpdateViewBodyBoundaries(CCmdUI *pCmdUI);
	int AutomaticPan(CPoint pt);
	int IsCursorOverDiv(CPoint pt);
	afx_msg void OnViewProfiles();
	afx_msg void OnUpdateViewProfiles(CCmdUI *pCmdUI);
	void Properties(void);
	afx_msg void OnFilePrintSetup();
protected:
	virtual BOOL OnPreparePrinting(CPrintInfo* pInfo);
	int		GetMargin(CDC* pDC, CSize& szMargin, BOOL bInit);

public:
	afx_msg void OnFilePrint();
	afx_msg void OnFilePrintPreview();

protected:
	virtual void OnEndPrinting(CDC* pDC, CPrintInfo* pInfo);
};

#ifndef _DEBUG  // debug version in ViewMap.cpp
inline CMod3DDoc* CViewMap::GetDocument() const
   { return reinterpret_cast<CMod3DDoc*>(m_pDocument); }
#endif

