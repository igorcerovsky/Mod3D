// 3dDefView.cpp : implementation of the CViewProfPlus class
//

#include "stdafx.h"
#include "Mod3D.h"

#include <math.h>
#include "Mod3DDoc.h"
#include "ViewProf+.h"
#include "GridObj.h"
#include "well.h"

#include "DlgFldSelect.h"
#include "DlgProfileSettings.h"
#include "DlgBodyMove.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


extern double g_x, g_y, g_z;
// CViewProfPlus

IMPLEMENT_DYNCREATE(CViewProfPlus, CViewProf)

BEGIN_MESSAGE_MAP(CViewProfPlus, CViewProf)
	// Standard printing commands
	ON_COMMAND(ID_FILE_PRINT, CView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_DIRECT, CView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_PREVIEW, CView::OnFilePrintPreview)
	ON_WM_SETCURSOR()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_RBUTTONDOWN()
	ON_WM_MOUSEWHEEL()
	ON_WM_KEYDOWN()
	ON_WM_SIZE()
	
	// floating menu
	ON_COMMAND(ID_BODY_ACTIVE, OnBodyActive)
	ON_COMMAND(ID_BODY_COPY_TO_NEXT_PROFILE, OnBodyCopyToNextProfile)
	ON_COMMAND(ID_BODY_COPY_TO_PREVIOUS_PROFILE, OnBodyCopyToPreviousProfile)
	ON_COMMAND(ID_BODY_EDIT, OnBodyEdit)
	ON_COMMAND(ID_BODY_FILL, OnBodyFill)
	ON_COMMAND(ID_BODY_INSERT_EXISTING, OnBodyInsertExisting)
	ON_COMMAND(ID_BODY_INSERT_NEW, OnBodyInsertNew)
	ON_COMMAND(ID_BODY_LOCK, OnBodyLock)
	ON_COMMAND(ID_BODY_PROPERTIES, OnBodyProperties)
	ON_COMMAND(ID_BODY_REMOVE, OnBodyRemove)
	ON_COMMAND(ID_BODY_REMOVE_FROM_PROFILE, OnBodyRemoveFromProfile)
	ON_COMMAND(ID_BODY_SHOW, OnBodyShow)
	ON_COMMAND(ID_MODEL_BODY_CREATION_PROPERTIES, OnModelBodyCreationProperties)

	// field toolbar
	ON_COMMAND(ID_VIEW_FIELD_MODEL_G, OnViewFieldModelG)
	ON_UPDATE_COMMAND_UI(ID_VIEW_FIELD_MODEL_G, OnUpdateViewFieldModelG)
	ON_COMMAND(ID_VIEW_FIELD_MODEL_GX, OnViewFieldModelGx)
	ON_UPDATE_COMMAND_UI(ID_VIEW_FIELD_MODEL_GX, OnUpdateViewFieldModelGx)
	ON_COMMAND(ID_VIEW_FIELD_MODEL_GY, OnViewFieldModelGy)
	ON_UPDATE_COMMAND_UI(ID_VIEW_FIELD_MODEL_GY, OnUpdateViewFieldModelGy)
	ON_COMMAND(ID_VIEW_FIELD_MODEL_GZ, OnViewFieldModelGz)
	ON_UPDATE_COMMAND_UI(ID_VIEW_FIELD_MODEL_GZ, OnUpdateViewFieldModelGz)
	ON_COMMAND(ID_VIEW_FIELD_MODEL_M, OnViewFieldModelM)
	ON_UPDATE_COMMAND_UI(ID_VIEW_FIELD_MODEL_M, OnUpdateViewFieldModelM)
	ON_COMMAND(ID_VIEW_FIELD_MODEL_MX, OnViewFieldModelMx)
	ON_COMMAND(ID_VIEW_FIELD_MODEL_MY, OnViewFieldModelMy)
	ON_UPDATE_COMMAND_UI(ID_VIEW_FIELD_MODEL_MX, OnUpdateViewFieldModelMx)
	ON_UPDATE_COMMAND_UI(ID_VIEW_FIELD_MODEL_MY, OnUpdateViewFieldModelMy)
	ON_COMMAND(ID_VIEW_FIELD_MODEL_MZ, OnViewFieldModelMz)
	ON_UPDATE_COMMAND_UI(ID_VIEW_FIELD_MODEL_MZ, OnUpdateViewFieldModelMz)
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
	ON_COMMAND(ID_VIEW_AXIS_FIELD, OnViewAxisField)
	ON_UPDATE_COMMAND_UI(ID_VIEW_AXIS_FIELD, OnUpdateViewAxisField)
	ON_COMMAND(ID_VIEW_MEASURED_FIELD, OnViewMeasuredField)
	ON_UPDATE_COMMAND_UI(ID_VIEW_MEASURED_FIELD, OnUpdateViewMeasuredField)
	ON_COMMAND(ID_VIEW_MODELED_FIELD, OnViewModeledField)
	ON_UPDATE_COMMAND_UI(ID_VIEW_MODELED_FIELD, OnUpdateViewModeledField)
	ON_COMMAND(ID_VIEW_DIFFERENCE_FIELD, OnViewDifferenceField)
	ON_UPDATE_COMMAND_UI(ID_VIEW_DIFFERENCE_FIELD, OnUpdateViewDifferenceField)


	// Zooming
	ON_COMMAND(ID_VIEW_ZOOM, OnViewZoom)
	ON_COMMAND(ID_VIEW_ZOOM_IN, OnViewZoomIn)
	ON_COMMAND(ID_VIEW_ZOOM_OUT, OnViewZoomOut)
	ON_COMMAND(ID_VIEW_ZOOM_PAGE, OnViewZoomPage)
	ON_WM_ERASEBKGND()

	ON_COMMAND(ID_VIEW_NEXT_PROFILE, OnViewNextProfile)
	ON_COMMAND(ID_VIEW_PREVIOUS_PROFILE, OnViewPreviousProfile)
	ON_COMMAND(ID_PROFILE_EW, OnProfileEW)
	ON_UPDATE_COMMAND_UI(ID_PROFILE_EW, OnUpdateProfileEW)
	ON_COMMAND(ID_PROFILE_SN, OnProfileSN)
	ON_UPDATE_COMMAND_UI(ID_PROFILE_SN, OnUpdateProfileSN)
	ON_COMMAND(ID_VIEW_EQUAL_SCALE, OnViewEqualScale)
	ON_UPDATE_COMMAND_UI(ID_VIEW_EQUAL_SCALE, OnUpdateViewEqualScale)
	ON_COMMAND(ID_PROFILE_NEXT_SHOW, OnProfileNextShow)
	ON_UPDATE_COMMAND_UI(ID_PROFILE_NEXT_SHOW, OnUpdateProfileNextShow)
	ON_COMMAND(ID_PROFILE_PREVIOUS_SHOW, OnProfilePreviousShow)
	ON_UPDATE_COMMAND_UI(ID_PROFILE_PREVIOUS_SHOW, OnUpdateProfilePreviousShow)
	ON_COMMAND(ID_FILE_PRINT_SETUP, OnFilePrintSetup)
	ON_COMMAND(ID_PROFILE_SHOW_MODEL_GUIDELINE, OnProfileShowModelGuideline)
	ON_UPDATE_COMMAND_UI(ID_PROFILE_SHOW_MODEL_GUIDELINE, OnUpdateProfileShowModelGuideline)
	ON_COMMAND(ID_PROFILE_EXTREM, OnProfileExtrem)
	ON_UPDATE_COMMAND_UI(ID_PROFILE_EXTREM, OnUpdateProfileExtrem)
	ON_COMMAND(ID_PROFILE_SET_EXTREM_FIELD, OnProfileSetExtremField)
	ON_UPDATE_COMMAND_UI(ID_PROFILE_SET_EXTREM_FIELD, OnUpdateProfileSetExtremField)
	ON_COMMAND(ID_PROFILE_JUMPTOMAX, OnProfileJumptomax)
	ON_UPDATE_COMMAND_UI(ID_PROFILE_JUMPTOMAX, OnUpdateProfileJumptomax)
	ON_COMMAND(ID_PROFILE_JUMPTOMIN, OnProfileJumptomin)
	ON_UPDATE_COMMAND_UI(ID_PROFILE_JUMPTOMIN, OnUpdateProfileJumptomin)
	ON_COMMAND(ID_PROFILE_SAVE_CURRENT_SETTINGS, OnProfileSaveCurrentSettings)
	ON_COMMAND(ID_COMPUTE_FIT_1D, OnComputeFit1d)
	ON_COMMAND(ID_VERTEX_FIT, OnVertexFit)
	ON_UPDATE_COMMAND_UI(ID_VERTEX_FIT, OnUpdateVertexFit)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_COMMAND(ID_PROFILE_SHOW_TOOLTIP, OnProfileShowTooltip)
	ON_UPDATE_COMMAND_UI(ID_PROFILE_SHOW_TOOLTIP, OnUpdateProfileShowTooltip)
	ON_COMMAND(ID_PROFILE_SHOW_DRV, OnProfileShowDrv)
	ON_UPDATE_COMMAND_UI(ID_PROFILE_SHOW_DRV, OnUpdateProfileShowDrv)
	ON_COMMAND(ID_PROFILE_SHOW_RMS, OnProfileShowRms)
	ON_UPDATE_COMMAND_UI(ID_PROFILE_SHOW_RMS, OnUpdateProfileShowRms)
	ON_COMMAND(ID_BODY_INVERT_DENSITY, OnBodyInvertDensity)
	ON_COMMAND(ID_BODY_DENSITY_INVERSION_PROPERTIES, OnBodyDensityInversionProperties)

	ON_COMMAND(ID_VIEW_EXTENDRANGE, OnViewExtendrange)
	ON_UPDATE_COMMAND_UI(ID_VIEW_EXTENDRANGE, OnUpdateViewExtendrange)
	ON_COMMAND(ID_BODY_MOVEBOBY, OnBodyMoveboby)
	ON_UPDATE_COMMAND_UI(ID_BODY_MOVEBOBY, OnUpdateBodyMoveboby)
	END_MESSAGE_MAP()

// CViewProfPlus construction/destruction

CViewProfPlus::CViewProfPlus()
{
	m_bScale = TRUE;
	m_bInit = TRUE;

	m_nPrfType = PRF_ROW;
	mod = NULL;
	m_nCursor = CRS_NORMAL;
	m_fResize = 0.5f;
	m_bInitAxis = TRUE;
	m_szCross = CSize(2, 2);
	m_penRelief.CreatePen( PS_SOLID, 0, RGB(113,85,55) );
	m_bProfPrev = FALSE;
	m_bProfNext = FALSE; 

	m_axV.SetType(SB_VERT);
	m_axFld.SetType(SB_VERT);
	m_axFldVirtual.SetType(SB_VERT);
	m_bScaleEq = TRUE;

	// capture structure
	m_cpt.nBodyID = -1;
	m_cpt.nPrfIndexOld = -1;
	m_cpt.nTag = CPT_NONE;

	// current row, col...
	m_nCol = 1;
	m_nRow = 1;
	m_nColOld =1;
	m_nRowOld = 1;
	m_nRcIndex = 1;

	// drawing
	LOGBRUSH lb;
	lb.lbStyle = BS_HOLLOW;
	m_brushHollow.CreateBrushIndirect( &lb );

	memset(&m_lf, 0, sizeof(LOGFONT));			// zero out structure
	m_lf.lfHeight = 100;						// request a 12-pixel-height font
	strcpy(m_lf.lfFaceName, "Arial");			// request a face name "Arial"

	m_bAxis = FALSE;
	m_bAxRange = TRUE;
	m_nFldAxis = -1;		// scale axis for gz field
	m_nFldTbl = FLDTLB_MODELED;

	m_bZoomig = FALSE;

	m_iconRmsDwn = AfxGetApp()->LoadIcon( IDI_ICON_OK );
	m_iconRmsUp = AfxGetApp()->LoadIcon( IDI_ICON_BAD );

	// drawing area
	m_szLeftTop = CSize(0, 20);
	m_szRightBot = CSize(100, 0);

	InitPenFld();

	// printing
	m_rcPrnMargin.top = 2000;
	m_rcPrnMargin.left = 2000;
	m_rcPrnMargin.right = 2000;
	m_rcPrnMargin.bottom = 2000;
	m_ptPaperSize.x = 21590;
	m_ptPaperSize.y = 27940;

	m_bModelGL = TRUE;

	m_nFldExtrem = -1;
	m_bToolTip = TRUE;

	m_bShowRms = TRUE;
	m_bShowDrv = TRUE;

	m_bBmp = FALSE;

}

CViewProfPlus::~CViewProfPlus()
{
	m_dc.DeleteDC();
}


BOOL CViewProfPlus::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	return CView::PreCreateWindow(cs);
}

void CViewProfPlus::OnInitialUpdate()
{
	m_pDoc = GetDocument();
	mod = m_pDoc->GetModel();
	for( int i=0; i<m_pDoc->GetComponentsNumber(); i++ ) {
		m_bf[i] = FALSE;
	}
	m_bInit = TRUE;
	m_bScale = TRUE;

	if( mod ) {
		m_nFldAxis = GRDMOD_GZ;		// scale axis for gz field
		m_bf[GRDMOD_GZ] = TRUE;
		m_bf[GRDMOD_M] = TRUE;
		CheckAxis( m_nFldAxis );

		if( !m_pDoc->GetProfViewSettingsArray()->IsEmpty() ) {
			LoadProfileSettings(m_pDoc->GetProfViewSettingsArray()->GetAt(0));
		}
	}
	if( mod )
		SetVariables();

	m_toolTip.Create(this);
	m_strToolTip = "Profile View";
	m_toolTip.AddTool(this, m_strToolTip);
	m_toolTip.Activate(TRUE);
	CView::OnInitialUpdate();

}



// CViewProfPlus diagnostics

// CViewProfPlus message handlers

///////////////////////////////////////////////////////////////////////////////////////////
//******** cordinate transformations ******************************************************
void CViewProfPlus::DPtoLP(CPoint& pt)
{
	CClientDC dc(this);
	OnPrepareDC( &dc );
	dc.DPtoLP( &pt );
}

void CViewProfPlus::WPtoLP(CPoint &pt, int nIndex, int row, int col)
{
	double xw, zw;
	GetXZd(nIndex, row, col, xw, zw);
	pt.x = (int) (m_xOff + m_xSc*xw);
	pt.y = (int) (m_yOff + m_ySc*zw);
}

void CViewProfPlus::WPtoLP(int& x, int& y, double xw, double yw)
{
	x = (int) (m_xOff + m_xSc*xw);
	y = (int) (m_yOff + m_ySc*yw);
}

void CViewProfPlus::WPtoLP(CPoint &pt, double xw, double yw)
{
	pt.x = (int) (m_xOff + m_xSc*xw);
	pt.y = (int) (m_yOff + m_ySc*yw);
}

void CViewProfPlus::WPtoDP(int& x, int& y, double xw, double yw)
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

void CViewProfPlus::WPtoDP(CPoint& pt, double xw, double yw)
{
	pt.x = (int) (m_xOff + m_xSc*xw);
	pt.y = (int) (m_yOff + m_ySc*yw);
	CClientDC dc(this);
	OnPrepareDC( &dc );
	dc.LPtoDP( &pt );
}

void CViewProfPlus::DPtoWP(CPoint pt, double &xw, double &yw)
{
	CClientDC dc(this);
	OnPrepareDC( &dc );
	dc.DPtoLP( &pt );
	
	xw = (double)(pt.x-m_xOff) / m_xSc;
	yw = (double)(pt.y-m_yOff) / m_ySc;
}

void CViewProfPlus::LPtoWP(double& xw, double& yw, int xl, int yl)
{
	xw = (xl-m_xOff) / m_xSc;
	yw = (yl-m_yOff) / m_ySc;
}

double CViewProfPlus::GetXYd(int n)
{
	if(m_nPrfType == PROF_X) 
		return mod->GetXd(n);
	if(m_nPrfType == PROF_Y) 
		return mod->GetYd(n);
	return 0;
}

void CViewProfPlus::GetXZd(int nIndex, int row, int col, double& x, double& z)
{
	if(m_nPrfType == PROF_X)  
		mod->GetXZd(nIndex, row, col, x, z);
	if(m_nPrfType == PROF_Y)  
		mod->GetYZd(nIndex, row, col, x, z);
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CViewProfPlus printing
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

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CViewProfPlus drawing
void CViewProfPlus::OnDraw(CDC* pDC)
{
	if( pDC->IsPrinting() ) {
		Scale(pDC);
		Draw(pDC);
		return;
	}
	// use the semaphore to enter this critic section
	static BOOL 	bBusy = FALSE;	
	if(bBusy) 		return;
	bBusy = TRUE;

	// initial screen
	if( !mod ) {
		DrawInit(pDC);
		bBusy = FALSE;
		return;
	}

	if( m_bScale ) {
		Scale(pDC);
	}

	CRect clRect;
	CRect lpRect;
	GetClientRect(clRect);
	lpRect = clRect;
	pDC->DPtoLP( &lpRect );


	CBitmap bmp;
	CDC memDC;
	if( !memDC.CreateCompatibleDC(pDC) ) {
		AfxMessageBox("Cannot create device context for drawing. Closing.", MB_OK | MB_ICONSTOP);
		bBusy = FALSE;
		return;
	}

	if( clRect!=m_clientRectOld || !m_bBmp) {
		m_bmp.DeleteObject();
		m_bmp.CreateCompatibleBitmap(pDC, clRect.right-clRect.left, clRect.bottom-clRect.top);
		m_clientRectOld = clRect;
		m_bBmp = TRUE;
	}

	CBitmap *pBmp = memDC.SelectObject(&m_bmp);
	CBrush bkBrush( RGB(255,255,255) );
	CBrush* pdBrush = memDC.SelectObject(&bkBrush);
	memDC.PatBlt(0, 0, lpRect.right, -lpRect.bottom, PATCOPY);
	OnPrepareDC(&memDC);

	Draw(&memDC);

	pDC->BitBlt(0, 0, lpRect.right, lpRect.bottom, &memDC, 0, 0, SRCCOPY  );

	memDC.SelectObject(pBmp);
	memDC.SelectObject(pdBrush);
	memDC.DeleteDC();

	// turn off semaphore
	bBusy = FALSE;
}

void CViewProfPlus::DrawInit(CDC* pDC) 
{
	CRect clRect;
	CRect lpRect;
	GetClientRect(clRect);
	lpRect = clRect;
	pDC->DPtoLP( &lpRect );

	CFont	font, *pFont;
	LOGFONT	lf;
	memset(&lf, 0, sizeof(LOGFONT));
	lf.lfHeight = 160;
	strcpy(lf.lfFaceName, "Arial");
	font.CreatePointFontIndirect( &lf, pDC );
	pFont = pDC->SelectObject( &font );
	pDC->TextOut(200,(lpRect.top+lpRect.bottom)/2+300,"Define new observations.");
	pDC->TextOut(200,(lpRect.top+lpRect.bottom)/2 - 300,"Start in menu 'Model/Define Observation' or 'Model/Impotr Observation'.");
	pDC->SelectObject( pFont );
}

void CViewProfPlus::Draw(CDC* pDC)
{
	// select default font
	CFont font, *pFont;
	font.CreatePointFontIndirect( &m_lf, pDC );
	pFont = pDC->SelectObject( &font );
	// select default Hollow brush
	CBrush* pBrush = pDC->SelectObject( &m_brushHollow );
	CreatePenBrush();

	DrawGrdLines(pDC);
	DrawImages(pDC);
	DrawRelief(pDC);
	DrawPlg(pDC);
	DrawLeadingHorizon(pDC);
	DrawField(pDC);
	DrawCheckMarks(pDC);
	DrawProfileIntersections(pDC);
	DrawWells(pDC);
	m_axH.Draw(pDC);
	m_axV.Draw(pDC);
	m_axH.DrawGridLines(pDC, m_drwRect);
	m_axV.DrawGridLines(pDC, m_drwRect);

	if(GetCapture() == this && m_cpt.nTag == CPT_ZOOM && m_bZoomig) {
		CRect rc = m_zoomRect;
		pDC->DPtoLP(&rc);
		CPen	pen(PS_DOT, 0, RGB(166, 166, 166)), *pPen;
		pPen = pDC->SelectObject( &pen );
		pDC->Rectangle(&rc);
		pDC->SelectObject( pPen);
	}

	if(GetCapture() == this && m_cpt.nTag == CPT_MOVE_EDGE) {
		COLORREF color;
		color = mod->GetBody( m_cpt.nBodyID )->GetPLogPen()->lopnColor;
		CPen	pen(PS_DOT, 0, color), *pPen;
		pPen = pDC->SelectObject( &pen );
		pDC->Polyline(m_bdCreatPoly, 4);
		pDC->SelectObject( pPen);
	}

	pDC->SelectObject(pBrush);
	pDC->SelectObject(pFont);
}

int CViewProfPlus::GetMargin(CDC* pDC, CSize& szMargin)
{
	double axMin, axMax, axDatMin, axDatMax;
	double	min, max;
	CGrid*	pGrd;

	if( m_nFldAxis != -1 ) {
		pGrd = m_pDoc->GetGrid(m_nFldAxis);
		if(pGrd) {
			pGrd->GetMinMaxValue(min, max);
			m_axFld.GetMaxDataExtent(axMin, axMax);
			m_axFld.GetDataExtent(axDatMin, axDatMax);
			if( axMin != min || axMax != max) {
				m_axFld.SetMaxDataExtent( min, max);
				m_axFld.SetDataExtent( min, max);
			}
			m_axFld.FindAxisMinMaxStep();
		}
	}

	if( m_bInit ) {
		min = GetXYd(1);
		max = GetXYd(m_nRC-2);
		m_axH.SetMaxDataExtent(min, max);
		m_axH.SetDataExtent(min, max);

		min = mod->GetHell();
		max = mod->GetHeaven();
		m_axV.SetMaxDataExtent(min, max);
		m_axV.SetDataExtent(min, max);
	}

	szMargin.cy = m_axH.GetWidth(pDC);
	int lenFld=0;
	if( m_nFldAxis != -1 ) {
		lenFld = m_axFld.GetWidth(pDC);
	}
	szMargin.cx = max( m_axV.GetWidth(pDC), lenFld );

	return 0;
}

void CViewProfPlus::Scale(CDC* pDC)
{
	CRect	clRc;
	CSize	szCR;
	CSize	szLT;
	CSize	szRB;
	CSize	szTxt;
	CSize	szAxWidth;
	double	lx, ly;
	int y;
	m_szMargin = CSize(6, 6);

	// monitor painting
	if(!pDC->IsPrinting()) {
		GetClientRect( clRc );
		if(clRc.right < 100 || clRc.bottom < 100)		return;

		szCR.cx = clRc.right;
		szCR.cy = clRc.bottom;
		szLT = m_szLeftTop;
		szRB = m_szRightBot;

		pDC->DPtoLP( &szCR );
		pDC->DPtoLP( &m_szMargin );
		pDC->DPtoLP( &szLT );
		pDC->DPtoLP( &szRB );
	}
	// printing
	else {
		szCR.cx = m_ptPaperSize.x;
		szCR.cy = m_ptPaperSize.y;
		szLT = CSize(m_rcPrnMargin.left, m_rcPrnMargin.top);
		szRB = CSize(m_rcPrnMargin.right, m_rcPrnMargin.bottom);
		m_szMargin = CSize(m_rcPrnMargin.left, m_rcPrnMargin.bottom);
	}

	GetMargin(pDC, szAxWidth);
	if(!pDC->IsPrinting())
		szAxWidth.cx += m_szMargin.cx;
	szTxt = pDC->GetTextExtent(" ");
	// cross section drawing
	m_drwRect.left	= szLT.cx + szAxWidth.cx;
	m_drwRect.top	= (int) (-szCR.cy*m_fResize)- szTxt.cy;
	m_drwRect.right = szCR.cx - m_szMargin.cx - szAxWidth.cx;
	m_drwRect.bottom	= -szCR.cy + m_szMargin.cy + szAxWidth.cy;

	// field drawing
	m_drwRectFld.left	= szLT.cx + szAxWidth.cx;
	m_drwRectFld.top	= -m_szMargin.cy;
	m_drwRectFld.right	= szCR.cx - m_szMargin.cx - szAxWidth.cx;
	m_drwRectFld.bottom	= (int) (-szCR.cy*m_fResize)+szTxt.cy;

	if( m_nPrfType == PROF_X ) {
		lx = mod->GetLXd();	}
	if( m_nPrfType == PROF_Y ) 	{
		lx = mod->GetLYd();	}
	ly = mod->GetHeaven() - mod->GetHell();
	m_xy0 = GetXYd(0);

	m_xSc = (m_drwRect.right-m_drwRect.left) / (lx);
	m_ySc = (m_drwRect.top-m_drwRect.bottom) / (ly );
	if( m_bScaleEq ) {
		double sc = min(m_xSc, m_ySc);
		m_xSc = sc;
		m_ySc = sc;
		m_drwRect.right = m_drwRect.left+(int) (sc*lx);
		m_drwRect.top = m_drwRect.bottom+(int) (sc*ly);
		if( (m_drwRect.top + 4*szTxt.cy) < m_drwRectFld.bottom)
			m_drwRectFld.bottom = m_drwRect.top + 4*szTxt.cy;
	}
	m_xOff = m_drwRect.left - m_xSc*m_xy0;
	m_yOff = m_drwRect.bottom - m_ySc*mod->GetHell();

	double	min, max;
	int		xl, xr;
	CRect	clRect;

	min = GetXYd(1);
	max = GetXYd(m_nRC-2);
	WPtoLP(xl, y, min, 0); 
	WPtoLP(xr, y, max, 0); 
	m_axH.SetDrawRect( xl, -szCR.cy+m_szMargin.cy, xr, -szCR.cy);
	if( m_bInit ) {
		m_axH.SetMaxDataExtent(min, max);
		m_axH.SetDataExtent(min, max);
	}
	else {
		m_axH.GetDataExtent(min, max);
		m_axH.SetDataExtent(min, max);
	}

	m_axV.SetDrawRect( 0, m_drwRect.top, m_szMargin.cx, m_drwRect.bottom );
	if( m_bInit ) {
		min = mod->GetHell();
		max = mod->GetHeaven();
		m_axV.SetMaxDataExtent(min, max);
		m_axV.SetDataExtent(min, max);
	}
	else {
		m_axV.GetDataExtent(min, max);
		m_axV.SetDataExtent(min, max);
	}

	m_axH.GetScOff(m_xSc, m_xOff);
	m_axV.GetScOff(m_ySc, m_yOff);
	m_xOff = xl - m_xSc*m_axH.GetDataMin();
	m_yOff = m_drwRect.bottom - m_ySc*m_axV.GetDataMin();

	// Field axis
	//m_axFld.SetDrawRect( m_drwRectFld.left-m_szMargin.cx, m_drwRectFld.top, m_drwRect.left, m_drwRectFld.bottom);
	m_axFld.SetDrawRect( 0, m_drwRectFld.top, m_szMargin.cx, m_drwRectFld.bottom);
	if( m_bInit ) {
		m_bInitAxis = TRUE;
	}
	ScaleFieldAxis();

	// VirtualField axis
	m_axFldVirtual.SetDrawRect( m_drwRectFld.left-m_szMargin.cx, m_drwRectFld.top, m_drwRect.left, m_drwRectFld.bottom);

	// rescaling needs upadate all polygons
	UpdatePlgAll();

	if( pDC->IsPrinting() ) {
		m_axV.SetCustomWidth(FALSE);
		m_axFld.SetCustomWidth(FALSE);
	}
	else {
		m_axV.SetCustomWidth();
		m_axFld.SetCustomWidth();
	}
	m_axV.SetWidth(szAxWidth.cx);
	m_axFld.SetWidth(szAxWidth.cx);
	// turn off rescaling
	m_bScale = FALSE;
	m_bInit = FALSE;
}

void CViewProfPlus::ScaleFieldAxis(void)
{
	double min, max;

	if( m_bInitAxis ) {
		CGrid* pGrd = m_pDoc->GetGrid(m_nFldAxis);
		if( pGrd && !pGrd->IsPlanar()) {
			pGrd->GetMinMaxValue(min, max);
			m_axFld.SetMaxDataExtent(min, max);
			m_axFld.SetDataExtent(min, max);
		}
		else {
			m_axFld.SetMaxDataExtent(0, 1);
			m_axFld.SetDataExtent(0, 1);
		}
		m_bInitAxis = FALSE;
	}
	else {
		m_axFld.GetDataExtent(min, max);
		m_axFld.SetDataExtent(min, max);
	}
}

void CViewProfPlus::UpdateScale(void)
{
	m_axH.GetScOff(m_xSc, m_xOff);
	m_axV.GetScOff(m_ySc, m_yOff);
	m_xOff = m_axH.GetLeft() - m_xSc*m_axH.GetDataMin();
	m_yOff = m_drwRect.bottom - m_ySc*m_axV.GetDataMin();
}

void CViewProfPlus::CreatePenBrush(void)
{
	int n = (int)mod->GetBodies()->GetSize();
	CBody* pBd;
	m_pen.RemoveAll();	m_pen.SetSize(n, 0);
	m_penPrevNext.RemoveAll();	m_penPrevNext.SetSize(n, 0);
	m_brs.RemoveAll();	m_brs.SetSize(n, 0);
	for(int i=0; i<n; i++) {
		pBd = mod->GetBodies()->GetAt(i);
		pBd->SetIndex(i);
		LOGPEN lp = *pBd->GetPLogPen();
		lp.lopnStyle = PS_DOT;
		m_pen[i].CreatePenIndirect( pBd->GetPLogPen() );
		m_penPrevNext[i].CreatePenIndirect( &lp );
		m_brs[i].CreateBrushIndirect( pBd->GetPLogBrush() );
	}
}

void CViewProfPlus::DrawPlg(CDC* pDC)
{
	int			k, l;
	BODYPLG*	pBplg;
	CPen*		pPen;
	CBrush*		pBrush;

	k = (int)m_plg.GetSize()-1;
	for(int i=0; i<k; i++) {
		l = (int)m_plg[i].GetSize();
		for(int j=0; j<l; j++) {
			pBplg = &m_plg[i].GetAt(j);
			// draw
			pPen = pDC->SelectObject( &m_pen.GetAt(pBplg->nPen) );
			if( pBplg->bFill )		
				pBrush = pDC->SelectObject( &m_brs.GetAt(pBplg->nBrush) );
			
			pDC->Polygon( pBplg->plg, 4 );
			
			pDC->SelectObject( pPen );
			if( pBplg->bFill )		
				pDC->SelectObject( pBrush );
		}
	}

	// previous profile
	if( m_bProfPrev ) {
		k = (int)m_plgPrev.GetSize()-1;
		for(int i=0; i<k; i++) {
			l = (int)m_plgPrev[i].GetSize();
			for(int j=0; j<l; j++) {
				pBplg = &m_plgPrev[i].GetAt(j);
				pPen = pDC->SelectObject( &m_penPrevNext.GetAt(pBplg->nPen) );
				pDC->Polygon( pBplg->plg, 4 );
				pDC->SelectObject( pPen );
			}
		}
	}
	// next profile
	if( m_bProfNext ) {
		k = (int)m_plgNext.GetSize()-1;
		for(int i=0; i<k; i++) {
			l = (int)m_plgNext[i].GetSize();
			for(int j=0; j<l; j++) {
				pBplg = &m_plgNext[i].GetAt(j);
				pPen = pDC->SelectObject( &m_penPrevNext.GetAt(pBplg->nPen) );
				pDC->Polygon( pBplg->plg, 4 );
				pDC->SelectObject( pPen );
			}
		}
	}
}

void CViewProfPlus::DrawRelief(CDC* pDC)
{
	int		i;
	int		*p_i, *p_j;
	double	x, z;
	CPoint	pt;
	CString	strLeft, strRight;

	// if row OR column view
	if( m_nPrfType == PROF_X ) {
		p_i = &m_nRow;
		p_j = &i;
		strLeft = "West";
		strRight = "East";
	}
	if( m_nPrfType == PROF_Y ) {
		p_i = &i;
		p_j = &m_nCol;
		strLeft = "South";
		strRight = "North";
	}

	// relief
	CPen* pPen = pDC->SelectObject( &m_penRelief );
	i=0;
	z = mod->GetZ(*p_i, *p_j, 0);
	x = GetXYd(i);
	WPtoLP(pt, x, z);
	pDC->MoveTo(pt);
	for(i=1; i < m_nRC; i++) {
		z = mod->GetZ(*p_i, *p_j, 0);
		x = GetXYd(i);
		WPtoLP(pt, x, z);
		pDC->LineTo(pt);
	}
	pDC->SelectObject(pPen);

	// profile orientation
	UINT oldTA = pDC->SetTextAlign(TA_LEFT | TA_BOTTOM);
	pDC->TextOut(m_drwRect.left, m_drwRect.top, strLeft);
	pDC->SetTextAlign(TA_RIGHT | TA_BOTTOM);
	pDC->TextOut(m_drwRect.right, m_drwRect.top, strRight);
	pDC->SetTextAlign(oldTA);
}

void CViewProfPlus::UpdatePlgAll()
{
	int i;
	int *p_i, *p_j;
	int *p_ip=NULL, *p_jp=NULL;		// previous profile
	int *p_in=NULL, *p_jn=NULL;		// next profile
	int prev, next;

	// if row OR column view
	if( m_nPrfType == PROF_X ) {
		prev = m_nRow-1;
		next = m_nRow+1;
		if(prev >= 0)		{
			p_ip = &prev;
		}
		if(next < mod->GetRows() )	 {
			p_in = &next;
		}
		p_i = &m_nRow;
		p_jp = &i;
		p_jn = &i;
		p_j = &i;
	}
	if( m_nPrfType == PROF_Y ) {
		prev = m_nCol-1;
		next = m_nCol+1;
		if(prev >= 0)		{
			p_jp = &prev;
		}
		if(next < mod->GetCols() )	 {
			p_jn = &next;
		}
		p_i = &i;
		p_ip = &i;
		p_in = &i;
		p_j = &m_nCol;
	}
	for(i=0; i < m_nRC-1; i++) {
		if( m_bProfPrev && p_ip && p_jp )
			UpdatePlg(m_plgPrev, *p_ip, *p_jp, 0, 1, PPN_PREV);
		UpdatePlg(m_plg, *p_i, *p_j, 0, 1);
		if( m_bProfNext && p_in && p_jn )
			UpdatePlg(m_plgNext, *p_in, *p_jn, 0, 1, PPN_NEXT);
	}
}

void CViewProfPlus::UpdatePlg(ClmnPlgArray& plg, int row, int col, int startIncr, int endIncr, int nTag)
{
	// nTag - previous, next or current profile
	int		rcStart, rcEnd;			// update changed row/column
	double	xw, zw;
	int		nID;
	int		i, ii, jj;
	int		*p_i, *p_j;
	int		rc;						// number of rows or columns
	BODYPLG bplg;
	BOOL	bIn;
	CBody*	pBody;

	// if( rowView || colView)
	rc = m_nRC-1;
	if( m_nPrfType == PROF_X ) {
		rcStart = col+startIncr;	
		rcEnd	= col+endIncr;	
		p_i = &row;
		if(nTag == PPN_CURRENT) p_i = &m_nRow;
		p_j = &i;
	}
	if( m_nPrfType == PROF_Y ) {
		rcStart = row+startIncr;	
		rcEnd	= row+endIncr;	
		p_i = &i;
		p_j = &col;
		if(nTag == PPN_CURRENT) p_j = &m_nCol;
	}

	if( rcStart < 0 )		rcStart=0;
	if( rcEnd > rc)			rcEnd = rc;

	for(i=rcStart; i < rcEnd; i++) {
		plg[i].RemoveAll();	
		plg[i].SetSize(0, 50);
		if( mod->GetCount(*p_i, *p_j) >2 ) {
			for( int k=1; k<mod->GetCount(*p_i, *p_j)-1; k += 2) {
				GetXZd(k, *p_i, *p_j, xw, zw);
				WPtoLP(bplg.plg[0], xw, zw);
				GetXZd(k+1, *p_i, *p_j, xw, zw);
				WPtoLP(bplg.plg[3], xw, zw);
				nID = mod->GetID(k, *p_i, *p_j);
				// if( rowView || colView)
				pBody = mod->GetBody(nID);
				int n = pBody->GetIndex();
				if( m_nPrfType == PROF_X ) {
					ii = *p_i; jj = *p_j+1;}
				if( m_nPrfType == PROF_Y ) {
					ii = *p_i+1; jj = *p_j;}
				bIn = FALSE;
				if( mod->GetCount(ii, jj) > 2 ) {
					n = mod->GetBody(nID)->GetIndex();
					for( int l=1; l<mod->GetCount(ii, jj)-1; l += 2) {
						if( nID == mod->GetID(l, ii, jj) ) {
							GetXZd(l, ii, jj, xw, zw);
							WPtoLP(bplg.plg[1], xw, zw);
							GetXZd(l+1, ii, jj, xw, zw);
							WPtoLP(bplg.plg[2], xw, zw);
							// add polygons
							bplg.bFill = pBody->IsFilled();
							bplg.nBrush = n;
							bplg.nPen = n;
							bplg.id = nID;
							plg[i].Add( bplg );
							bIn = TRUE;
							break;
						}
					}
				}
				if( !bIn ) {
					bplg.plg[1] = bplg.plg[0];
					bplg.plg[2] = bplg.plg[3];
					// add polygons
					bplg.bFill = pBody->IsFilled();
					bplg.nBrush = n;
					bplg.nPen = n;
					plg[i].Add( bplg );
				}
			}
		}
	}
}

void CViewProfPlus::DrawField(CDC* pDC)
{
	TRACE("Entering CViewProfPlus::DrawField(CDC* pDC)\n");

	CPen	*pPen;
	double	**pFld;								// grid data
	double	xw;
	double	min, max;
	double	sc, off;
	double	v;
	int		i;
	int		*p_i, *p_j;
	int		rc;						// number of rows or columns
	int		x, y;
	int		row, col;
	int		nComponents, nCompomentsDif;
	int		xLegend, xDifTxt;
	BOOL	bNewLine = TRUE;
	BOOL	bLine;
	CSize	szCross = m_szCross;

	// legend ++++++++++++++++++++++++++
	CSize 	s( 30, 12 );
	CSize	is(16, 16);
	int		l=0, m=0;;
	int		x0, y0, x1, y1;
	CString str;

	pDC->DPtoLP( &s );
	pDC->DPtoLP( &is );
	y0 = m_drwRectFld.bottom;
	y1 = m_drwRectFld.top;
	xLegend = m_drwRectFld.left + pDC->GetTextExtent("  ").cx;
	xDifTxt = xLegend+ pDC->GetTextExtent("       ").cx;
	x0 = m_drwRectFld.left + s.cx;
	x1 = m_drwRectFld.left + 3*s.cx;
	CFont	font, *pFont;
	font.CreatePointFontIndirect(&m_lf, pDC);
	pFont = pDC->SelectObject(&font);
	//++++++++++++++++++++++++++++++++++

	row = m_nRow-1;
	col = m_nCol-1;
	// if( rowView || colView)
	rc = m_nRC-2;					//!!! grid
	CGrid* pObsGrd = m_pDoc->GetObservationGrid();
	if(!pObsGrd) return;
	if( m_nPrfType == PROF_X ) {
		if( row < 0 || row > pObsGrd->GetRows()-1)	
			return; 
		p_i = &row;
		p_j = &i;
	}
	if( m_nPrfType == PROF_Y ) {
		if( col < 0 || col > pObsGrd->GetCols()-1)	
			return; 
		p_i = &i;
		p_j = &col;
	}

	// draw selected components
	pDC->DPtoLP(&szCross);
	nComponents = m_pDoc->GetComponentsNumber();
	nCompomentsDif = m_pDoc->GetComponentsDif();
	for(int k=0; k < nComponents; k++) {
		if( m_bf[k] ) {
			// set pen
			pPen = pDC->SelectObject( &m_penFld[k] );
				CGrid* pGrd = m_pDoc->GetGrid( k );
				if( pGrd ) {
					pFld = pGrd->GetData();
					pGrd->GetMinMaxValue(min, max);

					if( k >= nCompomentsDif && k < nComponents )		bLine = FALSE;
					else												bLine = TRUE;

					if( k==m_nFldAxis ) {		// field axis OR measured field axis
						double axMin, axMax, axDatMin, axDatMax;
						m_axFld.GetMaxDataExtent(axMin, axMax);
						m_axFld.GetDataExtent(axDatMin, axDatMax);
						// extend the range for modeled and measured field together
						if(m_bAxRange) {
							int kR = m_nFldAxis+40;
							CGrid* pGrdR = m_pDoc->GetGrid( kR );
							if(pGrdR!=NULL && m_bf[kR]) {
								double minR, maxR;
								pGrdR->GetMinMaxValue(minR, maxR);
								min = min(min, minR);
								max = max(max, maxR);
							}
						}
						if( axMin != min || axMax != max) {
							m_axFld.SetMaxDataExtent( min, max);
							m_axFld.SetDataExtent( min, max);
						}
						m_axFld.GetScOff(sc, off);
						m_axFld.Draw(pDC);
						pDC->SetTextAlign(TA_TOP | TA_LEFT);
						pDC->TextOut(m_drwRectFld.left + pDC->GetTextExtent("  ").cx, m_drwRectFld.top, m_pDoc->GetFieldString(m_nFldAxis) );
					}
					else if( k==m_nFldAxis-40 && m_bAxRange) {		// field axis OR measured field axis
						double axMin, axMax, axDatMin, axDatMax;
						m_axFld.GetMaxDataExtent(axMin, axMax);
						m_axFld.GetDataExtent(axDatMin, axDatMax);
						// extend the range for modeled and measured field together
						if(m_bAxRange) {
							int kR = m_nFldAxis;
							CGrid* pGrdR = m_pDoc->GetGrid( kR );
							if(pGrdR!=NULL && m_bf[kR]) {
								double minR, maxR;
								pGrdR->GetMinMaxValue(minR, maxR);
								min = min(min, minR);
								max = max(max, maxR);
							}
						}
						if( axMin != min || axMax != max) {
							m_axFld.SetMaxDataExtent( min, max);
							m_axFld.SetDataExtent( min, max);
						}
						m_axFld.GetScOff(sc, off);
						m_axFld.Draw(pDC);
						pDC->SetTextAlign(TA_TOP | TA_LEFT);
						pDC->TextOut(m_drwRectFld.left + pDC->GetTextExtent("  ").cx, m_drwRectFld.top, m_pDoc->GetFieldString(m_nFldAxis) );
					}
					//else if( (k==m_nFldAxis+40) && (!bLine) ) {		// measured field axis
					//	m_axFld.GetScOff(sc, off);
					//}
					else {
						m_axFldVirtual.SetMaxDataExtent(min, max);
						m_axFldVirtual.SetDataExtent(min, max);
						m_axFldVirtual.GetScOff(sc, off);
					}
					
					bNewLine = TRUE;
					for(i=0; i < rc; i++) {
						v = pFld[*p_i][*p_j];
						if( v != DUMMY ) {
							xw = GetXYd(i+1);
							x = (int) (m_xOff + m_xSc*xw);
							y = (int) (off + sc*v);
							// line
							if( bLine ) {
								if(bNewLine)	 {
									pDC->MoveTo(x, y);
									bNewLine = FALSE;
								}
								else		
									pDC->LineTo(x, y);
							}
							// crosses for measured field
							else {
								pDC->MoveTo(x-szCross.cx, y);	pDC->LineTo(x+szCross.cx, y);
								pDC->MoveTo(x, y-szCross.cy);	pDC->LineTo(x, y+szCross.cy);
							}
						}
						else
							bNewLine = TRUE;
					}	// all points on profile

					// ++++++++ legend +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
					pDC->SetTextAlign(TA_LEFT | TA_BOTTOM);
					str = m_pDoc->GetFieldString(k);
					int strLen = pDC->GetTextExtent(str).cx + pDC->GetTextExtent(" ").cx;
					pDC->TextOut( xLegend, y0, str);

					//----- field symbols------------------
					CSize szCross(3, 3);
					pDC->DPtoLP(&szCross);
					if( k>=GRDMES_GX && k<=GRDMES_MYZ ) {
						int nc = strLen/4;
						pDC->MoveTo(xLegend+nc, y0+szCross.cy);
						pDC->LineTo(xLegend+nc, y0-szCross.cy);
						pDC->MoveTo(xLegend+nc-szCross.cx, y0);
						pDC->LineTo(xLegend+nc+szCross.cx, y0);
						xLegend += strLen;
						int ne = xLegend-pDC->GetTextExtent(" ").cx-nc;
						pDC->MoveTo(ne, y0+szCross.cy);
						pDC->LineTo(ne, y0-szCross.cy);
						pDC->MoveTo(ne+szCross.cx, y0);
						pDC->LineTo(ne-szCross.cy, y0);
					}
					else {
						pDC->MoveTo(xLegend, y0);
						xLegend += strLen;
						pDC->LineTo(xLegend-pDC->GetTextExtent(" ").cx, y0);
					}
					//----- field symbols------------------
					l++;

					// RMS only for difference field
					if( k>=GRDDIF_GX && k<=GRDDIF_MYZ && (m_bShowRms || m_bShowDrv) && !pDC->IsPrinting() ) {
						CGrid*	pGrdDif = m_pDoc->GetGrid(k);
						if( !pGrdDif->IsPlanar() ) {
							CSize szText(0,0);
							CString strRms="";
							CString strDrv="";
							if(m_bShowRms) {
								strRms.Format("%s: RMS=%g", str, pGrdDif->GetRMS() );
								szText = pDC->GetTextExtent(strRms);
								pDC->SetTextAlign(TA_LEFT | TA_TOP);
								pDC->TextOut( xDifTxt, y1, strRms);
								strRms += "  ";
								if( pGrdDif->GetRMSTrend() )
									DrawIconEx(pDC->GetSafeHdc(), xDifTxt, y1-szText.cy, m_iconRmsDwn, is.cx, -is.cy, 0, NULL, DI_NORMAL);
								else
									DrawIconEx(pDC->GetSafeHdc(), xDifTxt, y1-szText.cy, m_iconRmsUp, is.cx, -is.cy, 0, NULL, DI_NORMAL);
							}
							

							if(m_bShowDrv) {
								strDrv.Format("%s: Drv=%g", str, pGrdDif->GetDrv() );
								pDC->SetTextAlign(TA_LEFT | TA_TOP);
								szText = pDC->GetTextExtent(strDrv);
								pDC->TextOut( xDifTxt, y1-2*szText.cy, strDrv);
								strDrv += "  ";
								if( pGrdDif->GetDrvTrend() )
									DrawIconEx(pDC->GetSafeHdc(), xDifTxt, y1-3*szText.cy, m_iconRmsDwn, is.cx, -is.cy, 0, NULL, DI_NORMAL);
								else
									DrawIconEx(pDC->GetSafeHdc(), xDifTxt, y1-3*szText.cy, m_iconRmsUp, is.cx, -is.cy, 0, NULL, DI_NORMAL);
							}
							xDifTxt += szText.cx + s.cx;
						}
					}
					// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

				pDC->SelectObject( pPen );
			}	// if(pGrd)
		} // if(m_bf[k])
	}	// all grids
	pDC->SelectObject(pFont);
}

void CViewProfPlus::DrawGrdLines(CDC* pDC)
{
	if( !m_bModelGL )	return;

	CPoint	pt;
	CPen	pen(PS_DOT, 0, RGB(180,180,180)), *pPen;

	pPen = pDC->SelectObject( &pen );

	for(int i=0; i<m_nRC; i++) {
		WPtoLP(pt, GetXYd(i), mod->GetHell());
		pDC->MoveTo(pt);
		WPtoLP(pt, GetXYd(i), mod->GetHeaven());
		pDC->LineTo(pt);
	}

	pDC->SelectObject( pPen );
}

void CViewProfPlus::DrawProfileIntersections(CDC* pDC)
{
	CPoint	pt1, pt2;
	CSize	sz(2, 2);
	CPen	pen, penMin, penMax;
	CPen*	pPen;

	pen.CreatePen(PS_SOLID, 0, RGB(180, 180, 180));
	pPen = pDC->SelectObject( &pen );
	pDC->DPtoLP(&sz);
	// Draw where is the profile
	pPen = pDC->SelectObject(&pen);
	POSITION pos = m_pDoc->GetFirstViewPosition();
	while (pos != NULL) {
		CView* pView = m_pDoc->GetNextView(pos);
		if( pView->IsKindOf(RUNTIME_CLASS( CViewProfPlus)) && pView != this) {
			CViewProfPlus *pProfView = (CViewProfPlus*) pView;
			int nSel;
			if(pProfView->GetProfType() != m_nPrfType) {
				nSel = pProfView->m_nRcIndex;	
				WPtoLP(pt1, GetXYd(nSel), mod->GetHell());
				pDC->MoveTo(pt1.x-sz.cx, pt1.y);
				pDC->LineTo(pt1.x+sz.cx, pt1.y);
				WPtoLP(pt2, GetXYd(nSel), mod->GetHeaven());
				pDC->LineTo(pt2.x+sz.cx, pt2.y);
				pDC->LineTo(pt2.x-sz.cx, pt2.y);
				pDC->LineTo(pt2.x-sz.cx, pt1.y);
			}
		}
	}   
	pDC->SelectObject( pPen );


	if( m_nFldExtrem == -1 ) return;
	CGrid* pGrd;
	pGrd = m_pDoc->GetGrid(m_nFldExtrem);
	if( pGrd != NULL ) {
		int rowMin, rowMax, colMin, colMax;
		pGrd->GetRowColMax(rowMax, colMax);
		pGrd->GetRowColMin(rowMin, colMin);
		rowMin++; rowMax++; colMin++; colMax++;
		penMax.CreatePen(PS_DOT, 0, RGB(180, 0, 0));
		penMin.CreatePen(PS_DOT, 0, RGB(0, 0, 180));
		if( m_nPrfType == PROF_X) {
			// min
			if( m_nRow==rowMin ) {
				pPen = pDC->SelectObject(&penMin);
				WPtoLP(pt1, GetXYd(colMin), mod->GetHell());
				pDC->MoveTo(pt1.x, pt1.y);
				WPtoLP(pt2, GetXYd(colMin), mod->GetHeaven());
				pDC->LineTo(pt2.x, pt2.y);
				pDC->SelectObject(pPen);
			}
			// max
			if( m_nRow==rowMax ) {
				pPen = pDC->SelectObject(&penMax);
				WPtoLP(pt1, GetXYd(colMax), mod->GetHell());
				pDC->MoveTo(pt1.x, pt1.y);
				WPtoLP(pt2, GetXYd(colMax), mod->GetHeaven());
				pDC->LineTo(pt2.x, pt2.y);
				pDC->SelectObject(pPen);
			}
		}
		if( m_nPrfType == PROF_Y ) {
			// min
			if( m_nCol==colMin ) {
				pPen = pDC->SelectObject(&penMin);
				WPtoLP(pt1, GetXYd(rowMin), mod->GetHell());
				pDC->MoveTo(pt1.x, pt1.y);
				WPtoLP(pt2, GetXYd(rowMin), mod->GetHeaven());
				pDC->LineTo(pt2.x, pt2.y);
				pDC->SelectObject(pPen);
			}
			// max
			if( m_nCol==colMax ) {
				pPen = pDC->SelectObject(&penMax);
				WPtoLP(pt1, GetXYd(rowMax), mod->GetHell());
				pDC->MoveTo(pt1.x, pt1.y);
				WPtoLP(pt2, GetXYd(rowMax), mod->GetHeaven());
				pDC->LineTo(pt2.x, pt2.y);
				pDC->SelectObject(pPen);
			}
		}
		penMax.DeleteObject();
		penMin.DeleteObject();
	}

}

void CViewProfPlus::DrawWells(CDC* pDC)
{
	double		xy;

	if( m_nPrfType == PRF_HRZ ) {
		xy = mod->GetYe(m_nRow);
	}
	if( m_nPrfType == PRF_VRT) {
		xy = mod->GetXe(m_nCol);
	}

	CMyObject *pObj;
	for(int i=0; i<m_pDoc->GetObjArray()->GetSize(); i++) {
		pObj = m_pDoc->GetObjArray()->GetAt( i );
		if( pObj->IsKindOf( RUNTIME_CLASS( CWell ) ) ) {
			CWell* pW = (CWell*) pObj;
			pW->DrawProfile( pDC, m_xSc, m_xOff, m_ySc, m_yOff, m_nPrfType, xy, m_cs*2 );
		}
	}
}

void CViewProfPlus::DrawImages(CDC* pDC)
{
	TRACE("Entering \n");

	double		xy;

	if( m_nPrfType == PRF_HRZ ) {
		xy = mod->GetYe(m_nRow);
	}
	if( m_nPrfType == PRF_VRT) {
		xy = mod->GetXe(m_nCol);
	}

	CMyObject *pObj;
	for(int i=0; i<m_pDoc->GetObjArray()->GetSize(); i++) {
		pObj = m_pDoc->GetObjArray()->GetAt( i );
		if( pObj->IsKindOf( RUNTIME_CLASS( CImgReferenced ) ) ) {
			CImgReferenced* pIm = (CImgReferenced*) pObj;
			if( pIm->IsOnProfile( m_nPrfType, xy, mod->GetX0(), mod->GetY0(), mod->GetSizeX(), mod->GetSizeX() ) &&
				!pIm->m_img.IsNull() && pIm->m_bVisible /*&& m_bShowPic*/ ) {
					pIm->DrawProfile( pDC, m_xSc, m_xOff, m_ySc, m_yOff, m_nPrfType, xy );
			}
		}
	}
}

void CViewProfPlus::DrawLeadingHorizon(CDC* pDC)
{
	CMyObject*	pObj;
	CGridObj*	pGrdObj=NULL;
	CGrid*		pGrd;
	int		i;

	for(i=0; i<m_pDoc->GetObjArray()->GetSize(); i++) {
		pObj = m_pDoc->GetObjArray()->GetAt( i );
		if( pObj->IsKindOf( RUNTIME_CLASS(CGridObj) ) ) {
			pGrdObj = (CGridObj*) pObj;
			pGrd = pGrdObj->GetGrid();
			if( pGrdObj->IsVisible() && !pGrd->IsEmpty() ) {
				// here draw it!
				CPen pen, *pPen;
				pen.CreatePenIndirect(&pGrdObj->GetLogPen());
				pPen = pDC->SelectObject(&pen);
					int		*p_i, *p_j;
					int		row, col;
					int		nProfs;
					double	x, z;
					double**	pData;	
					BOOL	bPrev=FALSE;
					CPoint	pt;
					CString	strLeft, strRight;
					pData = pGrd->GetData();

					// if row OR column view
					if( m_nPrfType == PROF_X ) {
						nProfs = mod->GetRows();
						if(m_nRow==0 || m_nRow>=nProfs-1) return;
						row = m_nRow-1;
						p_i = &row;
						p_j = &i;
					}
					if( m_nPrfType == PROF_Y ) {
						nProfs = mod->GetCols();
						if(m_nCol==0 || m_nCol>=nProfs-1) return;
						col = m_nCol-1;
						p_i = &i;
						p_j = &col;
					}

					// relief
					i=0;
					z = pData[*p_i][*p_j];
					if( z!=DUMMY) {
						x = GetXYd(i+1);
						WPtoLP(pt, x, z);
						pDC->MoveTo(pt);
						bPrev=TRUE;
					}
					for(i=1; i < m_nRC-2; i++) {
						z = pData[*p_i][*p_j];
						x = GetXYd(i+1);
						WPtoLP(pt, x, z);
						if(z!=DUMMY && bPrev)
							pDC->LineTo(pt);
						if(z!=DUMMY && !bPrev) {
							pDC->MoveTo(pt);
							bPrev = TRUE;
						}
					}
				pDC->SelectObject(pPen);
				pen.DeleteObject();
			}
		}
	}
}

void CViewProfPlus::DrawCheckMarks(CDC* pDC)
{
	TRACE("Entering DrawCheckMarks(CDC* pDC)\n");

	if( pDC->IsPrinting() ) return;

	for(int i=0; i<m_guideMarks.GetSize(); i++) {
		CGuideMark* pGm = &m_guideMarks.GetAt(i);
		pGm->Draw(pDC, m_xSc, m_xOff, m_ySc, m_yOff, mod->GetZMax());
	}
}

void CViewProfPlus::CopyProfile(int nIncr)
{
	AfxMessageBox("!!!");
	//int id, nFrom, nTo, nTag;

	//if( m_nPrfType == PROF_X ) {
	//	nFrom = m_nRow; 
	//	nTo = m_nRow+nIncr;
	//	nTag = PRF_ROW;
	//}
	//if( m_nPrfType == PROF_Y ){
	//	nFrom = m_nCol;
	//	nTo = m_nCol+nIncr;
	//	nTag = PRF_COL;
	//}

	//id = m_nIndexID;
	//mod->CopyBody(id, nFrom, nTo, nTag);
	//if( !m_pDoc->UpdateField() )
	//	m_pDoc->UpdateAllViews(NULL);
}

void CViewProfPlus::RemoveBodyFromProfile(void)
{
	AfxMessageBox("!!!");
	//int nFrom;

	//// row OR column view
	//if( m_nPrfType == PROF_X ) {
	//	nFrom = m_nRow;
	//}
	//if( m_nPrfType == PROF_Y ) {
	//	nFrom = m_nCol;
	//}
	//mod->RemoveBodyFromProfile(m_nIndexID, nFrom, m_nPrfType);
	//if( !m_pDoc->UpdateField() )
	//	m_pDoc->UpdateAllViews( NULL, UPDATE_PROPERTIES_REDRAW );
}

void CViewProfPlus::FindIndex(CPoint pt)
{
	double	xw, yw;
	DPtoWP(pt, xw, yw);

	int n = (int) ( (xw - m_xy0 + m_cs/2) / m_cs );
	if( n < 0 )			n=0;
	if( n > m_nRC-1)	n=m_nRC-1;

	if( m_nPrfType == PROF_X )  {
		m_nCol = n;
		g_x = xw;
		g_y = mod->GetYe(m_nRow);
	}
	if( m_nPrfType == PROF_Y )  {
		m_nRow = n;
		g_y = xw;
		g_x = mod->GetXe(m_nCol);
	}
	m_nPrfIndex = n;

	int nCrs = (int) ( (xw - m_xy0) / m_cs );
	if( nCrs < 0 )			nCrs=0;
	if( nCrs > m_nRC-2)		nCrs=m_nRC-2;
	m_nCrsIndex = nCrs;

	// set indicators
	g_z = yw; 
	m_pDoc->SetIndicators(m_nRow-1, m_nCol-1);
}

void CViewProfPlus::MoveProfile(int incr)
{

	//SetVariables();
	//// set indicators
	//m_pDoc->SetIndicators(m_nRow-1, m_nCol-1);
	//m_pDoc->UpdateAllViews( NULL, UPDATE_MOVE_PROFILE );
}

void CViewProfPlus::MoveProfileTo(int nPrf)
{
	//if( nPrf < 0 )			return;
	//if( nPrf > m_nRC-1)		return;

	//if( m_nPrfType == PROF_X ) 
	//{
	//	m_nRow = nPrf;
	//	m_nRcIndex = nPrf;
	//	g_y = mod->GetYe(m_nRow);
	//}
	//if( m_nPrfType == PROF_Y ) {
	//	m_nCol = nPrf;
	//	m_nRcIndex = nPrf;
	//	g_x = mod->GetXe(m_nCol);
	//}

	//SetVariables();
	//// set indicators
	//m_pDoc->SetIndicators(m_nRow-1, m_nCol-1);
	//m_pDoc->UpdateAllViews( NULL, UPDATE_MOVE_PROFILE );
}

/*	functions creates guide marks for given profile
	fills the variable m_ptGuideMark with the point in logical coorcinates for drawing
*/
int CViewProfPlus::CraeteGuideMarks(void)
{
	//TRACE("Entering CraeteGuideMarks(void) \n");

	//double		xy;
	//CPoint		pt;

	//if( m_nPrfType == PRF_HRZ ) {
	//	xy = mod->GetYe(m_nRow);
	//}
	//if( m_nPrfType == PRF_VRT) {
	//	xy = mod->GetXe(m_nCol);
	//}

	//CMyObject *pObj;
	//m_guideMarks.RemoveAll();
	//for(int i=0; i<m_pDoc->GetObjArray()->GetSize(); i++) {
	//	pObj = m_pDoc->GetObjArray()->GetAt( i );
	//	if( pObj->IsKindOf( RUNTIME_CLASS( CGuideMark ) ) ) {
	//		CGuideMark* pGm = (CGuideMark*) pObj;
	//		pGm->FindIntersections(m_nPrfType, xy);
	//		if( !pGm->m_pt3DArrayProf.IsEmpty() ) {
	//			m_guideMarks.Add( *pGm );
	//		}
	//	}
	//}
	//return 0;
}


void CViewProfPlus::SetVariables()
{
	//CString str1, str;
	//int n;

	//// init drawing variables
	//if(m_nPrfType == PROF_X)	{
	//	m_nRC = mod->GetCols();
	//	m_cs = mod->GetSizeX();
	//	str1 = "row ";
	//	n = m_nRow;
	//}
	//if(m_nPrfType == PROF_Y)	{
	//	m_nRC = mod->GetRows();
	//	m_cs = mod->GetSizeY();
	//	str1 = "column ";
	//	n = m_nCol;
	//}
	//m_xy0 = GetXYd(0);
	//m_plgPrev.RemoveAll();
	//m_plg.RemoveAll();
	//m_plgNext.RemoveAll();
	//m_plgPrev.SetSize( m_nRC, 0 );
	//m_plg.SetSize( m_nRC, 0 );
	//m_plgNext.SetSize( m_nRC, 0 );
	//m_bScale = TRUE;
	////Scale(bInit);
	////UpdatePlgAll();
	//CraeteGuideMarks();
	//// window title
	//str.Format("%d", n);
	//GetParent()->SetWindowText(GetDocument()->GetTitle()+ ": Profile -> " + str1 + str);
}

void CViewProfPlus::OnViewNextProfile()
{
	MoveProfile( 1 );
}

void CViewProfPlus::OnViewPreviousProfile()
{
	MoveProfile( -1 );
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////
// jump to profile on which extrem is located
void CViewProfPlus::OnProfileExtrem()
{
	//CGrid* pGrd;

	//pGrd = m_pDoc->GetGrid(m_nFldExtrem);
	//if( pGrd ) {
	//	int row, col;
	//	double min, max;
	//	pGrd->GetMinMaxValue(min, max);
	//	if( fabs(max) > fabs(min) ) 
	//		pGrd->GetRowColMax(row, col);
	//	else
	//		pGrd->GetRowColMin(row, col);
	//	if( m_nPrfType == PROF_X )
	//		MoveProfileTo( row+1 );
	//	if( m_nPrfType == PROF_Y )
	//		MoveProfileTo( col+1 );
	//}
}

void CViewProfPlus::OnUpdateProfileExtrem(CCmdUI *pCmdUI)
{
	if( m_nFldExtrem == -1 ) pCmdUI->Enable( 0 );
	else					pCmdUI->Enable( 1 );
}

// jump to profile on which mimimum is located
void CViewProfPlus::OnProfileJumptomax()
{
	CGrid* pGrd;

	pGrd = m_pDoc->GetGrid(m_nFldExtrem);
	if( pGrd ) {
		int row, col;
		pGrd->GetRowColMax(row, col);
		if( m_nPrfType == PROF_X )
			MoveProfileTo( row+1 );
		if( m_nPrfType == PROF_Y )
			MoveProfileTo( col+1 );
	}
}

void CViewProfPlus::OnUpdateProfileJumptomax(CCmdUI *pCmdUI)
{
	if( m_nFldExtrem == -1 ) pCmdUI->Enable( 0 );
	else					pCmdUI->Enable( 1 );
}

void CViewProfPlus::OnProfileJumptomin()
{
	CGrid* pGrd;

	pGrd = m_pDoc->GetGrid(m_nFldExtrem);
	if( pGrd ) {
		int row, col;
		pGrd->GetRowColMin(row, col);
		if( m_nPrfType == PROF_X )
			MoveProfileTo( row+1 );
		if( m_nPrfType == PROF_Y )
			MoveProfileTo( col+1 );
	}
}

void CViewProfPlus::OnUpdateProfileJumptomin(CCmdUI *pCmdUI)
{
	if( m_nFldExtrem == -1 ) pCmdUI->Enable( 0 );
	else					pCmdUI->Enable( 1 );
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////


void CViewProfPlus::OnProfileSaveCurrentSettings()
{
	CDlgProfileSettings dlg;

	dlg.m_pView = this;
	dlg.m_pPrfSetArray = m_pDoc->GetProfViewSettingsArray();

	dlg.m_prfSet.bAxis = m_bAxis;
	for(int i=0; i<60; i++) {
		dlg.m_prfSet.bf[i] = m_bf[i];
	}
	dlg.m_prfSet.bProfNext = m_bProfNext;
	dlg.m_prfSet.bProfPrev = m_bProfPrev;
	dlg.m_prfSet.bScaleEq = m_bScaleEq;
	dlg.m_prfSet.nCol = m_nCol;
	dlg.m_prfSet.nFldExtrem = m_nFldExtrem;
	dlg.m_prfSet.nFldTbl = m_nFldTbl;
	dlg.m_prfSet.nPrfType = m_nPrfType;
	dlg.m_prfSet.nRow = m_nRow;
	strcpy(dlg.m_prfSet.strSettingsName, "Enter Description");
	dlg.m_strName = "Enter Description";

	dlg.DoModal();
}

void CViewProfPlus::LoadProfileSettings(PRFVIEWSET profSet)
{
	m_bAxis = profSet.bAxis;
	for(int i=0; i<60; i++) {
		m_bf[i] = profSet.bf[i];
	}
	m_bProfNext = profSet.bProfNext;
	m_bProfPrev = profSet.bProfPrev;
	m_bScaleEq = profSet.bScaleEq;
	m_nCol = profSet.nCol;
	m_nFldExtrem = profSet.nFldExtrem;
	m_nFldTbl = profSet.nFldTbl;
	m_nPrfType = profSet.nPrfType;
	m_nRow = profSet.nRow;

	if(m_nPrfType == PROF_X)
		MoveProfileTo(m_nRow);
	if(m_nPrfType == PROF_Y)
		MoveProfileTo(m_nCol);
}


