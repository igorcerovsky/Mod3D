// MainFrm.cpp : implementation of the CMainFrame class
//

#include "stdafx.h"
#include "Mod3D.h"
#include "Grid.h"		// because of DUMMY
#include "MainFrm.h"
#include "Mod3DDoc.h"

double	g_x = 0, g_y = 0, g_z = 0;
double	g_Gz = 0, g_M;
int		g_row = 0, g_col = 0;
long	g_nComp = 0, g_nTotComp = 1;
int		g_nIndFld = GRDMOD_GZ;
double	g_dFld;
CString	g_strFld;

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


/////////////////////////////////////////////////////////////////////////////
// CMainFrame

IMPLEMENT_DYNAMIC(CMainFrame, CMDIFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CMDIFrameWnd)
	ON_WM_CREATE()
	ON_COMMAND(ID_WINDOW_MAP, OnNewWindowMap)
	ON_COMMAND(ID_WINDOW_3D, OnNewWindow3D)
	ON_COMMAND(ID_VIEW_PROFILE_TOOLBAR, OnViewProfileToolbar)
	ON_UPDATE_COMMAND_UI(ID_VIEW_PROFILE_TOOLBAR, OnUpdateViewProfileToolbar)
	ON_COMMAND(ID_VIEW_TOOLBAR_MAP_VIEW, OnViewToolbarMapView)
	ON_UPDATE_COMMAND_UI(ID_VIEW_TOOLBAR_MAP_VIEW, OnUpdateViewToolbarMapView)
	ON_COMMAND(ID_VIEW_FIELD_TOOLBAR, OnViewFieldToolbar)
	ON_UPDATE_COMMAND_UI(ID_VIEW_FIELD_TOOLBAR, OnUpdateViewFieldToolbar)
	// Global help commands
	ON_COMMAND(ID_HELP_FINDER, CMDIFrameWnd::OnHelpFinder)
	ON_COMMAND(ID_HELP, CMDIFrameWnd::OnHelp)
	ON_COMMAND(ID_CONTEXT_HELP, CMDIFrameWnd::OnContextHelp)
	ON_COMMAND(ID_DEFAULT_HELP, CMDIFrameWnd::OnHelpFinder)
	// status bar indicators
	ON_UPDATE_COMMAND_UI(ID_INDICATOR_ROW, OnUpdateRow)
	ON_UPDATE_COMMAND_UI(ID_INDICATOR_COL, OnUpdateCol)
	ON_UPDATE_COMMAND_UI(ID_INDICATOR_X, OnUpdateCoordinateX)
	ON_UPDATE_COMMAND_UI(ID_INDICATOR_Y, OnUpdateCoordinateY)
	ON_UPDATE_COMMAND_UI(ID_INDICATOR_Z, OnUpdateCoordinateZ)
	ON_UPDATE_COMMAND_UI(ID_INDICATOR_FLD, OnUpdateIndicatorFld)
	ON_UPDATE_COMMAND_UI(ID_INDICATOR_COMP, OnUpdateComp)

	ON_WM_TIMER()
	ON_UPDATE_COMMAND_UI(ID_WINDOW_3D, OnUpdateWindow3d)

	ON_MESSAGE(WM_COMPUTETHREADFINISHED, OnComputationFinished)

	ON_MESSAGE(WM_BODY_PROPERTIES, OnBodyProperties)

	ON_COMMAND(ID_OBJECT_MANAGER, OnObjectManager)
	ON_UPDATE_COMMAND_UI(ID_OBJECT_MANAGER, OnUpdateObjectManagerUI)

	ON_MESSAGE(WM_UPDATE_OBJMNG, OnUpdateObjMng)
	ON_COMMAND(ID_WINDOW_NEWGRIDWINDOW, OnWindowNewgridwindow)
	ON_WM_MOVING()
END_MESSAGE_MAP()

LRESULT CMainFrame::OnComputationFinished(WPARAM wParam, LPARAM lParam)
{
	TRACE("we are in CMainFrame::OnComputationFinished(...)");

	CMDIChildWnd* pChildWnd;
	CMDIChildWnd* pNextWnd;
	CMod3DDoc*	pDocComp;
	CMod3DDoc*	pDoc;
	CView*		pView;
	int			tag;

	pDocComp = (CMod3DDoc*) wParam;
	tag	= (int) lParam;

	pNextWnd = NULL;
	pChildWnd = (CMDIChildWnd *) MDIGetActive();
	pView = (CView *) pChildWnd->GetActiveView();
	pDoc = (CMod3DDoc*) pView->GetDocument();

	while( pChildWnd != pNextWnd ) {
		if(pDocComp == pDoc) {
			if(tag == WM_COMPUTETHREADFINISHED)
				pDoc->ComputationFinished();
			else if(tag == WM_COMPUTETHREADCANCELED)
				pDoc->ComputationCanceled();
			break;
		}
		MDINext();
		pNextWnd = (CMDIChildWnd *) MDIGetActive();
		pView = (CView *) pNextWnd->GetActiveView();
		pDoc = (CMod3DDoc*) pView->GetDocument();
	}

	return 0;
}

LRESULT CMainFrame::OnBodyProperties(WPARAM wParam, LPARAM lParam)
{
	TRACE("we are in CMainFrame::OnComputationFinished(...)");

	CMDIChildWnd* pChildWnd;
	CMod3DDoc*	pDoc;
	CView*		pView;

	pChildWnd = (CMDIChildWnd *) MDIGetActive();
	pView = (CView *) pChildWnd->GetActiveView();
	ASSERT( pView != NULL );
	pDoc = (CMod3DDoc*) pView->GetDocument();
	ASSERT( pDoc != NULL );

	pDoc->BodyPropertiesChanged((int) wParam, m_pPropSheetBody );

	return 0;
}





static UINT indicators[] =
{
	ID_SEPARATOR,           // status line indicator
	ID_INDICATOR_COMP,
	ID_INDICATOR_ROW,
	ID_INDICATOR_COL,
	ID_INDICATOR_X,
	ID_INDICATOR_Y,
	ID_INDICATOR_Z,
	ID_INDICATOR_FLD,
};

void CMainFrame::OnUpdateComp(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(); 
	CString str;
	str.Format(" %3.0f%%", 100*(float)g_nComp/g_nTotComp); 
	pCmdUI->SetText( str ); 
}

void CMainFrame::OnUpdateIndicatorFld(CCmdUI *pCmdUI)
{
    pCmdUI->Enable(); 
	CString str;
	str.Format( "%s=%6.6f", g_strFld, (float) g_dFld ); 
	pCmdUI->SetText( str ); 
}


void CMainFrame::OnUpdateRow(CCmdUI *pCmdUI)
{
    pCmdUI->Enable(); 
	CString str;
	str.Format( "row %d", (int) g_row ); 
	pCmdUI->SetText( str ); 
}

void CMainFrame::OnUpdateCol(CCmdUI *pCmdUI)
{
    pCmdUI->Enable(); 
	CString str;
	str.Format( "col %d", (int) g_col ); 
	pCmdUI->SetText( str ); 
}

void CMainFrame::OnUpdateCoordinateX(CCmdUI *pCmdUI)
{
    pCmdUI->Enable(); 
	CString str;
	str.Format( "x=%6.1f", (float) g_x ); 
	pCmdUI->SetText( str ); 
}

void CMainFrame::OnUpdateCoordinateY(CCmdUI *pCmdUI)
{
    pCmdUI->Enable(); 
	CString str;
	str.Format( "y=%6.1f", (float) g_y ); 
	pCmdUI->SetText( str ); 
}

void CMainFrame::OnUpdateCoordinateZ(CCmdUI *pCmdUI)
{
    pCmdUI->Enable(); 
	CString str;
	if(g_z == DUMMY){
		str = "";}
	else
		str.Format( "z=%6.3f", (float) g_z ); 
	pCmdUI->SetText( str ); 
}

/////////////////////////////////////////////////////////////////////////////
// CMainFrame construction/destruction

CMainFrame::CMainFrame()
{
	m_pPropSheetBody = NULL;
}

CMainFrame::~CMainFrame()
{
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CMDIFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	if (!m_wndToolBar.CreateEx(this, TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_TOP
		| CBRS_GRIPPER | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC) ||
		!m_wndToolBar.LoadToolBar(IDR_MAINFRAME))
	{
		TRACE0("Failed to create toolbar\n");
		return -1;      // fail to create
	}

	if (!m_wndStatusBar.Create(this) ||
		!m_wndStatusBar.SetIndicators(indicators,
		  sizeof(indicators)/sizeof(UINT)))
	{
		TRACE0("Failed to create status bar\n");
		return -1;      // fail to create
	}

	if (!m_wndProfViewBar.CreateEx(this, TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_TOP
		| CBRS_GRIPPER | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC)||
		!m_wndProfViewBar.LoadToolBar(IDR_TOOLBAR_PROF_VIEW)){
		TRACE0("Failed to create toolbar\n");
	}

	if (!m_wndMapViewBar.CreateEx(this, TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_TOP
		| CBRS_GRIPPER | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC)||
		!m_wndMapViewBar.LoadToolBar(IDR_TOOLBAR_MAP_VIEW)){
		TRACE0("Failed to create toolbar\n");
	}

	if (!m_wndFldBar.CreateEx(this, TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_RIGHT
		| CBRS_GRIPPER | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC)||
		!m_wndFldBar.LoadToolBar(IDR_TOOLBAR_FLD_VIEW)){
		TRACE0("Failed to create toolbar\n");
	}

	//if (!m_wndObjMng.Create(this, IDD_DLG_OBJECT_MANAGER, CBRS_LEFT|CBRS_TOOLTIPS|CBRS_FLYBY , IDD_DLG_OBJECT_MANAGER ) ){
	//	TRACE0("Failed to create toolbar\n");
	//}

	m_wndToolBar.SetWindowText(_T("Mod3D"));
	m_wndFldBar.SetWindowText(_T("Field Show / Hide"));
	m_wndMapViewBar.SetWindowText(_T("Map"));
	m_wndProfViewBar.SetWindowText(_T("Profile"));
	//m_wndObjMng.SetWindowText(_T("Object Manager"));
	
	// TODO: Delete these n lines if you don't want the toolbar to
	//  be dockable
	CRect rect;
	EnableDocking(CBRS_ALIGN_ANY);

	// simulating to drag ctrlbar, to align it

	//RecalcLayout();
	//m_wndToolBar.GetWindowRect(&rect);
	m_wndProfViewBar.EnableDocking(CBRS_ALIGN_ANY);
	DockControlBar(&m_wndProfViewBar, AFX_IDW_DOCKBAR_TOP);

	RecalcLayout();
	m_wndProfViewBar.GetWindowRect(&rect);
	m_wndMapViewBar.EnableDocking(CBRS_ALIGN_ANY);
	DockControlBar(&m_wndMapViewBar, AFX_IDW_DOCKBAR_TOP, &rect);

	RecalcLayout();
	m_wndMapViewBar.GetWindowRect(&rect);
	m_wndToolBar.EnableDocking(CBRS_ALIGN_ANY);
	DockControlBar(&m_wndToolBar, AFX_IDW_DOCKBAR_TOP, &rect);

	// right bars
	//RecalcLayout();
	m_wndFldBar.EnableDocking(CBRS_ALIGN_ANY);
	DockControlBar(&m_wndFldBar, AFX_IDW_DOCKBAR_RIGHT);
	//

	if( m_wndObjMng.Create( IDD_DLG_OBJECT_MANAGER, this ) == FALSE) {
		AfxMessageBox("Error creating Object Manager!", MB_OK | MB_ICONEXCLAMATION);
	}
	else {
		m_wndObjMng.m_pDoc = NULL;
		//m_wndObjMng.ShowWindow(SW_SHOW);
	}


	SetTimer(1, 1000, NULL);

	return 0;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if( !CMDIFrameWnd::PreCreateWindow(cs) )
		return FALSE;
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CMainFrame diagnostics

#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
	CMDIFrameWnd::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
	CMDIFrameWnd::Dump(dc);
}

#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CMainFrame message handlers


void CMainFrame::OnNewWindowMap() 
{
	CMDIChildWnd* pActiveChild = MDIGetActive();
	CDocument* pDocument;
	if (pActiveChild == NULL ||
	  (pDocument = pActiveChild->GetActiveDocument()) == NULL)
	{
		TRACE0("Warning: No active document for WindowNew command.\n");
		AfxMessageBox(AFX_IDP_COMMAND_FAILURE);
		return;     // command failed
	}

	// otherwise we have a new frame !
	CDocTemplate* pTemplate = ( (CMod3DApp*) AfxGetApp() )->m_pTemplateMap;
	ASSERT_VALID(pTemplate);
	CFrameWnd* pFrame = pTemplate->CreateNewFrame(pDocument, pActiveChild);
	if (pFrame == NULL)
	{
		TRACE0("Warning: failed to create new frame.\n");
		return;     // command failed
	}

	pTemplate->InitialUpdateFrame(pFrame, pDocument);
}

void CMainFrame::OnNewWindow3D() 
{
	CMDIChildWnd* pActiveChild = MDIGetActive();
	CDocument* pDocument;
	if (pActiveChild == NULL ||
	  (pDocument = pActiveChild->GetActiveDocument()) == NULL)
	{
		TRACE0("Warning: No active document for WindowNew command.\n");
		AfxMessageBox(AFX_IDP_COMMAND_FAILURE);
		return;     // command failed
	}

	CMod3DDoc* pDoc = (CMod3DDoc*) pDocument;
	if( pDoc->IsEmpty() ) {
		AfxMessageBox("3D View is available only for initialized document! Aborting operation.", MB_OK | MB_ICONINFORMATION);
		return;
	}

	// otherwise we have a new frame !
	CDocTemplate* pTemplate = ( (CMod3DApp*) AfxGetApp() )->m_pTemplate3D;
	ASSERT_VALID(pTemplate);
	CFrameWnd* pFrame = pTemplate->CreateNewFrame(pDocument, pActiveChild);
	if (pFrame == NULL)
	{
		TRACE0("Warning: failed to create new frame.\n");
		return;     // command failed
	}

	pTemplate->InitialUpdateFrame(pFrame, pDocument);
}

void CMainFrame::OnWindowNewgridwindow()
{
	CMDIChildWnd* pActiveChild = MDIGetActive();
	CDocument* pDocument;
	if (pActiveChild == NULL ||
	  (pDocument = pActiveChild->GetActiveDocument()) == NULL)
	{
		TRACE0("Warning: No active document for WindowNew command.\n");
		AfxMessageBox(AFX_IDP_COMMAND_FAILURE);
		return;     // command failed
	}

	CMod3DDoc* pDoc = (CMod3DDoc*) pDocument;
	if( pDoc->IsEmpty() ) {
		AfxMessageBox("3D View is available only for initialized document! Aborting operation.", MB_OK | MB_ICONINFORMATION);
		return;
	}

	// otherwise we have a new frame !
	CDocTemplate* pTemplate = ( (CMod3DApp*) AfxGetApp() )->m_pTemplateGrd;
	ASSERT_VALID(pTemplate);
	CFrameWnd* pFrame = pTemplate->CreateNewFrame(pDocument, pActiveChild);
	if (pFrame == NULL)
	{
		TRACE0("Warning: failed to create new frame.\n");
		return;     // command failed
	}

	pTemplate->InitialUpdateFrame(pFrame, pDocument);
}

void CMainFrame::OnViewProfileToolbar() 
{
	BOOL bShow;
	bShow = ((m_wndProfViewBar.GetStyle() & WS_VISIBLE) != 0);
	ShowControlBar(&m_wndProfViewBar, !bShow, FALSE);	
	m_wndProfViewBar.Invalidate();
}

void CMainFrame::OnUpdateViewProfileToolbar(CCmdUI* pCmdUI) 
{
	BOOL bShow;
	bShow = ((m_wndProfViewBar.GetStyle() & WS_VISIBLE) != 0);
	if(bShow)
		pCmdUI->SetCheck();
	else
		pCmdUI->SetCheck(0);
}

void CMainFrame::OnViewToolbarMapView() 
{
	BOOL bShow;
	bShow = ((m_wndMapViewBar.GetStyle() & WS_VISIBLE) != 0);
	ShowControlBar(&m_wndMapViewBar, !bShow, FALSE);	
	m_wndMapViewBar.Invalidate();
}

void CMainFrame::OnUpdateViewToolbarMapView(CCmdUI* pCmdUI) 
{
	BOOL bShow;
	bShow = ((m_wndMapViewBar.GetStyle() & WS_VISIBLE) != 0);
	if(bShow)
		pCmdUI->SetCheck();
	else
		pCmdUI->SetCheck(0);
}

void CMainFrame::OnViewFieldToolbar() 
{
	BOOL bShow;
	bShow = ((m_wndFldBar.GetStyle() & WS_VISIBLE) != 0);
	ShowControlBar(&m_wndFldBar, !bShow, FALSE);	
	m_wndFldBar.Invalidate();
}

void CMainFrame::OnUpdateViewFieldToolbar(CCmdUI* pCmdUI) 
{
	BOOL bShow;
	bShow = ((m_wndFldBar.GetStyle() & WS_VISIBLE) != 0);
	if(bShow)
		pCmdUI->SetCheck();
	else
		pCmdUI->SetCheck(0);
}

void CMainFrame::OnTimer(UINT nIDEvent)
{
	// TODO: Add your message handler code here and/or call default
	CMDIFrameWnd::OnTimer(nIDEvent);
}

void CMainFrame::OnUpdateWindow3d(CCmdUI *pCmdUI)
{
}

void CMainFrame::SetPropSheetBody(CPropertySheet* pSheet)
{
	m_pPropSheetBody = pSheet;
}

void CMainFrame::OnObjectManager()
{
	BOOL bVisible;

	bVisible = !(m_wndObjMng.GetStyle() & WS_VISIBLE);

	if( bVisible )
		m_wndObjMng.ShowWindow( SW_SHOW );
	else
		m_wndObjMng.ShowWindow( SW_HIDE );
}

void CMainFrame::OnUpdateObjectManagerUI(CCmdUI *pCmdUI)
{
	BOOL bVisible;

	bVisible = (  (m_wndObjMng.GetStyle() & WS_VISIBLE) != 0 );

	if( bVisible )	pCmdUI->SetCheck(1);
	else			pCmdUI->SetCheck(0);
}

LRESULT CMainFrame::OnUpdateObjMng(WPARAM wParam, LPARAM lParam)
{
	CMod3DDoc* pDoc = (CMod3DDoc*) wParam;
	int l = (int) lParam;

	
	if(l==1 && pDoc!=NULL) {
		m_wndObjMng.m_pDoc = pDoc;
	}

	if( l == -1 ) {
		m_wndObjMng.m_pDoc = NULL;
	}

	m_wndObjMng.OnInitDialog();

	return (LRESULT)0;
}



int CMainFrame::SnapWindows(CMDIChildWnd* pWndChld)
{
	CWnd* pWnd;
	CRect rcWnd;
	CRect rcChl;
	pWndChld->GetWindowRect(&rcChl);
	pWnd = pWndChld->GetNextWindow();
	ScreenToClient(&rcChl);
	while(pWnd) {
		pWnd->GetWindowRect(rcWnd);
		ScreenToClient(&rcWnd);
		// right pChildWndBound
		if( abs(rcChl.right-rcWnd.left) < 10 ) {
			pWndChld->SetWindowPos(NULL, rcChl.left, rcChl.top, rcChl.left-rcWnd.right, -1, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
			return 1;
		}

		pWnd = pWnd->GetNextWindow();
		
	}

	return 0;
}

BOOL CMainFrame::OnWndMsg(UINT message, WPARAM wParam, LPARAM lParam, LRESULT* pResult)
{
	// TODO: Add your specialized code here and/or call the base class

	return CMDIFrameWnd::OnWndMsg(message, wParam, lParam, pResult);
}
