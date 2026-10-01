// 3dDefView.cpp : implementation of the CViewProf class
//

#include "stdafx.h"
#include "Mod3D.h"

#include <math.h>
#include "Mod3DDoc.h"
#include "ViewProf.h"
#include "GridObj.h"
#include "well.h"

#include "DlgFldSelect.h"
#include "DlgProfileSettings.h"
#include "DlgBodyMove.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


extern double g_x, g_y, g_z;
// CViewProf

IMPLEMENT_DYNCREATE(CViewProf, CView)

BEGIN_MESSAGE_MAP(CViewProf, CView)
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
//	ON_WM_SIZING()
//	ON_WM_MOVING()
	END_MESSAGE_MAP()

// CViewProf construction/destruction

CViewProf::CViewProf()
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
	m_bScaleEq = FALSE;

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

CViewProf::~CViewProf()
{
	m_dc.DeleteDC();
}


int CViewProf::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CView::OnCreate(lpCreateStruct) == -1)
		return -1;

	return 0;
}

void CViewProf::OnDestroy()
{
	CView::OnDestroy();

	// TODO: Add your message handler code here
}

BOOL CViewProf::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	return CView::PreCreateWindow(cs);
}

void CViewProf::OnInitialUpdate()
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



// CViewProf diagnostics

#ifdef _DEBUG
void CViewProf::AssertValid() const
{
	CView::AssertValid();
}

void CViewProf::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}

CMod3DDoc* CViewProf::GetDocument() const // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CMod3DDoc)));
	return (CMod3DDoc*)m_pDocument;
}
#endif //_DEBUG

// CViewProf message handlers

///////////////////////////////////////////////////////////////////////////////////////////
//******** cordinate transformations ******************************************************
void CViewProf::DPtoLP(CPoint& pt)
{
	CClientDC dc(this);
	OnPrepareDC( &dc );
	dc.DPtoLP( &pt );
}

void CViewProf::WPtoLP(CPoint &pt, int nIndex, int row, int col)
{
	double xw, zw;
	GetXZd(nIndex, row, col, xw, zw);
	pt.x = (int) (m_xOff + m_xSc*xw);
	pt.y = (int) (m_yOff + m_ySc*zw);
}

void CViewProf::WPtoLP(int& x, int& y, double xw, double yw)
{
	x = (int) (m_xOff + m_xSc*xw);
	y = (int) (m_yOff + m_ySc*yw);
}

void CViewProf::WPtoLP(CPoint &pt, double xw, double yw)
{
	pt.x = (int) (m_xOff + m_xSc*xw);
	pt.y = (int) (m_yOff + m_ySc*yw);
}

void CViewProf::WPtoDP(int& x, int& y, double xw, double yw)
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

void CViewProf::WPtoDP(CPoint& pt, double xw, double yw)
{
	pt.x = (int) (m_xOff + m_xSc*xw);
	pt.y = (int) (m_yOff + m_ySc*yw);
	CClientDC dc(this);
	OnPrepareDC( &dc );
	dc.LPtoDP( &pt );
}

void CViewProf::DPtoWP(CPoint pt, double &xw, double &yw)
{
	CClientDC dc(this);
	OnPrepareDC( &dc );
	dc.DPtoLP( &pt );
	
	xw = (double)(pt.x-m_xOff) / m_xSc;
	yw = (double)(pt.y-m_yOff) / m_ySc;
}

void CViewProf::LPtoWP(double& xw, double& yw, int xl, int yl)
{
	xw = (xl-m_xOff) / m_xSc;
	yw = (yl-m_yOff) / m_ySc;
}

double CViewProf::GetXYd(int n)
{
	if(m_nPrfType == PROF_X) 
		return mod->GetXd(n);
	if(m_nPrfType == PROF_Y) 
		return mod->GetYd(n);
	return 0;
}

void CViewProf::GetXZd(int nIndex, int row, int col, double& x, double& z)
{
	if(m_nPrfType == PROF_X)  
		mod->GetXZd(nIndex, row, col, x, z);
	if(m_nPrfType == PROF_Y)  
		mod->GetYZd(nIndex, row, col, x, z);
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

void CViewProf::OnFilePrintSetup()
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

BOOL CViewProf::OnPreparePrinting(CPrintInfo* pInfo)
{
	BOOL	bRet;

	m_rcPrn.left = m_rcPrnMargin.left;
	m_rcPrn.right = m_ptPaperSize.x - m_rcPrnMargin.right - m_rcPrnMargin.left;
	m_rcPrn.top = m_rcPrnMargin.top;
	m_rcPrn.bottom = m_ptPaperSize.y - m_rcPrnMargin.bottom - m_rcPrnMargin.top;

	bRet = DoPreparePrinting(pInfo);

	return bRet;
}

void CViewProf::OnBeginPrinting(CDC* /*pDC*/, CPrintInfo* pInfo)
{
}

void CViewProf::OnEndPrinting(CDC* /*pDC*/, CPrintInfo* /*pInfo*/)
{
	m_bScale = TRUE;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CViewProf drawing
void CViewProf::OnUpdate(CView* /*pSender*/, LPARAM lHint, CObject* /*pHint*/)
{
	switch( lHint ) {
		case UPDATE_NEW_MODEL: {
			//mod = m_pDoc->GetModel();
			//SetVariables();	 
			//m_bInit = TRUE;
			//m_bScale = TRUE;
			Invalidate();
			break;							   }
		case UPDATE_COMPUTATION_FINISHED:
			UpdatePlg(m_plg, mod->GetRow(), mod->GetCol());
			m_bScale = TRUE;
			Invalidate();
			break;
		case UPDATE_PROPERTIES_REDRAW:
			CreatePenBrush();
			m_bScale = TRUE;
			Invalidate();
			break;
		case UPDATE_MOVE_EDGE: 
			UpdatePlg(m_plg, mod->GetRow(), mod->GetCol());
			Invalidate();
			break;								  
		case UPDATE_PROPERTIES_GRID_COLOR: 
			break;								  
		default: 
			UpdatePlg(m_plg, mod->GetRow(), mod->GetCol());
			m_bScale = TRUE;
			Invalidate();
	}
}

void CViewProf::OnSize(UINT nType, int cx, int cy)
{
	CView::OnSize(nType, cx, cy);

	if(mod) 
		m_bScale = TRUE;
	
}

void CViewProf::OnPrepareDC(CDC* pDC, CPrintInfo* pInfo)
{
	pDC->SetMapMode( MM_HIMETRIC );
	pDC->SetBkMode( TRANSPARENT );

	CView::OnPrepareDC(pDC, pInfo);
}

BOOL CViewProf::OnEraseBkgnd(CDC* pDC)
{
	if( !mod ) {
		return CView::OnEraseBkgnd(pDC); // default implementation
	}
	return TRUE;
}

void CViewProf::OnDraw(CDC* pDC)
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

void CViewProf::DrawInit(CDC* pDC) 
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

void CViewProf::Draw(CDC* pDC)
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
	m_axFld.DrawGridLines(pDC, m_drwRectFld);

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

int CViewProf::GetMargin(CDC* pDC, CSize& szMargin)
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
			if( (axMin != min || axMax != max) && m_axFld.GetAuto() ) {
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

void CViewProf::Scale(CDC* pDC)
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
		m_drwRectFld.right = m_drwRect.right;
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

void CViewProf::ScaleFieldAxis(void)
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

void CViewProf::UpdateScale(void)
{
	m_axH.GetScOff(m_xSc, m_xOff);
	m_axV.GetScOff(m_ySc, m_yOff);
	m_xOff = m_axH.GetLeft() - m_xSc*m_axH.GetDataMin();
	m_yOff = m_drwRect.bottom - m_ySc*m_axV.GetDataMin();
}

void CViewProf::CreatePenBrush(void)
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

void CViewProf::DrawPlg(CDC* pDC)
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

void CViewProf::DrawRelief(CDC* pDC)
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

void CViewProf::UpdatePlgAll()
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

void CViewProf::UpdatePlg(ClmnPlgArray& plg, int row, int col, int startIncr, int endIncr, int nTag)
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

void CViewProf::DrawField(CDC* pDC)
{
	TRACE("Entering CViewProf::DrawField(CDC* pDC)\n");

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

					if( k==m_nFldAxis ) {		// field axis 
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
						if( (axMin != min || axMax != max) && m_axFld.GetAuto()) {
							m_axFld.SetMaxDataExtent( min, max);
							m_axFld.SetDataExtent( min, max);
						}
						m_axFld.GetScOff(sc, off);
						m_axFld.Draw(pDC);
						pDC->SetTextAlign(TA_TOP | TA_LEFT);
						pDC->TextOut(m_drwRectFld.left + pDC->GetTextExtent("  ").cx, m_drwRectFld.top, m_pDoc->GetFieldString(m_nFldAxis) );
					}
					else if( m_nFldAxis==k-40 && m_bAxRange) {		// field axis OR measured field axis
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
						if( (axMin != min || axMax != max) && m_axFld.GetAuto()) {
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
					else if(m_nFldAxis!=k-40) {
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

void CViewProf::DrawGrdLines(CDC* pDC)
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

void CViewProf::DrawProfileIntersections(CDC* pDC)
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
		if( pView->IsKindOf(RUNTIME_CLASS( CViewProf)) && pView != this) {
			CViewProf *pProfView = (CViewProf*) pView;
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

void CViewProf::DrawWells(CDC* pDC)
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
		if( pObj->IsKindOf( RUNTIME_CLASS( CData3D ) ) ) {
			CData3D* pD = (CData3D*) pObj;
			pD->DrawProfile( pDC, m_xSc, m_xOff, m_ySc, m_yOff, m_nPrfType, xy);
		}
	}
}

void CViewProf::DrawImages(CDC* pDC)
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

void CViewProf::DrawLeadingHorizon(CDC* pDC)
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

void CViewProf::DrawCheckMarks(CDC* pDC)
{
	TRACE("Entering DrawCheckMarks(CDC* pDC)\n");

	if( pDC->IsPrinting() ) return;

	for(int i=0; i<m_guideMarks.GetSize(); i++) {
		CGuideMark* pGm = &m_guideMarks.GetAt(i);
		pGm->Draw(pDC, m_xSc, m_xOff, m_ySc, m_yOff, mod->GetZMax());
	}
}

BOOL CViewProf::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	HCURSOR hCursor;

	switch( m_nCursor ) {
		case CRS_OVER_SCBAR_H:
			hCursor = AfxGetApp()->LoadCursor(IDC_CURSOR_SB_HRZ);
			break;
		case CRS_OVER_SCBAR_V:
			hCursor = AfxGetApp()->LoadCursor(IDC_CURSOR_SB_VRT);
			break;
		case CRS_OVER_SCBAR_FLD:
			hCursor = AfxGetApp()->LoadCursor(IDC_CURSOR_SB_VRT);
			break;
		case CRS_OVER_LINE:
			hCursor = AfxGetApp()->LoadCursor(IDC_CURSOR_LINE);
			break;
		case CRS_OVER_VERTEX:
			CBody* pBd;
			pBd = mod->GetBody(m_nIndexID);
			if(pBd != NULL) {
				if(pBd->IsLocked())
					hCursor = AfxGetApp()->LoadCursor(IDC_CURSOR_VRTX_LOCK);
				else
					hCursor = AfxGetApp()->LoadCursor(IDC_CURSOR_PT);
			}
			else
				hCursor = AfxGetApp()->LoadCursor(IDC_CURSOR_NORMAL);
			break;
		case CRS_OVER_BODY:
			hCursor = AfxGetApp()->LoadCursor(IDC_CURSOR_BD);
			break;
		case CRS_OVER_BODY_LINE:
			hCursor = AfxGetApp()->LoadCursor(IDC_CURSOR_BD_LINE);
			break;
		case CRS_OVER_DIV:
			hCursor = AfxGetApp()->LoadCursor(IDC_CURSOR_DIV);
			break;
		// zooming
		case CRS_ZOOM:
			hCursor = AfxGetApp()->LoadCursor(IDC_CURSOR_ZOOM);
			break;
		case CRS_ZOOM_IN:
			hCursor = AfxGetApp()->LoadCursor(IDC_CURSOR_ZOOM_IN);
			break;
		case CRS_ZOOM_OUT:
			hCursor = AfxGetApp()->LoadCursor(IDC_CURSOR_ZOOM_OUT);
			break;

		// drawing rectangle
		case CRS_OVER_DIV_RIGHT:
			hCursor = AfxGetApp()->LoadCursor(IDC_CURSOR_DIV_VRT);
			break;
		case CRS_OVER_DIV_LEFT:
			hCursor = AfxGetApp()->LoadCursor(IDC_CURSOR_DIV_VRT);
			break;
		case CRS_OVER_DIV_TOP:
			hCursor = AfxGetApp()->LoadCursor(IDC_CURSOR_DIV);
			break;
		case CRS_OVER_DIV_BOTTOM:
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




////////////////////////////////////////////////////////////////////////////////////////////
//******** mouse routines ******************************************************************
void CViewProf::OnMouseMove(UINT nFlags, CPoint point)
{
	if( !mod ) return;

	// use the semaphore to enter this critic section
	static BOOL 	bBusy = FALSE;
	if(bBusy) return;
	bBusy = TRUE;

	CClientDC dc(this);
	OnPrepareDC(&dc);
	CPoint lPt = point;	// point in logical coordinates
	dc.DPtoLP(&lPt);
	int		div;
	div = (int) ((m_drwRect.top+m_drwRectFld.bottom)/2.0f);
	CPoint	ptDiv(0,div);
	dc.LPtoDP(&ptDiv);
	div = ptDiv.y;

	if(  div-2 < point.y && point.y < div+2 && !m_bScaleEq) {
		m_nCursor = CRS_OVER_DIV;
		bBusy = FALSE;
		return;
	}

	if( IsCursorOverDiv( point ) ) {
		bBusy = FALSE;
		return;
	}

	FindIndex( point );

	if( GetCapture() == this ) {
		MouseMoveCaptured(nFlags, point);
		bBusy = FALSE;
		return;
	}

	if( IsCursorOverScaleBar( lPt ) )	{
		bBusy = FALSE;
		return;	}
	if( IsCursorOverLine( point, m_nRow, m_nCol ) )	{
		bBusy = FALSE;
		return;	}

	// default
	m_nCursor = CRS_NORMAL;
	m_cpt.nTag = CPT_NONE;
	bBusy = FALSE;
	//CView::OnMouseMove(nFlags, point);
}

void CViewProf::GetRowCol(int &row, int &col, int nPrfIndex) 
{
	if( m_nPrfType == PROF_X ) {
		row = m_nRow; 
		col = nPrfIndex;
	}
	if( m_nPrfType == PROF_Y ){
		row = nPrfIndex;
		col = m_nCol;
	}
}

void CViewProf::MouseMoveCaptured(UINT nFlags, CPoint point)
{
	CPoint	lPt=point;
	CRect	clRc;
	int		div;

	// scale bars
	GetClientRect(&clRc);
	DPtoLP(lPt);
	switch( m_cpt.nTag ) {
		case CPT_MOVE_DIV:
			MoveDiv(point);
			return;
		case CPT_MOVE_SB_HRZ:
            m_axH.Move(lPt);
			UpdateScale();
			UpdatePlgAll();
			Invalidate();
			return;
		case CPT_MOVE_SB_VRT:
			m_axV.Move(lPt);
			UpdateScale();
			UpdatePlgAll();
			Invalidate();
			return;
		case CPT_MOVE_SB_FLD:
			m_axFld.Move(lPt);
			UpdateScale();
			UpdatePlgAll();
			Invalidate();
			return;
		case CPT_MOVE_POINT:
			MovePoint(point, nFlags);
			return;
		case CPT_MOVE_BODY_V:
			MoveBody(point, nFlags);
			return;
		case CPT_MOVE_EDGE:
			MoveEdge(point, nFlags);
			return;
		case CPT_ZOOM:
			if( m_bZoomig ) {
				m_zoomRect.right = point.x;
				m_zoomRect.bottom = point.y;
				Invalidate();
			}
			return;
		case CPT_MOVE_DIV_BOTTOM:
			div = clRc.bottom-point.y;
			if( div > 10 && point.y > m_szLeftTop.cy) {
				m_szRightBot.cy = div;
				m_bScale = TRUE;
				Invalidate();
			}
			return;
		case CPT_MOVE_DIV_LEFT:
			div = point.x;
			if( div > 10 && point.x < clRc.right-m_szRightBot.cx ) {
				m_szLeftTop.cx = div;
				m_bScale = TRUE;
				Invalidate();
			}
			return;
		case CPT_MOVE_DIV_RIGHT:
			//div = clRc.right-point.x;
			//if( div > 10 && point.x > m_szLeftTop.cx) {
			//	m_szRightBot.cx = div;
			//	Scale();
			//	Invalidate();
			//}
			return;
		case CPT_MOVE_DIV_TOP:
			//div = point.y;
			//if( div > 10 && point.y < clRc.bottom-m_szRightBot.cy) {
			//	m_szLeftTop.cy = div;
			//	Scale();
			//	Invalidate();
			//}
			return;
	}
}

void CViewProf::MoveDiv(CPoint point)
{
	CRect	clRc;
	int		div;
	GetClientRect(&clRc);
	div = (clRc.bottom-clRc.top);
	m_fResize = (float)point.y/div;
	if( m_fResize < 0.1)	m_fResize = 0.1f;
	if( m_fResize > 0.9)	m_fResize = 0.9f;

	m_bScale = TRUE;
	// Scale();
	//UpdatePlgAll();
	
	Invalidate();
}

int CViewProf::MovePoint(CPoint point, int nFlags)
{
	if( !m_pDoc->IsEnabled(MODF_MODEL) ) {
		ReleaseCapture();
		m_cpt.nBodyID = -1;
		AfxMessageBox("Model modification is disabled, computation is running.", MB_OK | MB_ICONINFORMATION);
		return 0;
	}

	double xw=0, zw=0;

	DPtoWP( point, xw, zw );
	if(m_nPrfIndex+1 == m_cpt.nPrfIndexOld || m_nPrfIndex-1 == m_cpt.nPrfIndexOld ) {
		mod->UpdateMoveIndex(m_nIndex, m_cpt.nBodyIndex, m_nRow, m_nCol, m_cpt.nBodyID);
		m_cpt.nPrfIndexOld = m_nPrfIndex;
		m_cpt.nBodyIndex = m_nIndex;
	}
	if( mod->MoveVertex( m_nIndex, m_nRow, m_nCol, zw, nFlags) == -1 ) {
		ReleaseCapture();
		return 0;
	}
	UpdatePlg(m_plg, m_nRow, m_nCol);
	Invalidate(FALSE);

	m_pDoc->SetModifiedFlag();

	CVrtxMove update;
	update.nBodyID = m_cpt.nBodyID;
	update.nRow = m_nRow; 
	update.nCol = m_nCol;
	update.nIndex = m_nIndex;
	update.dNewZ = DUMMY;
	m_pDoc->UpdateAllViews( this, UPDATE_MOVE_VERTEX, &update);

	m_pDoc->UpdateField();

	return 0;
}

int CViewProf::MoveEdge(CPoint point, int nFlags)
{
	if( !m_pDoc->IsEnabled(MODF_MODEL) ) {
		ReleaseCapture();
		m_cpt.nBodyID = -1;
		AfxMessageBox("Model modification is disabled, computation is running.", MB_OK | MB_ICONINFORMATION);
		return 0;
	}

	int row, col;
	double xw=0, zw=0;
	DPtoWP( point, xw, zw );
	WPtoLP( m_bdCreatPoly[2], xw, zw);
	m_bdCreatPoly[3] = m_bdCreatPoly[0];
	if(m_nPrfIndex+1 == m_cpt.nPrfIndexOld || m_nPrfIndex-1 == m_cpt.nPrfIndexOld ) {
		// create new edge
		if( IsCursorOverLine(point, m_nRow, m_nCol) == CRS_OVER_LINE ) {
			GetRowCol(row, col, m_cpt.nPrfIndexOld);
			double t = mod->GetThicknesNew(row, col, m_cpt.nBodyIndex);
			m_nIndex = mod->InsertBody(m_nRow, m_nCol, zw, t/2, FALSE, m_cpt.nBodyID);
			WPtoLP( m_bdCreatPoly[0], m_nIndex, m_nRow, m_nCol);
			WPtoLP( m_bdCreatPoly[1], m_nIndex+1, m_nRow, m_nCol);
			m_cpt.nPrfIndexOld = m_nPrfIndex;
			m_cpt.nBodyIndex = m_nIndex;
			CEdgeMove edgMove;
			edgMove.nBodyID = m_cpt.nBodyID;
			edgMove.nRow = m_nRow;
			edgMove.nCol = m_nCol;
			edgMove.nIndexTop = m_nIndex;
			edgMove.nIndexBot = m_nIndex+1;
			edgMove.nTag = 1;
			m_pDoc->UpdateAllViews( NULL, UPDATE_MOVE_EDGE, &edgMove );
			m_pDoc->UpdateField();
			//if( !m_pDoc->UpdateField() )
			//	m_pDoc->UpdateAllViews( NULL, UPDATE_MOVE_EDGE );
			m_pDoc->SetModifiedFlag();
			return 1;
		}

		// destroy edge
		if( (m_nCursor == CRS_OVER_BODY_LINE) || (m_nCursor == CRS_OVER_VERTEX) ) {
			if( mod->GetID(m_nIndex, m_nRow, m_nCol) == m_cpt.nBodyID  && m_cpt.nBodyID != -1) {
				GetRowCol(row, col, m_cpt.nPrfIndexOld);
				if( mod->GetID(m_cpt.nBodyIndex, row, col) == m_cpt.nBodyID ) {
					mod->RemoveBody(m_cpt.nBodyIndex, row, col);
					WPtoLP( m_bdCreatPoly[0], m_nIndex, m_nRow, m_nCol);
					WPtoLP( m_bdCreatPoly[1], m_nIndex+1, m_nRow, m_nCol);
					m_cpt.nPrfIndexOld = m_nPrfIndex;
					m_cpt.nBodyIndex = m_nIndex;
					CEdgeMove edgMove;
					edgMove.nBodyID = m_cpt.nBodyID;
					edgMove.nRow = m_nRow;
					edgMove.nCol = m_nCol;
					edgMove.nIndexTop = m_nIndex;
					edgMove.nIndexBot = m_nIndex+1;
					edgMove.nTag = 0;
					m_pDoc->UpdateAllViews( NULL, UPDATE_MOVE_EDGE, &edgMove );
					m_pDoc->UpdateField();
					//if( !m_pDoc->UpdateField() )
					//	m_pDoc->UpdateAllViews( NULL, UPDATE_MOVE_EDGE );
					m_pDoc->SetModifiedFlag();
					return 1;
				}
			}
			AfxMessageBox("No, no...");
			ReleaseCapture();
			m_cpt.nBodyID = -1;
		}
	}

	if(m_nPrfIndex < m_cpt.nPrfIndexOld-1 || m_nPrfIndex > m_cpt.nPrfIndexOld+1 ) {
		ReleaseCapture();
		m_cpt.nBodyID = -1;
	}
	Invalidate();
	m_pDoc->SetModifiedFlag();

	return 0;
}

void CViewProf::CopyProfile(int nIncr)
{
	int id, nFrom, nTo, nTag;

	if( m_nPrfType == PROF_X ) {
		nFrom = m_nRow; 
		nTo = m_nRow+nIncr;
		nTag = PRF_ROW;
	}
	if( m_nPrfType == PROF_Y ){
		nFrom = m_nCol;
		nTo = m_nCol+nIncr;
		nTag = PRF_COL;
	}

	id = m_nIndexID;
	mod->CopyBody(id, nFrom, nTo, nTag);
	m_pDoc->UpdateAllViews( this, UPDATE_COPY_PROFILE );
	if( !m_pDoc->UpdateField() )
		m_pDoc->UpdateAllViews(NULL);
}

void CViewProf::InsertBody(CPoint pt, int nTag)
{
	double xw=0, zw=0;
	DPtoWP( pt, xw, zw );
	switch( nTag ) {
		case INSBD_NEW:
			mod->InsertBody( m_nRow, m_nCol, zw );
			break;
		case INSBD_EXISTING:
			mod->InsertBody( m_nRow, m_nCol, zw, -1.0, FALSE );
			break;
	}
	m_pDoc->UpdateAllViews( this, UPDATE_INSERT_BODY );
	if( !m_pDoc->UpdateField() )
		m_pDoc->UpdateAllViews( NULL, UPDATE_MOVE_EDGE );
}

void CViewProf::RemoveBody(void)
{
	mod->RemoveBody(m_nIndex, m_nRow, m_nCol);
	m_pDoc->UpdateAllViews( this, UPDATE_REMOVE_BODY );
	if( !m_pDoc->UpdateField() )
		m_pDoc->UpdateAllViews( NULL, UPDATE_MOVE_EDGE );
}

void CViewProf::RemoveBodyFromProfile(void)
{
	int nFrom;

	// row OR column view
	if( m_nPrfType == PROF_X ) {
		nFrom = m_nRow;
	}
	if( m_nPrfType == PROF_Y ) {
		nFrom = m_nCol;
	}
	mod->RemoveBodyFromProfile(m_nIndexID, nFrom, m_nPrfType);
	m_pDoc->UpdateAllViews( this, UPDATE_REMOVE_BODY_FROM_PROF );
	m_pDoc->UpdateField();
	//if( !m_pDoc->UpdateField() )
	//	m_pDoc->UpdateAllViews( NULL, UPDATE_PROPERTIES_REDRAW );
}

void CViewProf::FindIndex(CPoint pt)
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

BOOL CViewProf::IsPointInPolygon(CPoint pt, CPoint* plg, int n)
{
	// pt - point
	// plg - polygon in the same coordinates as pt !!!
	// n - number of polygon points
	double x, y, x1, x2, y1, y2;
	BOOL	bIn = false;

	x = pt.x;
	y = pt.y;
	// check wheather point is inside polygon
	x1 = plg[0].x;
	y1 = plg[0].y;
	for( int j=0; j<n; j++) {
		x2 = plg[j].x;
		y2 = plg[j].y;
		if (y1 < y && y2 >= y  ||  y2 < y && y1 >= y) {
			if( x1 + (double)(y-y1)/(double)(y2 - y1)*(x2 - x1) < x ) {
				bIn = !bIn; 
			}
		}
		x1 = x2;
		y1 = y2;
	} //for (over polygon)
	if( bIn ) {
		return TRUE;
	}

	return FALSE; 
}

int CViewProf::IsCursorOverDiv(CPoint pt)
{
	// legend division
	CRect	clRc;
	GetClientRect(&clRc);
	int div;

	div = clRc.bottom - m_szRightBot.cy;
	if(  div+2 < pt.y && pt.y < div+5 ) {
		m_nCursor = CRS_OVER_DIV_BOTTOM;
		return 1;
	}

	div = m_szLeftTop.cx;
	if(  div-5 < pt.x && pt.x < div-2 ) {
		m_nCursor = CRS_OVER_DIV_LEFT;
		return 1;
	}

	//int	div = clRc.right - m_szRightBot.cx;
	//if(  div-3 < pt.x && pt.x < div+3 ) {
	//	m_nCursor = CRSM_OVER_DIV_RIGHT;
	//	return 1;
	//}

	//div = m_szLeftTop.cy;
	//if(  div-3 < pt.y && pt.y < div+3 ) {
	//	m_nCursor = CRSM_OVER_DIV_TOP;
	//	return 1;
	//}

	return 0;
}

int CViewProf::IsCursorOverScaleBar(CPoint &pt)
{
	if( m_axH.IsClicked(pt) )	{
		m_nCursor = CRS_OVER_SCBAR_H;
		return CRS_OVER_SCBAR_H;
	}
	if( m_axV.IsClicked(pt) )	{
		m_nCursor = CRS_OVER_SCBAR_V;
		return CRS_OVER_SCBAR_V;
	}
	if( m_axFld.IsClicked(pt) )	{
		m_nCursor = CRS_OVER_SCBAR_FLD;
		return CRS_OVER_SCBAR_V;
	}
	return FALSE;
}

int CViewProf::IsCursorOverLine(CPoint &pt, int row, int col)
{
	double wx;
	CPoint ptD1, ptD2, ptD;
	CPoint ptB1, ptB2;
	int ex=2, ey=2;	// in points
	int n;

	if( m_nPrfType == PROF_X ) n=col;
	if( m_nPrfType == PROF_Y ) n=row;
	wx = GetXYd(n);						
	WPtoDP(ptD1, wx, mod->GetHell() );
	WPtoDP(ptD2, wx, mod->GetRelief(row, col) );
	if( ptD1.x-ex < pt.x && pt.x < ptD1.x+ex && ptD1.y > pt.y && pt.y >= ptD2.y )	{
		m_nCursor = CRS_OVER_LINE;
		// over point
		if( ! mod->IsEmpty(row, col) )	{
			for( int i=1; i<mod->GetCount(row, col)-1; i++ ) {
				WPtoDP(ptD, wx, mod->GetZ(row, col, i) );
				if( ptD.x-ex < pt.x && pt.x < ptD.x+ex && ptD.y-ey < pt.y && pt.y < ptD.y+ey )	{
					m_nIndex = i;
					m_nIndexID = mod->GetID(m_nIndex, row, col);
					m_nCursor = CRS_OVER_VERTEX;
					SetToolTip(CRS_OVER_VERTEX);
					return CRS_OVER_VERTEX;
				}
				WPtoDP(ptD, wx, mod->GetZ(row, col, i+1) );
				if( ptD.x-ex < pt.x && pt.x < ptD.x+ex && ptD.y-ey < pt.y && pt.y < ptD.y+ey )	{
					m_nIndex = i+1;
					m_nIndexID = mod->GetID(m_nIndex, row, col);
					m_nCursor = CRS_OVER_VERTEX;
					SetToolTip(CRS_OVER_VERTEX);
					return CRS_OVER_VERTEX;
				}
				if( (i % 2) == 1 ) {
					WPtoDP(ptB1, wx, mod->GetZ(row, col, i) );
					WPtoDP(ptB2, wx, mod->GetZ(row, col, i+1) );
					if( ptB1.x-ex < pt.x && pt.x < ptB1.x+ex && ptB1.y < pt.y && pt.y < ptB2.y )	{
						m_nIndex = i;
						m_nIndexID = mod->GetID(m_nIndex, row, col);
						m_nCursor = CRS_OVER_BODY_LINE;
						SetToolTip(CRS_OVER_BODY_LINE);
						return CRS_OVER_BODY_LINE;
					}
				}
			}
		}
		m_nIndex = -1;
		return CRS_OVER_LINE;
	}

	BODYPLG* pBplg;
	CPoint plg[5];
	CClientDC dc(this);
	OnPrepareDC(&dc);
	int l = (int)m_plg[m_nCrsIndex].GetSize();
	for(int i=0; i<l; i++) {
		pBplg = &m_plg[m_nCrsIndex].GetAt(i);
		for(int k=0; k<4;k++)
			plg[k] = pBplg->plg[k];
		plg[4] = pBplg->plg[0];
		LPtoDP(dc.GetSafeHdc(), plg, 5);
		if( IsPointInPolygon(pt, plg, 5) ) {
			m_nCursor = CRS_OVER_BODY;
			m_nIndexID = pBplg->id;
			SetToolTip(CRS_OVER_BODY);
			return CRS_OVER_BODY;
		}
	}

	m_nIndex = -1;
	m_nCursor = CRS_NORMAL;
	SetToolTip(CRS_NORMAL);

	return FALSE;
}


void CViewProf::OnLButtonDblClk(UINT nFlags, CPoint pt)
{
	if( nFlags == MK_LBUTTON && m_nCursor == CRS_OVER_LINE ) {
		InsertBody(pt);
		return;
	}

	if( m_nCursor == CRS_OVER_BODY_LINE && nFlags == (MK_LBUTTON | MK_CONTROL) ) {
		RemoveBody();
		return;
	}

	if( m_nCursor == CRS_OVER_LINE && nFlags == (MK_LBUTTON | MK_SHIFT) ) {
		InsertBody(pt, INSBD_EXISTING);
		//double	xw=0, zw=0;
		//DPtoWP( point, xw, zw );
		//mod->InsertBody(m_nRow, m_nCol, zw, -1.0, FALSE );
		//m_pDoc->UpdateAllViews( NULL, UPDATE_MOVE_EDGE );
		return;
	}

	CView::OnLButtonDblClk(nFlags, pt);
}

void CViewProf::OnLButtonDown(UINT nFlags, CPoint point)
{
	// fill structure
	m_cpt.nPrfIndexOld = m_nPrfIndex;
	m_cpt.nBodyID = mod->GetID( m_nIndex, m_nRow, m_nCol );
	m_cpt.nBodyIndex = m_nIndex;
	
	// zooming
	if( GetCapture() == this ) {
		switch(m_cpt.nTag) {
			case CPT_ZOOM_IN:
				ZoomIn(point);
				return;
			case CPT_ZOOM_OUT:
				ZoomOut(point);
				return;
			case CPT_ZOOM:
				if( !m_bZoomig ) {
					m_zoomRect.top = point.y; 
					m_zoomRect.left = point.x; 
					m_bZoomig = TRUE;
				}
				return;
		}
	}

	// move window division
	if( m_nCursor == CRS_OVER_DIV) {
		m_cpt.nTag = CPT_MOVE_DIV;
		SetCapture();
		return;
	}
	// move scale bar
	if( m_nCursor == CRS_OVER_SCBAR_H) {
		m_cpt.nTag = CPT_MOVE_SB_HRZ;
		SetCapture();
		return;
	}
	if( m_nCursor == CRS_OVER_SCBAR_V) {
		m_cpt.nTag = CPT_MOVE_SB_VRT;
		SetCapture();
		return;
	}
	if( m_nCursor == CRS_OVER_SCBAR_FLD) {
		m_cpt.nTag = CPT_MOVE_SB_FLD;
		SetCapture();
		return;
	}
	// move point
	if( m_nCursor == CRS_OVER_VERTEX) {
		m_cpt.nTag = CPT_MOVE_POINT;
		SetCapture();
		return;
	}
	// move point
	if( m_nCursor == CRS_OVER_BODY && nFlags==(MK_SHIFT|MK_LBUTTON) ) {
		m_cpt.nTag = CPT_MOVE_BODY_V;
		m_cpt.ptPrev.x = LONG_MAX;m_cpt.ptPrev.y = LONG_MAX;
		SetCapture();
		return;
	}

	if( m_nCursor == CRS_OVER_BODY_LINE ) {
		WPtoLP( m_bdCreatPoly[0], m_nIndex, m_nRow, m_nCol);
		WPtoLP( m_bdCreatPoly[1], m_nIndex+1, m_nRow, m_nCol);
		m_cpt.nTag = CPT_MOVE_EDGE;
		SetCapture();
		return;
	}

	if( m_nCursor == CRS_OVER_DIV_LEFT) {
		m_cpt.nTag = CPT_MOVE_DIV_LEFT;
		SetCapture();
		return;
	}

	if( m_nCursor == CRS_OVER_DIV_BOTTOM) {
		m_cpt.nTag = CPT_MOVE_DIV_BOTTOM;
		SetCapture();
		return;
	}

	CView::OnLButtonDown(nFlags, point);
}

void CViewProf::OnLButtonUp(UINT nFlags, CPoint point)
{
	if( GetCapture() == this ) {
		if( m_bZoomig )
			Zoom(point);
		m_cpt.nTag = CPT_NONE;
		ReleaseCapture();
		Invalidate();
	}

	CView::OnLButtonUp(nFlags, point);
}

BOOL CViewProf::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	if( zDelta > 0 ) MoveProfile( 1 );
	if( zDelta < 0 ) MoveProfile( -1 );

	return CView::OnMouseWheel(nFlags, zDelta, pt);
}

void CViewProf::OnRButtonDown(UINT nFlags, CPoint point)
{
	CView::OnRButtonDown(nFlags, point);

	BOOL	bInvalidate = FALSE;;
	switch( m_nCursor ) {
	   case CRS_OVER_LINE:
		   FloatMenuBodyLine(point);
		   break;
	   case CRS_OVER_BODY_LINE:
		   FloatMenuBody(point);
		   break;
	   case CRS_OVER_BODY:
		   FloatMenuBody(point);
		   break;
		case CRS_OVER_SCBAR_H:
			bInvalidate = m_axH.Properties();
			break;
		case CRS_OVER_SCBAR_V:
			bInvalidate = m_axV.Properties();
			break;
		case CRS_OVER_SCBAR_FLD:
			bInvalidate = m_axFld.Properties();
			break;
		case CRS_NORMAL:
			FloatMenuProfile(point);
			break;
		case CRS_OVER_VERTEX:
			FloatMenuVertex(point);
			break;
   }

	if(bInvalidate) {
		//Scale();
		m_bScale = TRUE;
		Invalidate();
	}
	CView::OnRButtonDown(nFlags, point);
}

void CViewProf::MoveProfile(int incr)
{
	int n;
	int nMax;

	if( m_nPrfType == PROF_X ) {
		n = m_nRow;
		nMax = mod->GetRows();
	}
	if( m_nPrfType == PROF_Y ) {
		n = m_nCol;
		nMax = mod->GetCols();
	}

	n += incr;
	if( n < 0 )			n=0;
	if( n > nMax-1)		n=nMax-1;

	if( m_nPrfType == PROF_X ) 
	{
		m_nRow = n;
		m_nRcIndex = n;
		g_y = mod->GetYe(m_nRow);
	}
	if( m_nPrfType == PROF_Y ) {
		m_nCol = n;
		m_nRcIndex = n;
		g_x = mod->GetXe(m_nCol);
	}

	SetVariables();
	// set indicators
	m_pDoc->SetIndicators(m_nRow-1, m_nCol-1);
	m_pDoc->UpdateAllViews( NULL, UPDATE_MOVE_PROFILE );
}

void CViewProf::MoveProfileTo(int nPrf)
{
	if( nPrf < 0 )			return;
	if( nPrf > m_nRC-1)		return;

	if( m_nPrfType == PROF_X ) 
	{
		m_nRow = nPrf;
		m_nRcIndex = nPrf;
		g_y = mod->GetYe(m_nRow);
	}
	if( m_nPrfType == PROF_Y ) {
		m_nCol = nPrf;
		m_nRcIndex = nPrf;
		g_x = mod->GetXe(m_nCol);
	}

	SetVariables();
	// set indicators
	m_pDoc->SetIndicators(m_nRow-1, m_nCol-1);
	m_pDoc->UpdateAllViews( NULL, UPDATE_MOVE_PROFILE );
}

/*	functions creates guide marks for given profile
	fills the variable m_ptGuideMark with the point in logical coorcinates for drawing
*/
int CViewProf::CraeteGuideMarks(void)
{
	TRACE("Entering CraeteGuideMarks(void) \n");

	double		xy;
	CPoint		pt;

	if( m_nPrfType == PRF_HRZ ) {
		xy = mod->GetYe(m_nRow);
	}
	if( m_nPrfType == PRF_VRT) {
		xy = mod->GetXe(m_nCol);
	}

	CMyObject *pObj;
	m_guideMarks.RemoveAll();
	for(int i=0; i<m_pDoc->GetObjArray()->GetSize(); i++) {
		pObj = m_pDoc->GetObjArray()->GetAt( i );
		if( pObj->IsKindOf( RUNTIME_CLASS( CGuideMark ) ) ) {
			CGuideMark* pGm = (CGuideMark*) pObj;
			pGm->FindIntersections(m_nPrfType, xy);
			if( !pGm->m_pt3DArrayProf.IsEmpty() ) {
				m_guideMarks.Add( *pGm );
			}
		}
	}
	return 0;
}

void CViewProf::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	switch(nChar) {
		case VK_UP:
			MoveProfile( 1 );
			break;
		case VK_DOWN:
			MoveProfile( -1 );
			break;
		case VK_NEXT:	// page down
			MoveProfile( -10 );
			break;
		case VK_PRIOR:	// page up
			MoveProfile( 10 );
			break;
		case VK_RIGHT:
			MoveProfile( 1 );
			break;
		case VK_LEFT:
			MoveProfile( -1 );
			break;
		case VK_ESCAPE:
			ReleaseCapture();
			m_nCursor = CRS_NORMAL;
			break;
		case 86:		// v
			OnProfileSN();
			break;
		case 72:		// h
			OnProfileEW();
			break;
		case 80:		// p	
			MoveProfile( -1 );
			break;
		case 78:		// n	
			MoveProfile( 1 );
			break;
		case 73:		// i - jump to min
			OnProfileJumptomin();
			break;
		case 69:		// e - jump to extrem
			OnProfileExtrem();
			break;
		case 65:		// a - jump to max
			OnProfileJumptomax();
			break;
		case 83:		// s - profile settings load/save
			OnProfileSaveCurrentSettings();
			break;
	}

	//CView::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CViewProf::OnProfileEW()
{
	m_nPrfType = PROF_X;
	m_nRow = m_nRowOld;
	m_nColOld = m_nCol;
	m_bInit = TRUE;
	SetVariables();
	m_pDoc->UpdateAllViews(this);
	m_pDoc->UpdateAllViews(this, UPDATE_MOVE_PROFILE);
}

void CViewProf::OnUpdateProfileEW(CCmdUI *pCmdUI)
{
	if( m_nPrfType == PROF_X )	pCmdUI->SetCheck(1);
	else						pCmdUI->SetCheck(0);
}

void CViewProf::OnProfileSN()
{
	m_nPrfType = PROF_Y;
	m_nCol = m_nColOld;
	m_nRowOld = m_nRow;
	SetVariables();
	m_bInit = TRUE;
	Invalidate(FALSE);
	m_pDoc->UpdateAllViews(this);
	m_pDoc->UpdateAllViews(this, UPDATE_MOVE_PROFILE);
}

void CViewProf::OnUpdateProfileSN(CCmdUI *pCmdUI)
{
	if( m_nPrfType == PROF_Y )	pCmdUI->SetCheck(1);
	else						pCmdUI->SetCheck(0);
}



void CViewProf::SetVariables()
{
	CString str1, str;
	int n;

	// init drawing variables
	if(m_nPrfType == PROF_X)	{
		m_nRC = mod->GetCols();
		m_cs = mod->GetSizeX();
		str1 = "row ";
		n = m_nRow;
	}
	if(m_nPrfType == PROF_Y)	{
		m_nRC = mod->GetRows();
		m_cs = mod->GetSizeY();
		str1 = "column ";
		n = m_nCol;
	}
	m_xy0 = GetXYd(0);
	m_plgPrev.RemoveAll();
	m_plg.RemoveAll();
	m_plgNext.RemoveAll();
	m_plgPrev.SetSize( m_nRC, 0 );
	m_plg.SetSize( m_nRC, 0 );
	m_plgNext.SetSize( m_nRC, 0 );
	m_bScale = TRUE;
	//Scale(bInit);
	//UpdatePlgAll();
	CraeteGuideMarks();
	// window title
	str.Format("%d", n);
	GetParent()->SetWindowText(GetDocument()->GetTitle()+ ": Profile -> " + str1 + str);
}

void CViewProf::OnViewNextProfile()
{
	MoveProfile( 1 );
}

void CViewProf::OnViewPreviousProfile()
{
	MoveProfile( -1 );
}



////////////////////////////////////////////////////////////////////////////////////////////
// floating menu
void CViewProf::FloatMenuBody(CPoint pt)
{
	CBody* pBody;
	if( m_nCursor == CRS_OVER_BODY_LINE )	pBody = mod->GetBody(m_nIndex, m_nRow, m_nCol);
	if( m_nCursor == CRS_OVER_BODY )		pBody = mod->GetBody(m_nIndexID);
	if( !pBody )	return;

	CMenu bodyMenu;
	CRect wndRect;
	this->GetWindowRect(wndRect);
	bodyMenu.LoadMenu(IDR_MENU_FLOAT_BODY);
	CMenu* subMenu = bodyMenu.GetSubMenu(0);
	int nEnabled;
	int nChecked;

	// Propreties
	if( m_nCursor == CRS_OVER_BODY_LINE || m_nCursor == CRS_OVER_BODY) nEnabled = MF_ENABLED; else nEnabled = MF_GRAYED;
	subMenu->EnableMenuItem( ID_BODY_PROPERTIES, nEnabled );
	// Fill
	if( pBody->IsFilled() ) nChecked = MF_CHECKED; else nChecked = MF_UNCHECKED;
	subMenu->CheckMenuItem( ID_BODY_FILL, nChecked );
	// Insert existing
	if( m_nCursor == CRS_OVER_BODY_LINE) nEnabled = MF_ENABLED; else nEnabled = MF_GRAYED;
	subMenu->EnableMenuItem( ID_BODY_INSERT_EXISTING, nEnabled);
	// insert new
	if( m_nCursor == CRS_OVER_BODY_LINE) nEnabled = MF_ENABLED; else nEnabled = MF_GRAYED;
	subMenu->EnableMenuItem( ID_BODY_INSERT_NEW, nEnabled);

	// Copy to Previous Profile
	// Copy to Next Profile
	// Remove
	if( m_nCursor == CRS_OVER_BODY_LINE ) nEnabled = MF_ENABLED; else nEnabled = MF_GRAYED;
	subMenu->EnableMenuItem( ID_BODY_REMOVE, nEnabled );
	// Remove from Profile
	// Edit Bodies
	if( m_nCursor == CRS_OVER_BODY || m_nCursor == CRS_OVER_BODY_LINE ) nEnabled = MF_ENABLED; else nEnabled = MF_GRAYED;
	subMenu->EnableMenuItem( ID_BODY_EDIT, nEnabled );
	// Body Creation Properties

	m_ptFloatMenu = pt;
	//m_nCursor = CRS_NORMAL;
	subMenu->TrackPopupMenu(TPM_LEFTALIGN |TPM_RIGHTBUTTON , wndRect.left+pt.x, wndRect.top+pt.y , this);
}

void CViewProf::FloatMenuBodyLine(CPoint pt)
{
	CMenu bodyMenu;
	CRect wndRect;
	this->GetWindowRect(wndRect);
	bodyMenu.LoadMenu(IDR_MENU_FLOAT_BODY_LINE);
	CMenu* subMenu = bodyMenu.GetSubMenu(0);
	int nEnabled;

	// Insert New Body
	if( m_nCursor == CRS_OVER_LINE ) nEnabled = MF_ENABLED; else nEnabled = MF_GRAYED;
	subMenu->EnableMenuItem( ID_BODY_INSERT_NEW, nEnabled);

	// Insert Existing Body
	if( m_nCursor == CRS_OVER_LINE ) nEnabled = MF_ENABLED; else nEnabled = MF_GRAYED;
	subMenu->EnableMenuItem( ID_BODY_INSERT_EXISTING, nEnabled);

	// edit bodies
	// Body Creation Properties

	m_ptFloatMenu = pt;
	subMenu->TrackPopupMenu(TPM_LEFTALIGN |TPM_RIGHTBUTTON , wndRect.left+pt.x, wndRect.top+pt.y , this);
}

void CViewProf::FloatMenuProfile(CPoint pt)
{
	CMenu bodyMenu;
	CRect wndRect;
	this->GetWindowRect(wndRect);
	bodyMenu.LoadMenu(IDR_MENU_FLOAT_PROF);
	CMenu* subMenu = bodyMenu.GetSubMenu(0);

	int nEnabled;
	int nChecked;

	// jumps to extrems
	if( m_nFldExtrem == -1 ) nEnabled = MF_GRAYED; else nEnabled = MF_ENABLED;
	subMenu->EnableMenuItem( ID_PROFILE_EXTREM, nEnabled );
	subMenu->EnableMenuItem( ID_PROFILE_JUMPTOMIN, nEnabled );
	subMenu->EnableMenuItem( ID_PROFILE_JUMPTOMAX, nEnabled );

	// go to previos profile
	if( m_nRcIndex == 0) nEnabled = MF_GRAYED; else nEnabled = MF_ENABLED;
	subMenu->EnableMenuItem( ID_VIEW_PREVIOUS_PROFILE, nEnabled );
	// go to next profile
	if( m_nRcIndex == m_nRC-1) nEnabled = MF_GRAYED; else nEnabled = MF_ENABLED;
	subMenu->EnableMenuItem( ID_VIEW_NEXT_PROFILE, nEnabled );

	// east-west profile
	if( m_nPrfType == PROF_X) nEnabled = MF_GRAYED; else nEnabled = MF_ENABLED;
	subMenu->EnableMenuItem( ID_PROFILE_EW, nEnabled );
	// south-north profile
	if( m_nPrfType == PROF_Y) nEnabled = MF_GRAYED; else nEnabled = MF_ENABLED;
	subMenu->EnableMenuItem( ID_PROFILE_SN, nEnabled );
	// show next profile
	if( m_bProfNext ) nChecked = MF_CHECKED; else nChecked = MF_UNCHECKED;
	subMenu->CheckMenuItem( ID_PROFILE_NEXT_SHOW, nChecked );
	// show previous profile
	if( m_bProfPrev ) nChecked = MF_CHECKED; else nChecked = MF_UNCHECKED;
	subMenu->CheckMenuItem( ID_PROFILE_PREVIOUS_SHOW, nChecked );

	// float menu
	m_ptFloatMenu = pt;
	subMenu->TrackPopupMenu(TPM_LEFTALIGN |TPM_RIGHTBUTTON , wndRect.left+pt.x, wndRect.top+pt.y , this);
}

void CViewProf::OnBodyInsertNew()
{
	if( m_nCursor == CRS_OVER_LINE ) {
		InsertBody(m_ptFloatMenu);
		//double xw=0, zw=0;
		//DPtoWP( m_ptFloatMenu, xw, zw );
		//mod->InsertBody( m_nRow, m_nCol, zw );
		//m_pDoc->UpdateAllViews( NULL, UPDATE_MOVE_EDGE );
	}
}

void CViewProf::OnBodyInsertExisting()
{
	if( m_nCursor == CRS_OVER_LINE ) {
		InsertBody( m_ptFloatMenu, INSBD_EXISTING );
		//double	xw=0, zw=0;
		//DPtoWP( m_ptFloatMenu, xw, zw );
		//mod->InsertBody(m_nRow, m_nCol, zw, -1.0, FALSE );
		//m_pDoc->UpdateAllViews( NULL, UPDATE_MOVE_EDGE );
	}
}

void CViewProf::MoveBody(CPoint ptMoveTo, int nFlags)
{
	if( m_cpt.ptPrev.x==LONG_MAX || m_cpt.ptPrev.y==LONG_MAX)  {
		m_cpt.ptPrev.x = ptMoveTo.x;
		m_cpt.ptPrev.y = ptMoveTo.y;
		return;
	}
	if( m_cpt.ptPrev.x==ptMoveTo.x && m_cpt.ptPrev.y==ptMoveTo.y)  {
		return;
	}

	CPoint ptPrev, ptNow;
	ptPrev = m_cpt.ptPrev;
	double	xw, ywp, ywn, dz;

	ptNow = ptMoveTo;
	DPtoWP(ptPrev, xw, ywp);
	DPtoWP(ptNow, xw, ywn);

	dz = ywn-ywp;

	mod->MoveBody(m_nIndexID, dz, dz, BMF_BOT|BMF_TOP, FALSE);

	m_cpt.ptPrev.x = ptMoveTo.x;
	m_cpt.ptPrev.y = ptMoveTo.y;

	if( !m_pDoc->UpdateField() )
		m_pDoc->UpdateAllViews( NULL, UPDATE_MOVE_PROFILE );

}

void CViewProf::OnBodyMoveboby()
{
	CDlgBodyMove dlg;

	if( dlg.DoModal() == IDOK ) {
		if(!dlg.m_dShiftBot && !dlg.m_dShiftTop || (dlg.m_dShiftBot==0 && dlg.m_dShiftTop==0)) {
			AfxMessageBox("Nothing is checked OR shifts are 0! Canceling.", MB_OK | MB_ICONINFORMATION);
			return;
		}

		int nFlag=BMF_NULL;
		if(dlg.m_bBot) nFlag = BMF_BOT;
		if(dlg.m_bTop) nFlag |= BMF_TOP;
		BOOL bAbs = dlg.m_nTag;

		mod->MoveBody(m_nIndexID, dlg.m_dShiftTop, dlg.m_dShiftBot, nFlag, bAbs);
		if( !m_pDoc->UpdateField() )
			m_pDoc->UpdateAllViews( NULL, UPDATE_MOVE_PROFILE );

	}
}
void CViewProf::OnUpdateBodyMoveboby(CCmdUI *pCmdUI)
{
	// TODO: Add your command update UI handler code here
}

void CViewProf::OnBodyRemove()
{
	RemoveBody();
}

void CViewProf::OnBodyCopyToNextProfile()
{
	CopyProfile( 1 );
}

void CViewProf::OnBodyCopyToPreviousProfile()
{
	CopyProfile( -1 );
}

void CViewProf::OnBodyEdit()
{
	m_pDoc->EditBodies();
}

void CViewProf::OnBodyShow()
{
	CBody* pBody = mod->GetBody(m_nIndexID);
	if( pBody ) {
		pBody->ShowInv();
		m_pDoc->UpdateAllViews( NULL, UPDATE_PROPERTIES_REDRAW );
	}
}

void CViewProf::OnBodyActive()
{
	CBody* pBody = mod->GetBody(m_nIndexID);
	if( pBody ) {
		pBody->ActivateInv();
		m_pDoc->UpdateAllViews( NULL, UPDATE_PROPERTIES_REDRAW );
	}
}

void CViewProf::OnBodyFill()
{
	CBody* pBody = mod->GetBody(m_nIndexID);
	if( pBody ) {
		pBody->FillInv();
		m_pDoc->UpdateAllViews( NULL, UPDATE_PROPERTIES_REDRAW );
	}
}

void CViewProf::OnBodyLock()
{
	CBody* pBody = mod->GetBody(m_nIndexID);
	if( pBody ) {
		pBody->LockInv();
		m_pDoc->UpdateAllViews( NULL, UPDATE_PROPERTIES_REDRAW );
	}
}

void CViewProf::OnBodyProperties()
{
	mod->PropertiesBody(m_nIndexID);
	Invalidate();
}

void CViewProf::OnBodyInvertDensity()
{
	m_pDoc->InvertDensity(m_nIndexID);
}

void CViewProf::OnBodyDensityInversionProperties()
{
	m_pDoc->DensityInversionProperties();
}

void CViewProf::OnBodyRemoveFromProfile()
{
	RemoveBodyFromProfile();
}

void CViewProf::OnModelBodyCreationProperties()
{
	mod->CreationProperties();
}

void CViewProf::InitPenFld(void)
{
	m_penFld[GRDMOD_GX].CreatePen(PS_SOLID,	0, RGB(0,	128,	255) );
	m_penFld[GRDMOD_GY].CreatePen(PS_SOLID,	0, RGB(0,	0,		220) );
	m_penFld[GRDMOD_GZ].CreatePen(PS_SOLID,	0, RGB(0,	1,		160) );
	m_penFld[GRDMOD_G].CreatePen(PS_SOLID,	0, RGB(0,	58,		117) );
	m_penFld[GRDMOD_MX].CreatePen(PS_SOLID,	0, RGB(250,	210,	0) );
	m_penFld[GRDMOD_MY].CreatePen(PS_SOLID,	0, RGB(192,	128,	0) );
	m_penFld[GRDMOD_MZ].CreatePen(PS_SOLID,	0, RGB(160,	0,		0) );
	m_penFld[GRDMOD_M].CreatePen(PS_SOLID,	0, RGB(255, 0,		0) );
	m_penFld[GRDMOD_GXX].CreatePen(PS_DOT,	0, RGB(255,	128,	255) );
	m_penFld[GRDMOD_GYY].CreatePen(PS_DOT,	0, RGB(128,	128,	28) );
	m_penFld[GRDMOD_GZZ].CreatePen(PS_DOT,	0, RGB(0,	128,	64) );
	m_penFld[GRDMOD_GXY].CreatePen(PS_DOT,	0, RGB(0,	160,	0) );
	m_penFld[GRDMOD_GXZ].CreatePen(PS_DOT,	0, RGB(0,	160,	0) );
	m_penFld[GRDMOD_GYZ].CreatePen(PS_DOT,	0, RGB(0,	160,	0) );
	m_penFld[GRDMOD_MXX].CreatePen(PS_DOT,	0, RGB(160,	160,	160) );
	m_penFld[GRDMOD_MYY].CreatePen(PS_DOT,	0, RGB(160,	160,	160) );
	m_penFld[GRDMOD_MZZ].CreatePen(PS_DOT,	0, RGB(160,	160,	160) );
	m_penFld[GRDMOD_MXY].CreatePen(PS_DOT,	0, RGB(160,	160,	160) );
	m_penFld[GRDMOD_MXZ].CreatePen(PS_DOT,	0, RGB(160,	160,	160) );
	m_penFld[GRDMOD_MYZ].CreatePen(PS_DOT,	0, RGB(160,	160,	160) );

	m_penFld[GRDMES_GX].CreatePen(PS_SOLID,	0, RGB(0,	128,	255) );
	m_penFld[GRDMES_GY].CreatePen(PS_SOLID,	0, RGB(0,	0,		220) );
	m_penFld[GRDMES_GZ].CreatePen(PS_SOLID,	0, RGB(0,	0,		160) );
	m_penFld[GRDMES_G].CreatePen(PS_SOLID,	0, RGB(0,	58,		117) );
	m_penFld[GRDMES_MX].CreatePen(PS_SOLID,	0, RGB(250,	210,	0) );
	m_penFld[GRDMES_MY].CreatePen(PS_SOLID,	0, RGB(192,	128,	0) );
	m_penFld[GRDMES_MZ].CreatePen(PS_SOLID,	0, RGB(160,	0,		0) );
	m_penFld[GRDMES_M].CreatePen(PS_SOLID,	0, RGB(255, 0,		0) );
	m_penFld[GRDMES_GXX].CreatePen(PS_DOT,	0, RGB(255,	128,	255) );
	m_penFld[GRDMES_GYY].CreatePen(PS_DOT,	0, RGB(128,	128,	28) );
	m_penFld[GRDMES_GZZ].CreatePen(PS_DOT,	0, RGB(0,	128,	64) );
	m_penFld[GRDMES_GXY].CreatePen(PS_DOT,	0, RGB(0,	160,	0) );
	m_penFld[GRDMES_GXZ].CreatePen(PS_DOT,	0, RGB(0,	160,	0) );
	m_penFld[GRDMES_GYZ].CreatePen(PS_DOT,	0, RGB(0,	160,	0) );
	m_penFld[GRDMES_MXX].CreatePen(PS_DOT,	0, RGB(160,	160,	160) );
	m_penFld[GRDMES_MYY].CreatePen(PS_DOT,	0, RGB(160,	160,	160) );
	m_penFld[GRDMES_MZZ].CreatePen(PS_DOT,	0, RGB(160,	160,	160) );
	m_penFld[GRDMES_MXY].CreatePen(PS_DOT,	0, RGB(160,	160,	160) );
	m_penFld[GRDMES_MXZ].CreatePen(PS_DOT,	0, RGB(160,	160,	160) );
	m_penFld[GRDMES_MYZ].CreatePen(PS_DOT,	0, RGB(160,	160,	160) );

	m_penFld[GRDDIF_GX].CreatePen(PS_DOT,	0, RGB(0,	128,	255) );
	m_penFld[GRDDIF_GY].CreatePen(PS_DOT,	0, RGB(0,	0,		220) );
	m_penFld[GRDDIF_GZ].CreatePen(PS_DOT,	0, RGB(0,	0,		160) );
	m_penFld[GRDDIF_G].CreatePen(PS_DOT,	0, RGB(0,	58,		117) );
	m_penFld[GRDDIF_MX].CreatePen(PS_DOT,	0, RGB(250,	210,	0) );
	m_penFld[GRDDIF_MY].CreatePen(PS_DOT,	0, RGB(192,	128,	0) );
	m_penFld[GRDDIF_MZ].CreatePen(PS_DOT,	0, RGB(160,	0,		0) );
	m_penFld[GRDDIF_M].CreatePen(PS_DOT,	0, RGB(255, 0,		0) );
	m_penFld[GRDDIF_GXX].CreatePen(PS_DOT,	0, RGB(255,	128,	255) );
	m_penFld[GRDDIF_GYY].CreatePen(PS_DOT,	0, RGB(128,	128,	28) );
	m_penFld[GRDDIF_GZZ].CreatePen(PS_DOT,	0, RGB(0,	128,	64) );
	m_penFld[GRDDIF_GXY].CreatePen(PS_DOT,	0, RGB(0,	160,	0) );
	m_penFld[GRDDIF_GXZ].CreatePen(PS_DOT,	0, RGB(0,	160,	0) );
	m_penFld[GRDDIF_GYZ].CreatePen(PS_DOT,	0, RGB(0,	160,	0) );
	m_penFld[GRDDIF_MXX].CreatePen(PS_DOT,	0, RGB(160,	160,	160) );
	m_penFld[GRDDIF_MYY].CreatePen(PS_DOT,	0, RGB(160,	160,	160) );
	m_penFld[GRDDIF_MZZ].CreatePen(PS_DOT,	0, RGB(160,	160,	160) );
	m_penFld[GRDDIF_MXY].CreatePen(PS_DOT,	0, RGB(160,	160,	160) );
	m_penFld[GRDDIF_MXZ].CreatePen(PS_DOT,	0, RGB(160,	160,	160) );
	m_penFld[GRDDIF_MYZ].CreatePen(PS_DOT,	0, RGB(160,	160,	160) );
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//**************** end field control bar **********************************************************************
void CViewProf::OnViewFieldModelGx()						{ CheckModField(GRDMOD_GX);	}
void CViewProf::OnUpdateViewFieldModelGx(CCmdUI* pCmdUI)	{ UpdateViewFieldMod(pCmdUI, GRDMOD_GX);}

void CViewProf::OnViewFieldModelGy()						{ CheckModField(GRDMOD_GY);	}
void CViewProf::OnUpdateViewFieldModelGy(CCmdUI* pCmdUI)	{ UpdateViewFieldMod(pCmdUI, GRDMOD_GY); }

void CViewProf::OnViewFieldModelGz()						{ CheckModField(GRDMOD_GZ);	}
void CViewProf::OnUpdateViewFieldModelGz(CCmdUI* pCmdUI)	{ UpdateViewFieldMod(pCmdUI, GRDMOD_GZ); }

void CViewProf::OnViewFieldModelG()						{ CheckModField(GRDMOD_G);	} 
void CViewProf::OnUpdateViewFieldModelG(CCmdUI* pCmdUI)	{ UpdateViewFieldMod(pCmdUI, GRDMOD_G); }

void CViewProf::OnViewFieldModelMx()						{ CheckModField(GRDMOD_MX);	} 
void CViewProf::OnUpdateViewFieldModelMx(CCmdUI* pCmdUI)	{ UpdateViewFieldMod(pCmdUI, GRDMOD_MX); }

void CViewProf::OnViewFieldModelMy()						{ CheckModField(GRDMOD_MY);	} 
void CViewProf::OnUpdateViewFieldModelMy(CCmdUI* pCmdUI)	{ UpdateViewFieldMod(pCmdUI, GRDMOD_MY); }

void CViewProf::OnViewFieldModelMz()						{ CheckModField(GRDMOD_MZ);	} 
void CViewProf::OnUpdateViewFieldModelMz(CCmdUI* pCmdUI)	{ UpdateViewFieldMod(pCmdUI, GRDMOD_MZ); }

void CViewProf::OnViewFieldModelM()						{ CheckModField(GRDMOD_M);	} 
void CViewProf::OnUpdateViewFieldModelM(CCmdUI* pCmdUI)	{ UpdateViewFieldMod(pCmdUI, GRDMOD_M); }

// gradients
void CViewProf::OnModelTxx()								{ CheckModField(GRDMOD_GXX);	} 
void CViewProf::OnUpdateModelTxx(CCmdUI *pCmdUI)			{ UpdateViewFieldMod(pCmdUI, GRDMOD_GXX); }

void CViewProf::OnModelTxy()								{ CheckModField(GRDMOD_GXY);	} 
void CViewProf::OnUpdateModelTxy(CCmdUI *pCmdUI)			{ UpdateViewFieldMod(pCmdUI, GRDMOD_GXY); }

void CViewProf::OnModelTxz()								{ CheckModField(GRDMOD_GXZ);	} 
void CViewProf::OnUpdateModelTxz(CCmdUI *pCmdUI)			{ UpdateViewFieldMod(pCmdUI, GRDMOD_GXZ); }

void CViewProf::OnModelTyy()								{ CheckModField(GRDMOD_GYY);	} 
void CViewProf::OnUpdateModelTyy(CCmdUI *pCmdUI)			{ UpdateViewFieldMod(pCmdUI, GRDMOD_GYY); }

void CViewProf::OnModelTyz()								{ CheckModField(GRDMOD_GYZ);	} 
void CViewProf::OnUpdateModelTyz(CCmdUI *pCmdUI)			{ UpdateViewFieldMod(pCmdUI, GRDMOD_GYZ); }

void CViewProf::OnModelTzz()								{ CheckModField(GRDMOD_GZZ);	} 
void CViewProf::OnUpdateModelTzz(CCmdUI *pCmdUI)			{ UpdateViewFieldMod(pCmdUI, GRDMOD_GZZ); }

void CViewProf::FindFieldIndex(int &n) 
{
	switch( m_nFldTbl ) {
		case FLDTLB_MODELED:				return;
		case FLDTLB_DIFFERENCE: n += 20;	return;
		case FLDTLB_MEASURED:	n += 40;	return;
	}
}

void CViewProf::CheckModField(int n)
{
	FindFieldIndex( n );

	if( m_bAxis ) {
		CheckAxis( n );
		return;
	}
	else {
		if( m_pDoc->GetGrid(n)->IsPlanar() ) {
			AfxMessageBox("The grid is planar. Cannot be selected.", MB_OK | MB_ICONINFORMATION);
			return;
		}

		m_bf[n] = !m_bf[n];
		if( !m_bf[n] && m_nFldAxis==n )
			CheckAxis( 0 );
		m_bScale = TRUE;
		Invalidate();
	}
}

void CViewProf::UpdateViewFieldMod(CCmdUI* pCmdUI, int n)
{
	FindFieldIndex( n );

	if( m_bAxis ) {
		UpdateViewFieldScale( pCmdUI, n );
	}
	else {
		CGrid*	pGrd = m_pDoc->GetGrid(n);
		if( pGrd != NULL ) {
			if( pGrd->IsEmpty() || pGrd->IsPlanar() )		{ pCmdUI->Enable(0); m_bf[n] = FALSE;}
			else											{ pCmdUI->Enable(1);}

			if( m_bf[n] )						{ pCmdUI->SetCheck(1);}
			else								{ pCmdUI->SetCheck(0);}
		}
		else {
			pCmdUI->Enable(0);
			pCmdUI->SetCheck(0);
		}
	}
}


void CViewProf::UpdateViewFieldScale(CCmdUI* pCmdUI, int n) 
{
	CGrid*	pGrd = m_pDoc->GetGrid(n);
	if( pGrd != NULL ) {
		if( pGrd->IsEmpty() || pGrd->IsPlanar()  || !m_bf[n] )	{ pCmdUI->Enable(0);}
		else													{ pCmdUI->Enable(1);}

		if( m_nFldAxis == n ) 	pCmdUI->SetCheck(1);
		else					pCmdUI->SetCheck(0);
	}
	else {
		pCmdUI->Enable(0);
		pCmdUI->SetCheck(0);
	}

}

void CViewProf::CheckAxis(int n)
{
	CGrid*	pGrd = m_pDoc->GetGrid(n);
	if( pGrd != NULL ) {
		if( !pGrd->IsEmpty() && !pGrd->IsPlanar() && m_bf[n] )	{
			m_nFldAxis = n;
			m_bInitAxis = TRUE;
			ScaleFieldAxis();
		}
		else	{
			// select nothing or first field
			m_nFldAxis = -1;
			m_bInitAxis = TRUE;
			for(int i = 0; i < m_pDoc->GetComponentsNumber(); i++) {
				if( m_bf[i] ) {
					m_nFldAxis = i;
					break;
				}
			}
			ScaleFieldAxis();
		}
	}
	m_bScale = TRUE;
	Invalidate();
}

void CViewProf::OnViewAxisField()	{	m_bAxis = !m_bAxis;			}
void CViewProf::OnUpdateViewAxisField(CCmdUI *pCmdUI)
{
	if( m_bAxis )		pCmdUI->SetCheck(1);
	else				pCmdUI->SetCheck(0);
}

void CViewProf::OnViewMeasuredField()	{	m_nFldTbl = FLDTLB_MEASURED;	}
void CViewProf::OnUpdateViewMeasuredField(CCmdUI *pCmdUI)
{
	if( m_nFldTbl == FLDTLB_MEASURED )	pCmdUI->SetCheck(1);
	else								pCmdUI->SetCheck(0);
}

void CViewProf::OnViewModeledField()	{	m_nFldTbl = FLDTLB_MODELED;		}
void CViewProf::OnUpdateViewModeledField(CCmdUI *pCmdUI)
{
	if( m_nFldTbl == FLDTLB_MODELED )	pCmdUI->SetCheck(1);
	else								pCmdUI->SetCheck(0);
}

void CViewProf::OnViewDifferenceField(){	m_nFldTbl = FLDTLB_DIFFERENCE;	}
void CViewProf::OnUpdateViewDifferenceField(CCmdUI *pCmdUI)
{
	if( m_nFldTbl == FLDTLB_DIFFERENCE )	pCmdUI->SetCheck(1);
	else									pCmdUI->SetCheck(0);
}



//////////////////////////////////////////////////////////////////////////////////////////////////////////////
//************** zooming *************************************************************************************
void CViewProf::OnViewEqualScale()
{
	m_bScaleEq = !m_bScaleEq;
	m_bScale = TRUE;
	//Scale();
	//UpdatePlgAll();
	Invalidate(FALSE);
}

void CViewProf::OnUpdateViewEqualScale(CCmdUI *pCmdUI)
{
	if(m_bScaleEq)	pCmdUI->SetCheck(1);
	else			pCmdUI->SetCheck(0);
}

void CViewProf::OnViewZoom()
{
	SetCapture();
	::SetCursor(AfxGetApp()->LoadCursor(IDC_CURSOR_ZOOM));
	m_cpt.nTag = CPT_ZOOM;
	m_bZoomig = FALSE;
}

void CViewProf::OnViewZoomIn()
{
	SetCapture();
	::SetCursor(AfxGetApp()->LoadCursor(IDC_CURSOR_ZOOM_IN));
	m_cpt.nTag = CPT_ZOOM_IN;
}

void CViewProf::OnViewZoomOut()
{
	SetCapture();
	::SetCursor(AfxGetApp()->LoadCursor(IDC_CURSOR_ZOOM_OUT));
	m_cpt.nTag = CPT_ZOOM_OUT;
}

void CViewProf::OnViewZoomPage()
{
	m_axH.ZoomToMax();
	m_axV.ZoomToMax();
	m_bScale = TRUE;
	Invalidate();
}

void CViewProf::ZoomIn(CPoint pt)
{
	double	xw, yw;

	// transform to map coordinates
	DPtoWP(pt, xw, yw);
	m_axH.ZoomAxis(xw, 0.8);
	m_axV.ZoomAxis(yw, 0.8);

	m_bScale = TRUE;
	m_cpt.nTag = CPT_NONE;
	ReleaseCapture();
	m_bScaleEq = FALSE;
	Invalidate();
}

void CViewProf::ZoomOut(CPoint pt)
{
	double	xw, yw;

	// transform to map coordinates
	DPtoWP(pt, xw, yw);
	m_axH.ZoomAxis(xw, 1.2);
	m_axV.ZoomAxis(yw, 1.2);

	m_bScale = TRUE;
	m_cpt.nTag = CPT_NONE;
	ReleaseCapture();
	m_bScaleEq = FALSE;
	Invalidate();
}

void CViewProf::Zoom(CPoint pt)
{
	m_bZoomig = FALSE;

	double	l, r, t, b, temp;
	double min, max;

	DPtoWP(pt, r, b);
	pt = m_zoomRect.TopLeft();
	DPtoWP(pt, l, t);

	if( l==r || t==b) 		return;

	if(l>r)		{	temp = l; l = r; r = temp;	}
	m_axH.GetDataExtent(min, max);
	if(l<max && r>min) {
		m_axH.SetDataExtent(l, r);
	}

	if(b>t)		{	temp = b; b = t; t = temp;	}
	m_axV.GetDataExtent(min, max);
	if(b<max && t>min) {
		m_axV.SetDataExtent(b, t);
	}

	m_bScale = TRUE;
	m_cpt.nTag = CPT_NONE;
	m_bScaleEq = FALSE;
	Invalidate();
}

//************** end zooming *********************************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////


void CViewProf::OnProfileNextShow()
{
	m_bProfNext = !m_bProfNext;
	UpdatePlgAll();
	Invalidate(FALSE);
}

void CViewProf::OnUpdateProfileNextShow(CCmdUI *pCmdUI)
{
	if(m_bProfNext )	pCmdUI->SetCheck(1);
	else				pCmdUI->SetCheck(0);
}

void CViewProf::OnProfilePreviousShow()
{
	m_bProfPrev = !m_bProfPrev;
	UpdatePlgAll();
	Invalidate(FALSE);
}

void CViewProf::OnUpdateProfilePreviousShow(CCmdUI *pCmdUI)
{
	if( m_bProfPrev )	pCmdUI->SetCheck(1);
	else				pCmdUI->SetCheck(0);
}





void CViewProf::OnProfileShowModelGuideline()
{
	m_bModelGL = !m_bModelGL;
	Invalidate(FALSE);
}

void CViewProf::OnUpdateProfileShowModelGuideline(CCmdUI *pCmdUI)
{
	if( m_bModelGL )	pCmdUI->SetCheck(1);
	else				pCmdUI->SetCheck(0);
}


void CViewProf::GetProfRect(double& xt, double& yt, double& zt, double& xb, double& yb, double& zb)
{
	int		nRow, nCol;
	double	xw, yw;

	nRow = GetRow();
	nCol = GetCol();
	xw = mod->GetXd(nCol);
	yw = mod->GetYd(nRow);
	zt = mod->GetHeaven();
	zb = mod->GetHell();

	if(m_nPrfType == PROF_X) {
		yt = yb = mod->GetYd(nRow);
		xt = mod->GetX0();
		xb = xt + mod->GetLX();
	}
	if(m_nPrfType == PROF_Y) {
		xt = xb = mod->GetXd(nCol);
		yt = mod->GetY0();
		yb = yt + mod->GetLY();
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// extrem of field
void CViewProf::OnProfileSetExtremField()
{
	CDlgFldSelect dlg;

	// fill combo box
	m_pDoc->GetGridsNonEmptyString(dlg.m_strFld);
	dlg.m_strCombo = m_pDoc->GetFieldString(m_nFldExtrem);
	
	if( dlg.DoModal() == IDOK ) {
		m_pDoc->GetGrid(dlg.m_strCombo, m_nFldExtrem);
	}
}

void CViewProf::OnUpdateProfileSetExtremField(CCmdUI *pCmdUI)
{
	if( m_pDoc->IsEmpty() ) pCmdUI->Enable( 0 );
	else					pCmdUI->Enable( 1 );
}

// jump to profile on which extrem is located
void CViewProf::OnProfileExtrem()
{
	CGrid* pGrd;

	pGrd = m_pDoc->GetGrid(m_nFldExtrem);
	if( pGrd ) {
		int row, col;
		double min, max;
		pGrd->GetMinMaxValue(min, max);
		if( fabs(max) > fabs(min) ) 
			pGrd->GetRowColMax(row, col);
		else
			pGrd->GetRowColMin(row, col);
		if( m_nPrfType == PROF_X )
			MoveProfileTo( row+1 );
		if( m_nPrfType == PROF_Y )
			MoveProfileTo( col+1 );
	}
}

void CViewProf::OnUpdateProfileExtrem(CCmdUI *pCmdUI)
{
	if( m_nFldExtrem == -1 ) pCmdUI->Enable( 0 );
	else					pCmdUI->Enable( 1 );
}

// jump to profile on which mimimum is located
void CViewProf::OnProfileJumptomax()
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

void CViewProf::OnUpdateProfileJumptomax(CCmdUI *pCmdUI)
{
	if( m_nFldExtrem == -1 ) pCmdUI->Enable( 0 );
	else					pCmdUI->Enable( 1 );
}

void CViewProf::OnProfileJumptomin()
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

void CViewProf::OnUpdateProfileJumptomin(CCmdUI *pCmdUI)
{
	if( m_nFldExtrem == -1 ) pCmdUI->Enable( 0 );
	else					pCmdUI->Enable( 1 );
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////


void CViewProf::OnProfileSaveCurrentSettings()
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

void CViewProf::LoadProfileSettings(PRFVIEWSET profSet)
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


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// --------  fitting funtion ------------------------------------------------------------------------------------------------
void CViewProf::FloatMenuVertex(CPoint pt)
{
	CMenu bodyMenu;
	CRect wndRect;
	this->GetWindowRect(wndRect);
	bodyMenu.LoadMenu(IDR_MENU_FLOAT_VERTEX);
	CMenu* subMenu = bodyMenu.GetSubMenu(0);

	m_ptFloatMenu = pt;
	subMenu->TrackPopupMenu(TPM_LEFTALIGN |TPM_RIGHTBUTTON , wndRect.left+pt.x, wndRect.top+pt.y , this);
}

// properties for 1D fiting
void CViewProf::OnComputeFit1d()
{
	m_pDoc->FitVertexProperties();
}

// fittin itself
void CViewProf::OnVertexFit()
{
	m_pDoc->FitVertex(m_nIndex, m_nRow, m_nCol);
}

void CViewProf::OnUpdateVertexFit(CCmdUI *pCmdUI)
{
	// TODO: Add your command update UI handler code here
}
// -------- end fitting funtion ---------------------------------------------------------------------------------------------
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



BOOL CViewProf::PreTranslateMessage(MSG* pMsg)
{
	// activate tool tips
	m_toolTip.RelayEvent(pMsg);

	return CView::PreTranslateMessage(pMsg);
}

// sets tool tip string
void CViewProf::SetToolTip(int tag)
{
	CString str="";

	CBody* pBody = mod->GetBody(m_nIndexID);
	if( pBody && ( tag==CRS_OVER_BODY_LINE || tag==CRS_OVER_BODY || tag==CRS_OVER_VERTEX) ) {
		double d=pBody->GetDensity();
		double s=pBody->GetSusceptibility();
		CString strBody = pBody->GetName();

		str.Format("%s: dens=%4.4g, susc=%4.4g", strBody, d, s);
		//switch(tag) {
		//	case CRS_OVER_VERTEX:
		//		str.Format("%s, dens=%.2f, susc=%.4f",	strBody, d, s);
		//		break;
		//	case CRS_OVER_BODY:
		//		str.Format("%s, dens=%.2f, susc=%.4f", strBody, d, s);
		//		break;
		//	case CRS_OVER_BODY_LINE:
		//		str.Format("%s, dens=%.2f, susc=%.4f", strBody, d, s);
		//		break;
		//	default:
		//		m_strToolTip = "Profile View";
		//}
	}

	if( m_nToolTip != tag)
		m_toolTip.Pop();
	m_nToolTip = tag;

	m_toolTip.UpdateTipText(str, this);

	return;

}

void CViewProf::OnProfileShowTooltip()
{
	m_bToolTip = !m_bToolTip;
	m_toolTip.Activate( m_bToolTip );
}

void CViewProf::OnUpdateProfileShowTooltip(CCmdUI *pCmdUI)
{
	if(m_bToolTip)		pCmdUI->SetCheck(1);
	else				pCmdUI->SetCheck(0);
}

void CViewProf::OnProfileShowDrv()
{
	m_bShowDrv = !m_bShowDrv;
	Invalidate(FALSE);
}

void CViewProf::OnUpdateProfileShowDrv(CCmdUI *pCmdUI)
{
	if(m_bShowDrv)	pCmdUI->SetCheck(1);
	else			pCmdUI->SetCheck(0);
}

void CViewProf::OnProfileShowRms()
{
	m_bShowRms = !m_bShowRms;
	Invalidate(FALSE);
}

void CViewProf::OnUpdateProfileShowRms(CCmdUI *pCmdUI)
{
	if(m_bShowRms)	pCmdUI->SetCheck(1);
	else			pCmdUI->SetCheck(0);
}

void CViewProf::OnViewExtendrange()
{
	m_bAxRange = !m_bAxRange;
	Invalidate(FALSE);
}

void CViewProf::OnUpdateViewExtendrange(CCmdUI *pCmdUI)
{
	if( m_bAxRange )	pCmdUI->SetCheck(1);
	else				pCmdUI->SetCheck(0);
}

