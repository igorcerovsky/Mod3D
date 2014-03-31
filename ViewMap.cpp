// ViewMap.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "ViewMap.h"
#include "Mod3DDoc.h"
#include <math.h>
#include "ViewProf.h"
#include "DlgViewMapProperties.h"

extern double g_x, g_y, g_z;

// CViewMap

IMPLEMENT_DYNCREATE(CViewMap, CView)

CViewMap::CViewMap()
{
	mod = NULL;
	m_bShowObjects = TRUE;
	m_nObject = -1;
	m_nCursor = 0;
	m_axY.SetType(SB_VERT);
	m_szObs = CSize(1, 1);
	m_bObsPoints = TRUE;
	m_obs.bCnt = FALSE;
	m_obs.bImg = FALSE;
	m_bBodies = TRUE;

	m_szDiv = CSize(100, 100);
	m_szLeftTop = CSize(50, 20);
	m_szRightBot = CSize(100, 30);

	m_penObs.CreatePen(PS_DOT, 1, RGB(150, 150, 150));
	m_penProf.CreatePen(PS_DOT, 1, RGB(150, 150, 150));
	m_penProfCur.CreatePen(PS_DOT, 1, RGB(150, 10, 10));

	// default font
	memset(&m_lfDefault, 0, sizeof(LOGFONT));
	m_lfDefault.lfHeight = 80;
	strcpy(m_lfDefault.lfFaceName, "Arial");

	// default hollow brush
	LOGBRUSH lb;
	lb.lbStyle = BS_HOLLOW;
	m_brushHollow.CreateBrushIndirect( &lb );

	m_bZooming = FALSE;

	m_nFldDisp = FTB_IMAGE;
	m_nFldType = FLDTLB_MODELED;
	m_dImgQ = 0.2f;
	m_nAlphaImg = 160;
	m_nAlphaRel = 160;
	m_bScaleEq = TRUE;
	m_bShowProfiles = TRUE;

	// printing
	m_rcPrnMargin.top = 2000;
	m_rcPrnMargin.left = 2000;
	m_rcPrnMargin.right = 2000;
	m_rcPrnMargin.bottom = 2000;
	m_ptPaperSize.x = 21590;
	m_ptPaperSize.y = 27940;

	m_bScale = TRUE;
}

CViewMap::~CViewMap()
{
}

void CViewMap::OnInitialUpdate()
{
	CView::OnInitialUpdate();

	m_pDoc = GetDocument();
	mod = m_pDoc->GetModel();

	if( mod )
		SetVariables(TRUE);
}


BEGIN_MESSAGE_MAP(CViewMap, CView)
	ON_WM_SIZE()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_SETCURSOR()
	ON_WM_ERASEBKGND()
	// zooming
	ON_COMMAND(ID_VIEW_ZOOM, OnViewZoom)
	ON_COMMAND(ID_VIEW_ZOOM_IN, OnViewZoomIn)
	ON_COMMAND(ID_VIEW_ZOOM_OUT, OnViewZoomOut)
	ON_COMMAND(ID_VIEW_ZOOM_PAGE, OnViewZoomPage)
	// field toolbar
	ON_COMMAND(ID_VIEW_FIELD_CONTOURS, OnViewFieldContours)
	ON_UPDATE_COMMAND_UI(ID_VIEW_FIELD_CONTOURS, OnUpdateViewFieldContours)
	ON_COMMAND(ID_VIEW_FIELD_IMAGE, OnViewFieldImage)
	ON_UPDATE_COMMAND_UI(ID_VIEW_FIELD_IMAGE, OnUpdateViewFieldImage)

	ON_COMMAND(ID_VIEW_FIELD_MODEL_GX, OnViewFieldModelGx)
	ON_UPDATE_COMMAND_UI(ID_VIEW_FIELD_MODEL_GX, OnUpdateViewFieldModelGx)
	ON_COMMAND(ID_VIEW_FIELD_MODEL_GY, OnViewFieldModelGy)
	ON_UPDATE_COMMAND_UI(ID_VIEW_FIELD_MODEL_GY, OnUpdateViewFieldModelGy)
	ON_COMMAND(ID_VIEW_FIELD_MODEL_GZ, OnViewFieldModelGz)
	ON_UPDATE_COMMAND_UI(ID_VIEW_FIELD_MODEL_GZ, OnUpdateViewFieldModelGz)
	ON_COMMAND(ID_VIEW_FIELD_MODEL_G, OnViewFieldModelG)
	ON_UPDATE_COMMAND_UI(ID_VIEW_FIELD_MODEL_G, OnUpdateViewFieldModelG)
	ON_COMMAND(ID_VIEW_FIELD_MODEL_MX, OnViewFieldModelMx)
	ON_UPDATE_COMMAND_UI(ID_VIEW_FIELD_MODEL_MX, OnUpdateViewFieldModelMx)
	ON_COMMAND(ID_VIEW_FIELD_MODEL_MY, OnViewFieldModelMy)
	ON_UPDATE_COMMAND_UI(ID_VIEW_FIELD_MODEL_MY, OnUpdateViewFieldModelMy)
	ON_COMMAND(ID_VIEW_FIELD_MODEL_MZ, OnViewFieldModelMz)
	ON_UPDATE_COMMAND_UI(ID_VIEW_FIELD_MODEL_MZ, OnUpdateViewFieldModelMz)
	ON_COMMAND(ID_VIEW_FIELD_MODEL_M, OnViewFieldModelM)
	ON_UPDATE_COMMAND_UI(ID_VIEW_FIELD_MODEL_M, OnUpdateViewFieldModelM)
	ON_COMMAND(ID_MODEL_TXX, OnModelTxx)
	ON_UPDATE_COMMAND_UI(ID_MODEL_TXX, OnUpdateModelTxx)
	ON_COMMAND(ID_MODEL_TXY, OnModelTxy)
	ON_UPDATE_COMMAND_UI(ID_MODEL_TXY, OnUpdateModelTxy)
	ON_COMMAND(ID_MODEL_TXZ, OnModelTxz)
	ON_UPDATE_COMMAND_UI(ID_MODEL_TXZ, OnUpdateModelTxz)
	ON_COMMAND(ID_MODEL_TYY, OnModelTyy)
	ON_UPDATE_COMMAND_UI(ID_MODEL_TYY, OnUpdateModelTyy)
	ON_COMMAND(ID_MODEL_TYZ, OnModelTyz)
	ON_UPDATE_COMMAND_UI(ID_MODEL_TYZ, OnUpdateModelTyz)
	ON_COMMAND(ID_MODEL_TZZ, OnModelTzz)
	ON_UPDATE_COMMAND_UI(ID_MODEL_TZZ, OnUpdateModelTzz)

	ON_COMMAND(ID_VIEW_MEASURED_FIELD, OnViewMeasuredField)
	ON_UPDATE_COMMAND_UI(ID_VIEW_MEASURED_FIELD, OnUpdateViewMeasuredField)
	ON_COMMAND(ID_VIEW_MODELED_FIELD, OnViewModeledField)
	ON_UPDATE_COMMAND_UI(ID_VIEW_MODELED_FIELD, OnUpdateViewModeledField)
	ON_COMMAND(ID_VIEW_DIFFERENCE_FIELD, OnViewDifferenceField)
	ON_UPDATE_COMMAND_UI(ID_VIEW_DIFFERENCE_FIELD, OnUpdateViewDifferenceField)
	ON_WM_RBUTTONDOWN()
	ON_COMMAND(ID_VIEW_SHOW_OBJECTS, OnViewShowObjects)
	ON_UPDATE_COMMAND_UI(ID_VIEW_SHOW_OBJECTS, OnUpdateViewShowObjects)
	ON_COMMAND(ID_VIEW_OBSERVATION_CONTOURS, OnViewObservationContours)
	ON_UPDATE_COMMAND_UI(ID_VIEW_OBSERVATION_CONTOURS, OnUpdateViewObservationContours)
	ON_COMMAND(ID_VIEW_OBSERVATION_IMAGE, OnViewObservationImage)
	ON_UPDATE_COMMAND_UI(ID_VIEW_OBSERVATION_IMAGE, OnUpdateViewObservationImage)
	ON_COMMAND(ID_VIEW_EQUAL_SCALE, OnViewEqualScale)
	ON_UPDATE_COMMAND_UI(ID_VIEW_EQUAL_SCALE, OnUpdateViewEqualScale)
	ON_COMMAND(ID_VIEW_OBSERVATION_POINTS, OnViewObservationPoints)
	ON_UPDATE_COMMAND_UI(ID_VIEW_OBSERVATION_POINTS, OnUpdateViewObservationPoints)
	ON_COMMAND(ID_VIEW_BODY_BOUNDARIES, OnViewBodyBoundaries)
	ON_UPDATE_COMMAND_UI(ID_VIEW_BODY_BOUNDARIES, OnUpdateViewBodyBoundaries)
	ON_COMMAND(ID_VIEW_PROFILES, OnViewProfiles)
	ON_UPDATE_COMMAND_UI(ID_VIEW_PROFILES, OnUpdateViewProfiles)
	ON_COMMAND(ID_FILE_PRINT_SETUP, OnFilePrintSetup)
	ON_COMMAND(ID_FILE_PRINT, OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_PREVIEW, OnFilePrintPreview)
END_MESSAGE_MAP()


// CViewMap diagnostics

#ifdef _DEBUG
void CViewMap::AssertValid() const
{
	CView::AssertValid();
}

void CViewMap::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}

CMod3DDoc* CViewMap::GetDocument() const // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CMod3DDoc)));
	return (CMod3DDoc*)m_pDocument;
}

#endif //_DEBUG




///////////////////////////////////////////////////////////////////////////////////////////
//******** cordinate transformations ******************************************************
void CViewMap::DPtoLP(CPoint& pt)
{
	CClientDC dc(this);
	OnPrepareDC( &dc );
	dc.DPtoLP( &pt );
}

void CViewMap::WPtoLP(int& x, int& y, double xw, double yw)
{
	x = (int) (m_xOff + m_xSc*xw);
	y = (int) (m_yOff + m_ySc*yw);
}

void CViewMap::WPtoLP(CPoint &pt, double xw, double yw)
{
	pt.x = (int) (m_xOff + m_xSc*xw);
	pt.y = (int) (m_yOff + m_ySc*yw);
}

void CViewMap::WPtoDP(int& x, int& y, double xw, double yw)
{
	x = (int) (m_xOff + m_xSc*xw);
	y = (int) (m_yOff + m_ySc*yw);
	CPoint	pt(x, y);
	CClientDC dc(this);
	OnPrepareDC( &dc );
	dc.LPtoDP( &pt );
	x = pt.x;
	y = pt.y;
}

void CViewMap::WPtoDP(CPoint& pt, double xw, double yw)
{
	pt.x = (int) (m_xOff + m_xSc*xw);
	pt.y = (int) (m_yOff + m_ySc*yw);
	CClientDC dc(this);
	OnPrepareDC( &dc );
	dc.LPtoDP( &pt );
}

void CViewMap::DPtoWP(CPoint pt, double &xw, double &yw)
{
	CClientDC dc(this);
	OnPrepareDC( &dc );
	dc.DPtoLP( &pt );
	
	xw = (double)(pt.x-m_xOff) / m_xSc;
	yw = (double)(pt.y-m_yOff) / m_ySc;
}

void CViewMap::LPtoWP(double& xw, double& yw, int xl, int yl)
{
	xw = (xl-m_xOff) / m_xSc;
	yw = (yl-m_yOff) / m_ySc;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CViewProf printing
inline int roundleast(int n)
{
	int mod = n%10;
	n -= mod;
	if (mod >= 5)
		n += 10;
	else if (mod <= -5)
		n -= 10;
	return n;
}

static void RoundRect(LPRECT r1)
{
	r1->left = roundleast(r1->left);
	r1->right = roundleast(r1->right);
	r1->top = roundleast(r1->top);
	r1->bottom = roundleast(r1->bottom);
}

static void MulDivRect(LPRECT r1, LPRECT r2, int num, int div)
{
	r1->left = MulDiv(r2->left, num, div);
	r1->top = MulDiv(r2->top, num, div);
	r1->right = MulDiv(r2->right, num, div);
	r1->bottom = MulDiv(r2->bottom, num, div);
}

void CViewMap::OnFilePrintSetup()
{
	CWinApp* pApp=AfxGetApp();
	CPageSetupDialog dlg;
	PAGESETUPDLG& psd = dlg.m_psd;
	BOOL bMetric = TRUE; //centimeters
	psd.Flags |= PSD_MARGINS | (bMetric ? PSD_INHUNDREDTHSOFMILLIMETERS : PSD_INTHOUSANDTHSOFINCHES);
	int nUnitsPerInch = bMetric ? 2540 : 1000;
	MulDivRect(&psd.rtMargin, m_rcPrnMargin, nUnitsPerInch, 1440);
	RoundRect(&psd.rtMargin);
	// get the current device from the app
	PRINTDLG pd;
	pd.hDevNames = NULL;
	pd.hDevMode = NULL;
	pApp->GetPrinterDeviceDefaults(&pd);
	psd.hDevNames = pd.hDevNames;
	psd.hDevMode = pd.hDevMode;
	if (dlg.DoModal() == IDOK)
	{
		RoundRect(&psd.rtMargin);
		MulDivRect(m_rcPrnMargin, &psd.rtMargin, 1440, nUnitsPerInch);
		//pApp->m_rectPageMargin = m_rcPrnMargin;
		pApp->SelectPrinter(psd.hDevNames, psd.hDevMode);
		//pApp->NotifyPrinterChanged();
		m_ptPaperSize = dlg.m_psd.ptPaperSize;
	}
}

BOOL CViewMap::OnPreparePrinting(CPrintInfo* pInfo)
{
	BOOL	bRet;

	m_rcPrn.left = m_rcPrnMargin.left;
	m_rcPrn.right = m_ptPaperSize.x - m_rcPrnMargin.right - m_rcPrnMargin.left;
	m_rcPrn.top = m_rcPrnMargin.top;
	m_rcPrn.bottom = m_ptPaperSize.y - m_rcPrnMargin.bottom - m_rcPrnMargin.top;

	bRet = DoPreparePrinting(pInfo);

	return bRet;
}

void CViewMap::OnFilePrint()
{
	CView::OnFilePrint();
}

void CViewMap::OnFilePrintPreview()
{
	CView::OnFilePrintPreview();
}

void CViewMap::OnEndPrinting(CDC* pDC, CPrintInfo* pInfo)
{
	CView::OnEndPrinting(pDC, pInfo);
	m_bScale = TRUE;
}

// end CViewProf printing
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// ****** CViewMap drawing ********************************************************************************
void CViewMap::OnUpdate(CView* /*pSender*/, LPARAM lHint, CObject* /*pHint*/)
{
	switch( (int)lHint ) {
		case UPDATE_COMPUTATION_FINISHED:
			for(int i=0; i<m_imgGrd.GetCount(); i++) {
				m_imgGrd.GetAt(i)->bImgUpdate = TRUE;
			}
			Invalidate();
			break;
		case UPDATE_MOVE_VERTEX:
			Invalidate();
			break;
		case UPDATE_NEW_MODEL: 
			SetVariables( TRUE );
			CreateBodyPen();
			Invalidate();
			break;
		case UPDATE_BODY_EDIT_DELETE:
			CreateBodyPen();
			Invalidate();
			break;
		case UPDATE_PROPERTIES_GRID_COLOR: 
			for(int i=0; i<m_imgGrd.GetCount(); i++) {
				m_imgGrd.GetAt(i)->bImgUpdate = TRUE;
			}
			Invalidate();
			break;								  
		default: 
			if( mod )
				Invalidate();
	}
}

void CViewMap::OnPrepareDC(CDC* pDC, CPrintInfo* pInfo)
{
	pDC->SetMapMode( MM_HIMETRIC );
	pDC->SetBkMode( TRANSPARENT );

	CView::OnPrepareDC(pDC, pInfo);
}

BOOL CViewMap::OnEraseBkgnd(CDC* pDC)
{
	if( !mod ) {
		return CView::OnEraseBkgnd(pDC); // default implementation
	}
	return TRUE;
	//return CView::OnEraseBkgnd(pDC); // default implementation
	//return TRUE;
}

void CViewMap::OnDraw(CDC* pDC)
{
	if( !mod ) return;

	// printing
	if( pDC->IsPrinting() ) {
		Scale(FALSE, TRUE);
		Draw(pDC);
		return;
	}

	// use the semaphore to enter this critic section
	static BOOL 	bBusy = FALSE;	
	if(bBusy) return;
	bBusy = TRUE;

	CRect clRect;
	CRect lpRect;
	GetClientRect(clRect);
	lpRect = clRect;
	pDC->DPtoLP( &lpRect );


	CBitmap bmp;
	CDC memDC;
	memDC.CreateCompatibleDC(pDC);
	if( !memDC ) {
		AfxMessageBox("Fuck! CViewMap::OnDraw(CDC* pDC)");
		bBusy = FALSE;
		return;
	}

	bmp.CreateCompatibleBitmap(pDC, clRect.right-clRect.left, clRect.bottom-clRect.top);
	CBitmap *pBmp = memDC.SelectObject(&bmp);
	CBrush bkBrush( RGB(255, 255, 255) );
	CBrush* pdBrush = memDC.SelectObject(&bkBrush);
	memDC.PatBlt(0, 0, lpRect.right, -lpRect.bottom, PATCOPY);
	OnPrepareDC(&memDC);

	Draw(&memDC);

	pDC->BitBlt(0, 0, lpRect.right, lpRect.bottom, &memDC, 0, 0, SRCCOPY  );

	memDC.SelectObject(pBmp);
	memDC.SelectObject(pdBrush);
	memDC.Detach();
	memDC.DeleteDC();
	bmp.DeleteObject();

	// turn off semaphore
	bBusy = FALSE;
}

void CViewMap::Draw(CDC *pDC)
{
	if(m_bScale) Scale();

	//default font
	CFont font, *pFont;
	font.CreatePointFontIndirect( &m_lfDefault, pDC );
	pFont = pDC->SelectObject( &font );
	// select default Hollow brush
	CBrush* pBrush = pDC->SelectObject( &m_brushHollow );

	CreateBodyPen();
	DrawObjects(pDC);
	DrawGrid(pDC);
	DrawBodies(pDC);
	DrawSelectedProfiles(pDC);
	m_axX.Draw(pDC);
	m_axY.Draw(pDC);
	m_axX.DrawGridLines(pDC, m_drwRect);
	m_axY.DrawGridLines(pDC, m_drwRect);
	DrawObservation(pDC);

	if(GetCapture() == this && m_cpt.nTag == CPTM_ZOOM && m_bZooming) {
		CRect rc = m_zoomRect;
		pDC->DPtoLP(&rc);
		CPen	pen(PS_DOT, 0, RGB(166, 166, 166)), *pPen;
		pPen = pDC->SelectObject( &pen );
		pDC->Rectangle(&rc);
		pDC->SelectObject( pPen);
	}

	pDC->SelectObject( pFont );
	pDC->SelectObject( pBrush );
}

void CViewMap::DrawObjects(CDC* pDC)
{
	if( !m_bShowObjects ) return;

	CMyObject *pObj;
	for(int i=0; i<m_pDoc->GetObjArray()->GetSize(); i++) {
		pObj = m_pDoc->GetObjArray()->GetAt( i );
		if( pObj->IsKindOf( RUNTIME_CLASS(CImgReferenced) ) ) {
			CImgReferenced* pIm = (CImgReferenced*) pObj;
			BOOL bVisible = pIm->IsVisible();
			if( bVisible ) {
				double yw, xw;
				CPoint pt;
				pIm->GetTopLeft(yw, xw);
				WPtoDP(pt, xw, yw);
				CPaintDC dc(this);
				pIm->DrawMap( pDC, m_xSc, m_xOff, m_ySc, m_yOff );
			}
		}
		if( pObj->IsKindOf( RUNTIME_CLASS(CGuideMark) ) ) {
			CGuideMark* pGm = (CGuideMark*) pObj;
			if( pGm->IsVisible() ) {
				pGm->DrawMap( pDC, m_xSc, m_xOff, m_ySc, m_yOff );
			}
		}
		if( pObj->IsKindOf( RUNTIME_CLASS(CWell) ) ) {
			CWell* pW = (CWell*) pObj;
			if( pW->IsVisible() ) {
				pW->DrawMap( pDC, m_xSc, m_xOff, m_ySc, m_yOff );
			}
		}
		if( pObj->IsKindOf( RUNTIME_CLASS(CData3D) ) ) {
			CData3D* pD = (CData3D*) pObj;
			if( pD->IsVisible() ) {
				pD->DrawMap( pDC, m_xSc, m_xOff, m_ySc, m_yOff );
			}
		}
		//if( pObj->IsKindOf( RUNTIME_CLASS(CGridObj) ) ) {
		//	CGridObj* pG = (CGridObj*) pObj;
		//	if( pG->IsVisible() ) {
		//		// add code
		//	}
		//}
	}
}

void CViewMap::DrawGrid(CDC* pDC)
{
	//TRACE("Entering CViewMap::DrawGrid(CDC* pDC)\n");

	CGrid*		pGrd;
	GRDIMGFLD*	pImg;
	int			xDest, yDest, nDestWidth, nDestHeight;
	xDest =		(int) (m_xOff + m_xSc*mod->GetX0() );
	yDest =		(int) (m_yOff + m_ySc*mod->GetY0() );
	nDestWidth =	(int) (m_xSc*mod->GetLX());
	nDestHeight =	(int) (m_ySc*mod->GetLY());

	// transform to pixels, and scale according to image quality
	CSize cs(nDestWidth, nDestHeight);
	pDC->LPtoDP( &cs );
	cs.cx = (int) (cs.cx*m_dImgQ);
	cs.cy = (int) (cs.cy*m_dImgQ);

	// image legend rect
	CRect	rcLegend;
	CSize	szLegend;
	CSize	szLegShf;	// legend shift
	if(!pDC->IsPrinting()) {
		szLegend = CSize(10, 200);
		szLegShf = CSize(60, 220);	// legend shift
		pDC->DPtoLP(&szLegend);
		pDC->DPtoLP(&szLegShf);
	}
	else
	{
		szLegend = CSize(400, 6000);
		szLegShf = CSize(60, 220);	// legend shift
	}

	// observations image
	if(m_obs.bImg && !m_pDoc->GetObservationGrid()->IsPlanar()) {
		if( m_obs.bImgUpdate || m_obs.img.IsNull() ) {
			pGrd = m_pDoc->GetObservationGrid();
			pGrd->CreateImage(m_obs.img, 1/*cs.cx*/, cs.cy );
			pGrd->CreateImageLegeng(m_obs.imgLegend);
			m_obs.bImgUpdate = FALSE;
		}
		m_obs.img.AlphaBlend(pDC->GetSafeHdc(), xDest, yDest, nDestWidth, nDestHeight, 0, 0, m_obs.img.GetWidth(), m_obs.img.GetHeight(), m_nAlphaRel );
	}

	// field images
	int n = (int) m_imgGrd.GetSize();
	for(int i=0; i<n; i++) {
		pImg = m_imgGrd.GetAt(i);
		pGrd = m_pDoc->GetGrid(pImg->nGrid);
		if(pGrd != NULL) {
			if(pImg->bImg ) {
				if( pImg->bImgUpdate || pImg->img.IsNull() ) {
					if( pGrd->CreateImage( pImg->img, cs.cx, cs.cy ) ) {
						pGrd->CreateImageLegeng(pImg->imgLegend);
						pImg->bImgUpdate = FALSE;
						pImg->img.AlphaBlend(pDC->GetSafeHdc(), xDest, yDest, nDestWidth, nDestHeight, 0, 0, pImg->img.GetWidth(), pImg->img.GetHeight(), m_nAlphaImg );
					}
					else {
						pImg->bImg = FALSE;
					}
				}
				else if ( pImg->bImg )
					pImg->img.AlphaBlend(pDC->GetSafeHdc(), xDest, yDest, nDestWidth, nDestHeight, 0, 0, pImg->img.GetWidth(), pImg->img.GetHeight(), m_nAlphaImg );
			}
		}
	}

	// observation contours
	if(m_obs.bCnt) {
		pGrd = m_pDoc->GetObservationGrid();
		if( pGrd!=NULL)
			pGrd->DrawContours(pDC, m_xSc, m_ySc, m_xOff, m_yOff);
	}

	// field contours
	for(int i=0; i<n; i++) {
		pImg = m_imgGrd.GetAt(i);;
		if(pImg->bCnt) {
		pGrd = m_pDoc->GetObservationGrid();
		if( pGrd!=NULL)
			pGrd->DrawContours(pDC, m_xSc, m_ySc, m_xOff, m_yOff);
		}
	}

	// legend
	rcLegend.left = m_drwRect.right+m_szMargin.cx;
	rcLegend.right = m_drwRect.right+m_szMargin.cx+szLegend.cx;
	rcLegend.top = m_drwRect.top-szLegend.cy;
	rcLegend.bottom = m_drwRect.top;
	CScaleAxis ax;
	double	min, max;
	int		l=0;
	CRect	rcTmp = rcLegend;
	// relief
	pImg = &m_obs;
	if((pImg->bCnt || pImg->bImg) && !m_pDoc->GetObservationGrid()->IsPlanar() ) {
		pGrd = m_pDoc->GetObservationGrid();
		ax.SetDrawRect(rcLegend.left, rcLegend.bottom, rcLegend.right, rcLegend.top);
		//pGrd->GetMinMaxValue(min, max);
		pGrd->GetHistogramMinMax(min, max);
		ax.SetMaxDataExtent(min, max);
		ax.SetDataExtent(min, max);
		ax.DrawAxisYl(pDC);
		if( pImg->imgLegend.IsNull() ) {
			pGrd->CreateImageLegeng(pImg->imgLegend);
		}
		pImg->imgLegend.AlphaBlend(pDC->GetSafeHdc(), rcLegend.left, rcLegend.top, rcLegend.Width(), rcLegend.Height(), 0, 0, pImg->imgLegend.GetWidth(), pImg->imgLegend.GetHeight(), m_nAlphaImg );
		// component text
		pDC->SetTextAlign(TA_TOP | TA_LEFT);
		pDC->TextOut(rcLegend.left, rcLegend.top, "h" );
		l++;
		if( (l % 2) == 0 )  {
			rcLegend.left += szLegShf.cx;
			rcLegend.right += szLegShf.cx;
			rcLegend.top = rcTmp.top;
			rcLegend.bottom = rcTmp.bottom;
		}
		else {
			rcLegend.top -= szLegShf.cy;
			rcLegend.bottom -= szLegShf.cy;
		}
	}
	// grids
	for(int i=0; i<n; i++) {
		GRDIMGFLD* pImg = m_imgGrd.GetAt(i);;
		pGrd = m_pDoc->GetGrid(pImg->nGrid);
		if(pGrd != NULL) {
			if(pImg->bCnt || pImg->bImg) {
				ax.SetDrawRect(rcLegend.left, rcLegend.bottom, rcLegend.right, rcLegend.top);
				pGrd->GetLegendRange(min, max);
				ax.SetMaxDataExtent(min, max);
				ax.SetDataExtent(min, max);
				ax.DrawAxisYl(pDC);
				if( pImg->imgLegend.IsNull() ) {
					pGrd->CreateImageLegeng(pImg->imgLegend);
				}
				pImg->imgLegend.AlphaBlend(pDC->GetSafeHdc(), rcLegend.left, rcLegend.top, rcLegend.Width(), rcLegend.Height(), 0, 0, pImg->imgLegend.GetWidth(), pImg->imgLegend.GetHeight(), m_nAlphaImg );
				// component text
				pDC->SetTextAlign(TA_TOP | TA_LEFT);
				pDC->TextOut(rcLegend.left, rcLegend.top, m_pDoc->GetFieldString(pImg->nGrid) );
				l++;
				if( (l % 2) == 0 )  {
					rcLegend.left += szLegShf.cx;
					rcLegend.right += szLegShf.cx;
					rcLegend.top = rcTmp.top;
					rcLegend.bottom = rcTmp.bottom;
				}
				else {
					rcLegend.top -= szLegShf.cy;
					rcLegend.bottom -= szLegShf.cy;
				}
			}
		}
	}
}

void CViewMap::DrawSelectedProfiles(CDC *pDC)
{
	//TRACE("Entering CViewMap::DrawSelectedProfiles(CDC *pDC)\n");
	if( !m_bShowProfiles )	return;

	CPoint	pt;
	CPen	*pPen;
	int		nCol, nRow;
	CString	str;	// PROFILE NUMBER
	double	xw, yw;

	CMDIFrameWnd *pFrame = (CMDIFrameWnd*)AfxGetApp()->m_pMainWnd;

	// Get the active MDI child window.
	CMDIChildWnd *pChild = (CMDIChildWnd *) pFrame->GetActiveFrame();	// or CMDIChildWnd *pChild = pFrame->MDIGetActive();
	// Get the active view attached to the active MDI child window.
	CView *pActView = pChild->GetActiveView();

	POSITION pos = m_pDoc->GetFirstViewPosition();
	UINT nTa = pDC->GetTextAlign();
	while (pos != NULL) {
		CView* pView = m_pDoc->GetNextView(pos);
		if( pView->IsKindOf(RUNTIME_CLASS( CViewProf)) ) {
			CViewProf *pProfView = (CViewProf*) pView;
			nRow = pProfView->GetRow();
			nCol = pProfView->GetCol();
			xw = mod->GetXd(nCol);
			yw = mod->GetYd(nRow);
			WPtoLP(pt, xw, yw);
			int nProf = pProfView->GetProfType(); 
			if( nProf == PRF_COL) {
				// column profile
				if(pActView == pProfView) {
					pPen = pDC->SelectObject(&m_penProfCur); }
				else {
					pPen = pDC->SelectObject(&m_penProf); }
				pDC->MoveTo( pt.x, m_drwRect.bottom);
				pDC->LineTo( pt.x, m_drwRect.top );

				str.Format("col %d", nCol);
				pDC->SetTextAlign(TA_LEFT | TA_TOP);
				pDC->TextOut( pt.x, m_drwRect.top, str );
				pDC->SelectObject(pPen);
			}
			if( nProf == PRF_ROW) {
				// row profile 
				if(pActView == pProfView) {
					pPen = pDC->SelectObject(&m_penProfCur); }
				else {
					pPen = pDC->SelectObject(&m_penProf); }
				pDC->MoveTo( m_drwRect.left, pt.y );
				pDC->LineTo( m_drwRect.right, pt.y );

				str.Format("row %d", nRow);
				pDC->SetTextAlign(TA_RIGHT | TA_TOP);
				pDC->TextOut( m_drwRect.right, pt.y, str );
				pDC->SelectObject(pPen);
			}
		}
	}
	pDC->SetTextAlign( nTa );
}

void CViewMap::DrawObservation(CDC* pDC)
{
	if( !m_bObsPoints )		return;

	CSize dis(10, 10); // minimal distance between drawed observations in device coordinates
	CSize len = m_szObs;
	int x, y;
	int iStep = 1, jStep = 1;

	// find if observation points are not too close
	pDC->DPtoLP( &dis );
	pDC->DPtoLP( &len );
	// observations are too close draw every nth point
	if(m_xSc*m_cx < dis.cx) {
		jStep = (int) ceil(dis.cx/(m_xSc*m_cx));
	}
	if(m_ySc*m_cy < dis.cy) {	
		iStep = (int) ceil(dis.cy/(m_ySc*m_cy));
	}

	CPen* pPen = pDC->SelectObject( &m_penObs );
	for(int i = 0; i < m_nRows; i += iStep) {
		for(int j = 0; j < m_nCols; j += jStep) {
			WPtoLP(x, y, mod->GetXd(j), mod->GetYd(i));
			pDC->MoveTo(x, y-len.cy);
			pDC->LineTo(x, y+len.cy);
			pDC->MoveTo(x-len.cx, y);
			pDC->LineTo(x+len.cx, y);
		}
	}
	pDC->SelectObject( pPen );
}

void CViewMap::CreateBodyPen(void)
{
	int n = (int)mod->GetBodies()->GetSize();
	CBody* pBd;
	m_bodyPen.RemoveAll();	
	m_bodyPen.SetSize(n, 0);
	for(int i=0; i<n; i++) {
		pBd = mod->GetBodies()->GetAt(i);
		pBd->SetIndex(i);
		m_bodyPen[i].CreatePenIndirect( pBd->GetPLogPen() );
	}
}

void CViewMap::DrawBodies(CDC* pDC)
{
	if( !m_bBodies )	return;

	BDLINArray*	pAr;
	BDLIN*		pB;
	CPoint		pt;
	CPen*		pPen;

	for(int i=0; i<m_nRows-1; i++) {
		for(int j=0; j<m_nCols-1; j++) {
			pAr = mod->GetBodyBoundAt(i, j);
			for( int k=0; k < pAr->GetSize(); k +=2) {
				pB = &( pAr->GetAt(k) );
				pPen = pDC->SelectObject( &m_bodyPen.GetAt(pB->n) );
				WPtoLP( pt, pB->pt.x, pB->pt.y );
				pDC->MoveTo( pt );
				pB = &( pAr->GetAt(k+1) );
				WPtoLP( pt, pB->pt.x, pB->pt.y );
				pDC->LineTo( pt );
				pDC->SelectObject(pPen);
			}
		}
	}
}

void CViewMap::SetVariables(bool bInit)
{
	m_pDoc = GetDocument();
	mod = m_pDoc->GetModel();
	if( mod ) {
		m_nRows = mod->GetRows();
		m_nCols = mod->GetCols();
		m_cx = mod->GetSizeX();
		m_cy = mod->GetSizeY();
		Scale(TRUE);
	}
	// window title
	GetParent()->SetWindowText(GetDocument()->GetTitle()+ ": Map");
}

int CViewMap::GetMargin(CDC* pDC, CSize& szMargin, BOOL bInit)
{
	double min, max;
	if( bInit ) {
		min = mod->GetXd(1);
		max = mod->GetXd(m_nCols-2);
		m_axX.SetMaxDataExtent(min, max);
		m_axX.SetDataExtent(min, max);
	}
	if( bInit ) {
		min = mod->GetYd(1);
		max = mod->GetYd(m_nRows-2);
		m_axY.SetMaxDataExtent(min, max);
		m_axY.SetDataExtent(min, max);
	}

	szMargin.cy = m_axX.GetWidth(pDC);
	szMargin.cx = m_axY.GetWidth(pDC);

	return 0;
}

void CViewMap::Scale(BOOL bInit, BOOL bPrint)
{
	CRect	clRc;
	CSize	szCR;
	CSize	szLT;
	CSize	szRB;
	CSize	szAxWidth;
	CSize	szDiv;
	double	lx, ly;				// axis length
	int x, y;
	double	min, max;

	m_szMargin = CSize(6, 6);
	szLT = m_szLeftTop;
	szRB = m_szRightBot;

	GetClientRect( clRc );
	if(clRc.right < 50 || clRc.bottom < 50)		return;

	if( m_szDiv.cx > clRc.right/2)
		m_szDiv.cx = clRc.right/2;
	szDiv = m_szDiv;

	CClientDC dc(this);
	OnPrepareDC(&dc);
	if( !bPrint ) {
		szCR.cx = clRc.right;
		szCR.cy = clRc.bottom;
		dc.DPtoLP( &szCR );
		dc.DPtoLP( &m_szMargin );
		dc.DPtoLP( &szDiv );
		dc.DPtoLP( &szLT );
		dc.DPtoLP( &szRB );
	}
	// printing
	else {
		szCR.cx = m_ptPaperSize.x;
		szCR.cy = m_ptPaperSize.y;
		szLT = CSize(m_rcPrnMargin.left, m_rcPrnMargin.top);
		szRB = CSize(m_rcPrnMargin.right, m_rcPrnMargin.bottom);
		m_szMargin = CSize(m_rcPrnMargin.left, m_rcPrnMargin.bottom);
	}

	GetMargin(&dc, szAxWidth, bInit);

	m_drwRect.left	= szAxWidth.cx + m_szMargin.cx;
	m_drwRect.top	= -szLT.cy;
	m_drwRect.right = szCR.cx - szRB.cx;
	m_drwRect.bottom	= -szCR.cy + szAxWidth.cy + m_szMargin.cy;
	//m_drwRect.left	= szLT.cx;
	//m_drwRect.top	= -szLT.cy;
	//m_drwRect.right = szCR.cx - szRB.cx;
	//m_drwRect.bottom	= -szCR.cy + szRB.cy;

	lx = mod->GetLXd();	
	ly = mod->GetLYd();	
	m_x0 = mod->GetXd(0);
	m_y0 = mod->GetYd(0);

	m_xSc = (m_drwRect.right-m_drwRect.left) / (lx);
	m_ySc = (m_drwRect.top-m_drwRect.bottom) / (ly );
	if( m_bScaleEq ) {
		double sc = min(m_xSc, m_ySc);
		m_xSc = sc;
		m_ySc = sc;
		m_drwRect.right = m_drwRect.left+(int) (sc*lx);
		m_drwRect.bottom = m_drwRect.top-(int) (sc*ly);
	}
	m_xOff = m_drwRect.left - m_xSc*m_x0;
	m_yOff = m_drwRect.bottom - m_ySc*m_y0;

	int		xl, xr, yt, yb;
	CRect	clRect;

	min = mod->GetXd(1);
	max = mod->GetXd(m_nCols-2);
	WPtoLP(xl, y, min, 0); 
	WPtoLP(xr, y, max, 0); 
	//m_axX.SetDrawRect( xl, -szCR.cy + m_szMargin.cy, xr, -szCR.cy);
	m_axX.SetDrawRect( xl, m_drwRect.bottom - szAxWidth.cy , xr , m_drwRect.bottom- szAxWidth.cy- m_szMargin.cy);
	if( bInit ) {
		m_axX.SetMaxDataExtent(min, max);
		m_axX.SetDataExtent(min, max);
	}
	else {
		m_axX.GetDataExtent(min, max);
		m_axX.SetDataExtent(min, max);
	}

	min = mod->GetYd(1);
	max = mod->GetYd(m_nRows-2);
	WPtoLP(x, yt, 0, max); 
	WPtoLP(x, yb, 0, min); 
	m_axY.SetDrawRect( 0, yt, m_szMargin.cx, yb);
	if( bInit ) {
		m_axY.SetMaxDataExtent(min, max);
		m_axY.SetDataExtent(min, max);
	}
	else {
		m_axY.GetDataExtent(min, max);
		m_axY.SetDataExtent(min, max);
	}

	m_axX.GetScOff(m_xSc, m_xOff);
	m_axY.GetScOff(m_ySc, m_yOff);
	m_xOff = xl - m_xSc*m_axX.GetDataMin();
	m_yOff = yb - m_ySc*m_axY.GetDataMin();

	m_bScale = FALSE;
}


void CViewMap::OnSize(UINT nType, int cx, int cy)
{
	CView::OnSize(nType, cx, cy);

	if(mod) {
		Scale();
	}
}




/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ******** mouse routines **************************************************************************************

void CViewMap::OnMouseMove(UINT nFlags, CPoint point)
{
	if( !mod ) return;

	CPoint lPt=point;
	CClientDC dc(this);
	OnPrepareDC(&dc);
	dc.DPtoLP(&lPt);

	FindIndex(point);

	if( IsCursorOverDiv( point ) )	return;

	if( GetCapture() == this ) {
		MouseMoveCaptured(nFlags, point);
		return;
	}

	// scaling
	if( IsCursorOverScaleBar(lPt) )		return;
	AutomaticPan(point);

	if( IsCursorOverObject(point) )		return;

	// default
	m_nCursor = CRSM_NORMAL;
	CView::OnMouseMove(nFlags, point);
}

void CViewMap::MouseMoveCaptured(UINT nFlags, CPoint point)
{
	CPoint	lPt=point;
	CRect	clRc;
	int		div;

	// scale bars
	GetClientRect(&clRc);
	DPtoLP(lPt);
	switch( m_cpt.nTag ) {
		case CPTM_MOVE_SB_HRZ:
            m_axX.Move(lPt);
			m_bScaleEq = FALSE;
			Scale();
			Invalidate();
			return;
		case CPTM_MOVE_SB_VRT:
			m_axY.Move(lPt);
			m_bScaleEq = FALSE;
			Scale();
			Invalidate();
			return;
		case CPTM_ZOOM:
			if( m_bZooming ) {
				m_zoomRect.right = point.x;
				m_zoomRect.bottom = point.y;
				Invalidate();
			}
			return;
		case CPTM_MOVE_DIV_RIGHT:
			div = clRc.right-point.x;
			if( div > 10 && point.x > m_szLeftTop.cx) {
				m_szRightBot.cx = div;
				Scale();
				Invalidate();
			}
			return;
		case CPTM_MOVE_DIV_BOTTOM:
			div = clRc.bottom-point.y;
			if( div > 10 && point.y > m_szLeftTop.cy) {
				m_szRightBot.cy = div;
				Scale();
				Invalidate();
			}
			return;
		case CPTM_MOVE_DIV_LEFT:
			div = point.x;
			if( div > 10 && point.x < clRc.right-m_szRightBot.cx ) {
				m_szLeftTop.cx = div;
				Scale();
				Invalidate();
			}
			return;
		case CPTM_MOVE_DIV_TOP:
			div = point.y;
			if( div > 10 && point.y < clRc.bottom-m_szRightBot.cy) {
				m_szLeftTop.cy = div;
				Scale();
				Invalidate();
			}
			return;
	}
}

void CViewMap::OnLButtonDown(UINT nFlags, CPoint point)
{
	// fill structure
	//m_cpt;

	// zooming
	if( GetCapture() == this ) {
		switch(m_cpt.nTag) {
			case CPTM_ZOOM_IN:
				ZoomIn(point);
				return;
			case CPTM_ZOOM_OUT:
				ZoomOut(point);
				return;
			case CPTM_ZOOM:
				if( !m_bZooming ) {
					m_zoomRect.top = point.y; 
					m_zoomRect.left = point.x; 
					m_bZooming = TRUE;
				}
				return;
		}
	}

	switch( m_nCursor ) {
		case CRSM_OVER_SCBAR_H:
			m_cpt.nTag = CPTM_MOVE_SB_HRZ;
			SetCapture();
			return;
		case CRSM_OVER_SCBAR_V:
			m_cpt.nTag = CPTM_MOVE_SB_VRT;
			SetCapture();
			return;
		case CRSM_OVER_DIV_RIGHT:
			m_cpt.nTag = CPTM_MOVE_DIV_RIGHT;
			SetCapture();
			return;
		case CRSM_OVER_DIV_LEFT:
			m_cpt.nTag = CPTM_MOVE_DIV_LEFT;
			SetCapture();
			return;
		case CRSM_OVER_DIV_TOP:
			m_cpt.nTag = CPTM_MOVE_DIV_TOP;
			SetCapture();
			return;
		case CRSM_OVER_DIV_BOTTOM:
			m_cpt.nTag = CPTM_MOVE_DIV_BOTTOM;
			SetCapture();
			return;
	}
	CView::OnLButtonDown(nFlags, point);
}

void CViewMap::OnLButtonUp(UINT nFlags, CPoint point)
{
	if( GetCapture() == this ) {
		if( m_bZooming )
			Zoom(point);
		m_cpt.nTag = CPTM_NONE;
		ReleaseCapture();
		Invalidate();
	}

	CView::OnLButtonUp(nFlags, point);
}

void CViewMap::OnRButtonDown(UINT nFlags, CPoint point)
{
	BOOL	bInvalidate = FALSE;;
	if( (m_nCursor == CRSM_OVER_OBJECT) && (nFlags == (MK_RBUTTON | MK_CONTROL)) ) {
		CMyObject* pObj = m_pDoc->GetObject(m_nObject);
		if(pObj)	{ 
			pObj->Properties();	
			Invalidate();
		}
		return;
	}

	switch( m_nCursor ) {
		case CRSM_OVER_SCBAR_H:
			bInvalidate = m_axX.Properties();
			break;
		case CRSM_OVER_SCBAR_V:
			bInvalidate = m_axY.Properties();
			break;
		default :
			Properties();
			break;
	}

	if(bInvalidate) {
		Scale();
		Invalidate();
	}

	CView::OnRButtonDown(nFlags, point);
}

int CViewMap::IsCursorOverDiv(CPoint pt)
{
	// legend division
	CRect	clRc;
	GetClientRect(&clRc);

	int	div = clRc.right - m_szRightBot.cx;
	if(  div-3 < pt.x && pt.x < div+3 ) {
		m_nCursor = CRSM_OVER_DIV_RIGHT;
		return 1;
	}
	div = clRc.bottom - m_szRightBot.cy;
	if(  div-3 < pt.y && pt.y < div+3 ) {
		m_nCursor = CRSM_OVER_DIV_BOTTOM;
		return 1;
	}
	div = m_szLeftTop.cx;
	if(  div-3 < pt.x && pt.x < div+3 ) {
		m_nCursor = CRSM_OVER_DIV_LEFT;
		return 1;
	}
	div = m_szLeftTop.cy;
	if(  div-3 < pt.y && pt.y < div+3 ) {
		m_nCursor = CRSM_OVER_DIV_TOP;
		return 1;
	}
	return 0;
}

int CViewMap::IsCursorOverScaleBar(CPoint &pt)
{
	if( m_axX.IsClicked(pt) )	{
		m_nCursor = CRSM_OVER_SCBAR_H;
		return CRSM_OVER_SCBAR_H;
	}
	if( m_axY.IsClicked(pt) )	{
		m_nCursor = CRSM_OVER_SCBAR_V;
		return CRSM_OVER_SCBAR_V;
	}
	return FALSE;
}

int CViewMap::IsCursorOverObject(CPoint &pt)
{
	int	n;
	CMyObject	*pObj;

	n = m_pDoc->GetObjArray()->GetSize();
	for(int i=0; i<n; i++) {
		pObj = m_pDoc->GetObjArray()->GetAt( i );
		if( pObj->IsKindOf( RUNTIME_CLASS( CImgReferenced ) ) ) {
			CImgReferenced* pIm = (CImgReferenced*) pObj;
			double t,l, b, r;
			pIm->GetTopLeft(t, l);
			pIm->GetBotRight(b, r);
			CPoint lt, rb;
			WPtoDP(lt, l, t);
			WPtoDP(rb, r, b);
			if( (lt.x < pt.x && pt.x < rb.x)  &&  (lt.y < pt.y && pt.y < rb.y) ) {
				m_nCursor = CRSM_OVER_OBJECT;
				m_nObject = i;
				return 1;
			}
		}
	}
	m_nObject = -1;
	return FALSE;
}

BOOL CViewMap::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	HCURSOR hCursor;

	switch( m_nCursor ) {
		case CRSM_OVER_SCBAR_H:
			hCursor = AfxGetApp()->LoadCursor(IDC_CURSOR_SB_HRZ);
			break;
		case CRSM_OVER_SCBAR_V:
			hCursor = AfxGetApp()->LoadCursor(IDC_CURSOR_SB_VRT);
			break;
		case CRSM_OVER_OBJECT:
			hCursor = AfxGetApp()->LoadCursor(IDC_CURSOR_OBJECT);
			break;
		case CRSM_OVER_DIV_RIGHT:
			hCursor = AfxGetApp()->LoadCursor(IDC_CURSOR_DIV_VRT);
			break;
		case CRSM_OVER_DIV_LEFT:
			hCursor = AfxGetApp()->LoadCursor(IDC_CURSOR_DIV_VRT);
			break;
		case CRSM_OVER_DIV_TOP:
			hCursor = AfxGetApp()->LoadCursor(IDC_CURSOR_DIV);
			break;
		case CRSM_OVER_DIV_BOTTOM:
			hCursor = AfxGetApp()->LoadCursor(IDC_CURSOR_DIV);
			break;
		default:
			hCursor = AfxGetApp()->LoadCursor(IDC_CURSOR_NORMAL);
			break;
	}
	::SetCursor(hCursor);

	return 1;

	//return CView::OnSetCursor(pWnd, nHitTest, message);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//************** zooming *************************************************************************************
void CViewMap::OnViewEqualScale()
{
	m_bScaleEq = !m_bScaleEq;
	Scale();
	Invalidate(FALSE);
}

void CViewMap::OnUpdateViewEqualScale(CCmdUI *pCmdUI)
{
	if(m_bScaleEq)	pCmdUI->SetCheck(1);
	else			pCmdUI->SetCheck(0);
}

void CViewMap::OnViewZoom()
{
	SetCapture();
	::SetCursor(AfxGetApp()->LoadCursor(IDC_CURSOR_ZOOM));
	m_cpt.nTag = CPTM_ZOOM;
	m_bZooming = FALSE;
}

void CViewMap::OnViewZoomIn()
{
	SetCapture();
	::SetCursor(AfxGetApp()->LoadCursor(IDC_CURSOR_ZOOM_IN));
	m_cpt.nTag = CPTM_ZOOM_IN;
}

void CViewMap::OnViewZoomOut()
{
	SetCapture();
	::SetCursor(AfxGetApp()->LoadCursor(IDC_CURSOR_ZOOM_OUT));
	m_cpt.nTag = CPTM_ZOOM_OUT;
}

void CViewMap::OnViewZoomPage()
{
	m_axX.ZoomToMax();
	m_axY.ZoomToMax();
	Scale();
	Invalidate();
}

void CViewMap::ZoomIn(CPoint pt)
{
	double	xw, yw;

	// transform to map coordinates
	DPtoWP(pt, xw, yw);
	m_axX.ZoomAxis(xw, 0.8);
	m_axY.ZoomAxis(yw, 0.8);

	m_bScale = TRUE;
	m_cpt.nTag = CPT_NONE;
	ReleaseCapture();
	Scale();
	Invalidate();
}

void CViewMap::ZoomOut(CPoint pt)
{
	double	xw, yw;

	// transform to map coordinates
	DPtoWP(pt, xw, yw);
	m_axX.ZoomAxis(xw, 1.2);
	m_axY.ZoomAxis(yw, 1.2);

	m_bScale = TRUE;
	m_cpt.nTag = CPT_NONE;
	ReleaseCapture();
	Scale();
	Invalidate();
}

void CViewMap::Zoom(CPoint pt)
{
	m_bZooming = FALSE;

	double	l, r, t, b, temp;
	double min, max;

	DPtoWP(pt, r, b);
	pt = m_zoomRect.TopLeft();
	DPtoWP(pt, l, t);

	if( l==r || t==b) 		return;

	if(l>r)		{	temp = l; l = r; r = temp;	}
	m_axX.GetDataExtent(min, max);
	if(l<max && r>min) {
		m_axX.SetDataExtent(l, r);
	}

	if(b>t)		{	temp = b; b = t; t = temp;	}
	m_axY.GetDataExtent(min, max);
	if(b<max && t>min) {
		m_axY.SetDataExtent(b, t);
	}

	m_bScale = TRUE;
	m_cpt.nTag = CPT_NONE;
	m_bScaleEq = FALSE;
	Invalidate();
}

int CViewMap::AutomaticPan(CPoint pt)
{
	CRect	clRect;
	GetClientRect( &clRect );
	BOOL	bInvalidate=FALSE;

	if( pt.x < 10 ) {
		m_axX.Move(AMT_MINUS);
		bInvalidate=TRUE;
	}
	if( pt.y < 10 ) {
		m_axY.Move(AMT_PLUS);
		bInvalidate=TRUE;
	}
	if( pt.x > clRect.right - 10 ) {
		m_axX.Move(AMT_PLUS);
		bInvalidate=TRUE;
	}
	if( pt.y > clRect.bottom - 10 ) {
		m_axY.Move(AMT_MINUS);
		bInvalidate=TRUE;
	}

	if( bInvalidate ) {
		Scale();
		Invalidate();
	}
	return 0;
}

//************** end zooming *********************************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//************** field toolbar *******************************************************************************
void CViewMap::OnViewFieldContours()						{	m_nFldDisp = FTB_CONTOUR;	}
void CViewMap::OnUpdateViewFieldContours(CCmdUI *pCmdUI)
{
	if( m_nFldDisp == FTB_CONTOUR )							{ pCmdUI->SetCheck(1);			}
	else													{ pCmdUI->SetCheck(0);			}
}

void CViewMap::OnViewFieldImage()							{	m_nFldDisp = FTB_IMAGE;		}
void CViewMap::OnUpdateViewFieldImage(CCmdUI *pCmdUI)
{
	if( m_nFldDisp == FTB_IMAGE )							{ pCmdUI->SetCheck(1);			}
	else													{ pCmdUI->SetCheck(0);			}
}

void CViewMap::OnViewMeasuredField()	{	m_nFldType = FLDTLB_MEASURED;	}
void CViewMap::OnUpdateViewMeasuredField(CCmdUI *pCmdUI)
{
	if( m_nFldType == FLDTLB_MEASURED )	pCmdUI->SetCheck(1);
	else								pCmdUI->SetCheck(0);
}

void CViewMap::OnViewModeledField()	{	m_nFldType = FLDTLB_MODELED;		}
void CViewMap::OnUpdateViewModeledField(CCmdUI *pCmdUI)
{
	if( m_nFldType == FLDTLB_MODELED )	pCmdUI->SetCheck(1);
	else								pCmdUI->SetCheck(0);
}

void CViewMap::OnViewDifferenceField(){	m_nFldType = FLDTLB_DIFFERENCE;	}
void CViewMap::OnUpdateViewDifferenceField(CCmdUI *pCmdUI)
{
	if( m_nFldType == FLDTLB_DIFFERENCE )	pCmdUI->SetCheck(1);
	else									pCmdUI->SetCheck(0);
}

// gravity
void CViewMap::OnViewFieldModelGx()						{	SetField(GRDMOD_GX);}
void CViewMap::OnUpdateViewFieldModelGx(CCmdUI* pCmdUI)	{	UpdateViewField(pCmdUI, GRDMOD_GX);}

void CViewMap::OnViewFieldModelGy()						{	SetField(GRDMOD_GY);}
void CViewMap::OnUpdateViewFieldModelGy(CCmdUI* pCmdUI)	{	UpdateViewField(pCmdUI, GRDMOD_GY);}

void CViewMap::OnViewFieldModelGz()						{	SetField(GRDMOD_GZ);}
void CViewMap::OnUpdateViewFieldModelGz(CCmdUI* pCmdUI)	{	UpdateViewField(pCmdUI, GRDMOD_GZ);}

void CViewMap::OnViewFieldModelG()						{	SetField(GRDMOD_G);}
void CViewMap::OnUpdateViewFieldModelG(CCmdUI* pCmdUI)	{	UpdateViewField(pCmdUI, GRDMOD_G);}

// magnetic
void CViewMap::OnViewFieldModelMx()						{	SetField(GRDMOD_MX);}
void CViewMap::OnUpdateViewFieldModelMx(CCmdUI* pCmdUI)	{	UpdateViewField(pCmdUI, GRDMOD_MX);}

void CViewMap::OnViewFieldModelMy()						{	SetField(GRDMOD_MY);}
void CViewMap::OnUpdateViewFieldModelMy(CCmdUI* pCmdUI)	{	UpdateViewField(pCmdUI, GRDMOD_MY);}

void CViewMap::OnViewFieldModelMz()						{	SetField(GRDMOD_MZ);}
void CViewMap::OnUpdateViewFieldModelMz(CCmdUI* pCmdUI)	{	UpdateViewField(pCmdUI, GRDMOD_MZ);}

void CViewMap::OnViewFieldModelM()						{	SetField(GRDMOD_M);}
void CViewMap::OnUpdateViewFieldModelM(CCmdUI* pCmdUI)	{	UpdateViewField(pCmdUI, GRDMOD_M);}

//// gradients
void CViewMap::OnModelTxx()								{	SetField(GRDMOD_GXX);}
void CViewMap::OnUpdateModelTxx(CCmdUI *pCmdUI)			{	UpdateViewField(pCmdUI, GRDMOD_GXX);}

void CViewMap::OnModelTxy()								{	SetField(GRDMOD_GXY);}
void CViewMap::OnUpdateModelTxy(CCmdUI *pCmdUI)			{	UpdateViewField(pCmdUI, GRDMOD_GXY);}

void CViewMap::OnModelTxz()								{	SetField(GRDMOD_GXZ);}
void CViewMap::OnUpdateModelTxz(CCmdUI *pCmdUI)			{	UpdateViewField(pCmdUI, GRDMOD_GXZ);}

void CViewMap::OnModelTyy()								{	SetField(GRDMOD_GYY);}
void CViewMap::OnUpdateModelTyy(CCmdUI *pCmdUI)			{	UpdateViewField(pCmdUI, GRDMOD_GYY);}

void CViewMap::OnModelTyz()								{	SetField(GRDMOD_GYZ);}
void CViewMap::OnUpdateModelTyz(CCmdUI *pCmdUI)			{	UpdateViewField(pCmdUI, GRDMOD_GYZ);}

void CViewMap::OnModelTzz()								{	SetField(GRDMOD_GZZ);}
void CViewMap::OnUpdateModelTzz(CCmdUI *pCmdUI)			{	UpdateViewField(pCmdUI, GRDMOD_GZZ);}

void CViewMap::FindFieldIndex(int &nFld) 
{
	switch( m_nFldType ) {
		case FLDTLB_MODELED:				return;
		case FLDTLB_DIFFERENCE: nFld += 20;	return;
		case FLDTLB_MEASURED:	nFld += 40;	return;
	}
}

void CViewMap::SetField(int nFld)
{
	FindFieldIndex( nFld );

	CGrid*	pGrd = m_pDoc->GetGrid(nFld);
	if( pGrd == NULL) return;
	if( pGrd->IsPlanar()  ) {
		AfxMessageBox("The grid is planar. Cannot be selected.", MB_OK | MB_ICONINFORMATION);
		return;
	}

	GRDIMGFLD* pImg;
	pImg = GetImgGrd(nFld);
	if( pImg ) {
		if( m_nFldDisp == FTB_CONTOUR)	{
			pImg->bCnt = !pImg->bCnt;
			if( pImg->bCnt )	pImg->bCntUpdate = TRUE;	
		}
		if( m_nFldDisp == FTB_IMAGE)	{
			pImg->bImg = !pImg->bImg;
			if( pImg->bImg)		pImg->bImgUpdate = TRUE;
		}
		if( !pImg->bCnt && !pImg->bImg ) {
			RemoveImgGrd(nFld);
			Invalidate(FALSE);
			return;
		}
	}
	else {
		// add new
		GRDIMGFLD* pIg = new GRDIMGFLD;
		pIg->nGrid = nFld;
		pIg->bImg = FALSE;
		pIg->bImgUpdate = TRUE;
		pIg->bCnt = FALSE;
		pIg->bCntUpdate = TRUE;
		if( m_nFldDisp == FTB_CONTOUR)	pIg->bCnt = TRUE;
		if( m_nFldDisp == FTB_IMAGE)	pIg->bImg = TRUE;
		m_imgGrd.Add(pIg);
	}

	Invalidate(FALSE);
}

void CViewMap::UpdateViewField(CCmdUI* pCmdUI, int nFld)
{
	FindFieldIndex( nFld );
	
	CGrid*	pGrd = m_pDoc->GetGrid(nFld);
	if( pGrd == NULL) {
		pCmdUI->SetCheck(0);
		pCmdUI->Enable(0);
		return;
	}
	if( pGrd->IsEmpty() || pGrd->IsPlanar() ) {
		pCmdUI->SetCheck(0);
		pCmdUI->Enable(0);
		return;
	}

	pCmdUI->Enable(1);
	GRDIMGFLD* pImg = GetImgGrd(nFld);
	if( !pImg ) {
		pCmdUI->SetCheck(0);
		return;
	}

	if( m_nFldDisp == FTB_IMAGE )		{
		if(pImg->bImg)	pCmdUI->SetCheck(1); 
		else			pCmdUI->SetCheck(0); 
		return;
	}
	if( m_nFldDisp == FTB_CONTOUR )	{	
		if(pImg->bCnt)	pCmdUI->SetCheck(1); 	
		else			pCmdUI->SetCheck(0); 
		return;
	}

}

GRDIMGFLD* CViewMap::GetImgGrd(int nFld)
{
	int n = (int) m_imgGrd.GetSize();
	for(int i=0; i<n; i++) {
		GRDIMGFLD* pImg = m_imgGrd.GetAt(i);
		if( nFld == pImg->nGrid ) {
			return pImg;
		}
	}
	return NULL;
}

void CViewMap::RemoveImgGrd(int nFld)
{
	int n = (int) m_imgGrd.GetSize();
	for(int i=0; i<n; i++) {
		GRDIMGFLD* pImg = m_imgGrd.GetAt(i);;
		if( nFld == pImg->nGrid ) {
			m_imgGrd.RemoveAt(i);
			delete pImg;
			return;
		}
	}
}
//************** end field toolbar ***************************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////


int CViewMap::FindIndex(CPoint pt)
{
	TRACE("Entering CMod3DView::FindIndex(CPoint pt)\n");

	//find current clicked cell, lower left corner identify the cell
	if( !m_pDoc->IsEmpty() ) {
		double	dCol, dRow;
		double	xw, yw;
		DWORD	row, col;

		DPtoWP(pt, xw, yw);
		dCol = ( xw + m_cx/2 ) / m_cx;
		dRow = ( yw + m_cy/2 ) / m_cy;
		row = (DWORD) floor(dRow);
		col = (DWORD) floor(dCol);
		g_x = xw;
		g_y = yw;
		m_pDoc->SetIndicators( row, col, TRUE );
		return TRUE;
	}
	return FALSE;
}

void CViewMap::OnViewShowObjects()
{
	m_bShowObjects = !m_bShowObjects;
	Invalidate(FALSE);
}

void CViewMap::OnUpdateViewShowObjects(CCmdUI *pCmdUI)
{
	if( m_pDoc->GetObjArray()->IsEmpty() )	{
		pCmdUI->Enable(FALSE);
		return;
	}
	if( m_bShowObjects )	pCmdUI->SetCheck(1);
	else					pCmdUI->SetCheck(0);
}

void CViewMap::OnViewObservationContours()
{
	m_obs.bCnt = !m_obs.bCnt;
	Invalidate(FALSE);
}

void CViewMap::OnUpdateViewObservationContours(CCmdUI *pCmdUI)
{
	CGrid *pGrd = m_pDoc->GetObservationGrid();
	if( !pGrd ) 
	{	pCmdUI->Enable(FALSE);		return;}
	if( pGrd->IsEmpty() || pGrd->IsPlanar() )	{
		pCmdUI->Enable(FALSE);		return;	}

	if( m_obs.bCnt )	pCmdUI->SetCheck(1);
	else							pCmdUI->SetCheck(0);
}

void CViewMap::OnViewObservationImage()
{
	m_obs.bImg = !m_obs.bImg;
	Invalidate(FALSE);
}

void CViewMap::OnUpdateViewObservationImage(CCmdUI *pCmdUI)
{
	CGrid *pGrd = m_pDoc->GetObservationGrid();
	if( !pGrd ) 
	{	pCmdUI->Enable(FALSE);		return;}
	if( pGrd->IsEmpty() || pGrd->IsPlanar() )	{
		pCmdUI->Enable(FALSE);		return;	}

	if( m_obs.bImg )	pCmdUI->SetCheck(1);
	else							pCmdUI->SetCheck(0);
}



void CViewMap::OnViewObservationPoints()
{
	m_bObsPoints = !m_bObsPoints;
	Invalidate(FALSE);
}

void CViewMap::OnUpdateViewObservationPoints(CCmdUI *pCmdUI)
{
	if(m_bObsPoints)	pCmdUI->SetCheck(1);
	else				pCmdUI->SetCheck(0);
}

void CViewMap::OnViewBodyBoundaries()
{
	m_bBodies = !m_bBodies;
	Invalidate(FALSE);
}

void CViewMap::OnUpdateViewBodyBoundaries(CCmdUI *pCmdUI)
{
	if(m_bBodies)		pCmdUI->SetCheck(1);
	else				pCmdUI->SetCheck(0);
}



void CViewMap::OnViewProfiles()
{
	m_bShowProfiles = !m_bShowProfiles;
	Invalidate(FALSE);
}

void CViewMap::OnUpdateViewProfiles(CCmdUI *pCmdUI)
{
	if( m_bShowProfiles )	pCmdUI->SetCheck(1);
	else					pCmdUI->SetCheck(0);
}

void CViewMap::Properties(void)
{
	CDlgViewMapProperties dlg;

	dlg.m_bBodies = m_bBodies;
	dlg.m_bObsPoints = m_bObsPoints;
	dlg.m_bScaleEq = m_bScaleEq;
	dlg.m_bShowObjects = m_bShowObjects;
	dlg.m_bShowProfiles = m_bShowProfiles;
	dlg.m_nImgQual = (int) (m_dImgQ*100);
	dlg.m_bObsImg = m_obs.bImg;
	dlg.m_bObsCnt = m_obs.bCnt;
	dlg.m_bGrdHrz = m_axY.m_bGLMajor;
	dlg.m_bGrdVrt = m_axX.m_bGLMajor;
	dlg.m_fAlphaImg = m_nAlphaImg/255.0f;
	dlg.m_fAlphaRel = m_nAlphaRel/255.0f;
	
	if( dlg.DoModal() == IDOK ) {
		m_bBodies = dlg.m_bBodies;
		m_bObsPoints = dlg.m_bObsPoints;
		m_bScaleEq = dlg.m_bScaleEq;
		m_bShowObjects = dlg.m_bShowObjects;
		m_bShowProfiles = dlg.m_bShowProfiles;
		m_dImgQ = (float)dlg.m_nImgQual/100.0f;
		m_obs.bImg = dlg.m_bObsImg;
		m_obs.bCnt = dlg.m_bObsCnt;
		m_axY.m_bGLMajor = dlg.m_bGrdHrz;
		m_axY.m_bGLMinor = dlg.m_bGrdHrz;
		m_axX.m_bGLMajor = dlg.m_bGrdVrt;
		m_axX.m_bGLMinor = dlg.m_bGrdVrt;
		m_nAlphaImg = (int)(dlg.m_fAlphaImg*255);
		m_nAlphaRel = (int)(dlg.m_fAlphaRel*255);

		Invalidate(FALSE);
	}
}



