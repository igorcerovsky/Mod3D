// DlgPictureManager.cpp : implementation file
//

#include "stdafx.h"
#include "afxmt.h"
#include "Mod3D.h"
#include "DlgPictureManager.h"
#include "GridObj.h"


// CDlgObjectManager dialog

IMPLEMENT_DYNAMIC(CDlgObjectManager, CDialog)

CDlgObjectManager::CDlgObjectManager(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgObjectManager::IDD, pParent)
{
	m_pObPtrArray = NULL;
	m_txtColor = RGB(0, 51, 102);
	m_randColor = RGB(160, 160, 160);
	m_randPen.CreatePen(PS_SOLID, 1, RGB(160, 160, 160));
	m_randPenCur.CreatePen(PS_SOLID, 1, RGB(200, 200, 200));
	m_pDoc = NULL;
	m_hI = NULL;
}

CDlgObjectManager::~CDlgObjectManager()
{
}

void CDlgObjectManager::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TREE, m_tree);
}


BEGIN_MESSAGE_MAP(CDlgObjectManager, CDialog)
	ON_WM_DRAWITEM()
	ON_WM_CTLCOLOR()
	ON_NOTIFY(NM_CLICK, IDC_TREE, OnNMClickTree)
	ON_NOTIFY(NM_RCLICK, IDC_TREE, OnNMRclickTree)
	ON_WM_LBUTTONDOWN()

	ON_COMMAND(ID_OBJMNG_DELETE, OnObjmngDelete)
	ON_COMMAND(ID_OBJMNG_PROPERTIES, OnObjmngProperties)
	ON_NOTIFY(NM_DBLCLK, IDC_TREE, OnNMDblclkTree)
	ON_WM_ACTIVATE()
	ON_WM_SIZE()
END_MESSAGE_MAP()


// CDlgObjectManager message handlers

BOOL CDlgObjectManager::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_tree.ModifyStyle( TVS_CHECKBOXES, 0 );
	m_tree.ModifyStyle( 0, TVS_CHECKBOXES );
	m_tree.DeleteAllItems();

	CImageList* pImLst;
	pImLst = m_tree.GetImageList(TVSIL_STATE);
	pImLst->Replace(0, AfxGetApp()->LoadIcon(IDI_ICON_IMG_NONE) );
	pImLst->Replace(1, AfxGetApp()->LoadIcon(IDI_ICON_IMG_UNCHECKED) );
	pImLst->Replace(2, AfxGetApp()->LoadIcon(IDI_ICON_IMG_CHECKED) );

	if( m_pDoc == NULL) return 0;
	m_pObPtrArray = m_pDoc->GetObjArray();

	HTREEITEM hBod;		// bodies
	HTREEITEM hMap;		// map bitmaps
	HTREEITEM hPrf;		// profile bitmaps
	HTREEITEM hGrd;		// grids
	HTREEITEM hGul;		// guidelines
	HTREEITEM hWel;		// wells
	HTREEITEM hD3d;		// 3D data
	HTREEITEM hIt;
	CMyObject* pObj;

	hBod = m_tree.InsertItem("Model Bodies");
	hMap = m_tree.InsertItem("Map pictures");
	hPrf = m_tree.InsertItem("Profile profile pictures");
	hGrd = m_tree.InsertItem("Grids");
	hGul = m_tree.InsertItem("Giudelines");
	hWel = m_tree.InsertItem("Wells");
	hD3d = m_tree.InsertItem("3D Data");
	for( int i=0; i<m_pObPtrArray->GetSize(); i++) {
		pObj = m_pObPtrArray->GetAt(i);
		if( pObj->IsKindOf( RUNTIME_CLASS( CImgReferenced ) )  ) {
			CImgReferenced* pIm = (CImgReferenced*) pObj;
			switch( pIm->m_nType ) {
				case IMG_MAP:
					hIt = m_tree.InsertItem(pObj->GetIdName(), hMap);
					pIm->m_hTreeItem = hIt;
					if( pIm->IsVisible() )	m_tree.SetCheck(hIt, TRUE);
					break;
				case IMG_PRF:
					hIt = m_tree.InsertItem(pObj->GetIdName(), hPrf);
					pIm->m_hTreeItem = hIt;
					m_tree.SetCheck(hIt, pIm->IsVisible() );
					break;
			}
		}
		if( pObj->IsKindOf( RUNTIME_CLASS(CGridObj) ) ) {
			CGridObj* pOb = (CGridObj*) pObj;
			switch( pOb->GetType() ) {
				case CGridObj::leadingHorizon:
					hIt = m_tree.InsertItem(pObj->GetIdName(), hGrd);
					if( pOb->IsVisible() )	m_tree.SetCheck(hIt, TRUE);
					break;
			}
		}
		if( pObj->IsKindOf( RUNTIME_CLASS(CGuideMark)  ) ) {
			CGuideMark* pOb = (CGuideMark*) pObj;
			hIt = m_tree.InsertItem(pObj->GetIdName(), hGul);
			if( pOb->IsVisible() )	m_tree.SetCheck(hIt, TRUE);
		}
		if( pObj->IsKindOf( RUNTIME_CLASS(CWell) ) ) {
			CWell* pOb = (CWell*) pObj;
			hIt = m_tree.InsertItem(pObj->GetIdName(), hWel);
			if( pOb->IsVisible() )	
				m_tree.SetCheck(hIt, TRUE);
		}
		if( pObj->IsKindOf( RUNTIME_CLASS(CData3D) ) ) {
			CData3D* pOb = (CData3D*) pObj;
			hIt = m_tree.InsertItem(pObj->GetIdName(), hD3d);
			if( pOb->IsVisible() )	
				m_tree.SetCheck(hIt, TRUE);
		}
	}

	BodyPtrArray* pBds;
	pBds = m_pDoc->GetModel()->GetBodies();
	if( pBds != NULL ) {
		for(int i=0; i<pBds->GetSize(); i++) {
			CBody* pBd = pBds->GetAt(i);
			hIt = m_tree.InsertItem(pBd->GetStrIdName(), hBod);
			m_tree.SetCheck(hIt, pBd->IsVisisble());
		}
	}

	m_tree.Expand(hBod, TVE_EXPAND);
	m_tree.Expand(hMap, TVE_EXPAND);
	m_tree.Expand(hPrf, TVE_EXPAND);
	m_tree.Expand(hGrd, TVE_EXPAND);
	m_tree.Expand(hGul, TVE_EXPAND);
	m_tree.Expand(hWel, TVE_EXPAND);
	m_tree.Expand(hD3d, TVE_EXPAND);
	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

CMyObject* CDlgObjectManager::FindObj(HTREEITEM hI)
{
	if( m_pDoc == NULL)	return NULL;

	CMyObject* pObj;
	CString	strItemID;
	CString strObjID;

	strItemID = m_tree.GetItemText(hI);

	for( int i=0; i<m_pObPtrArray->GetSize(); i++) {
		pObj = m_pObPtrArray->GetAt(i);
		strObjID = pObj->GetID();
		if( strItemID.Find(strObjID) != -1 ) 
			return pObj;
	}


	return NULL;
}

CObject* CDlgObjectManager::FindBody(HTREEITEM hI)
{
	if( m_pDoc == NULL)	return NULL;

	CString strTxt = m_tree.GetItemText( m_tree.GetParentItem(hI) );
	if( strTxt = "Model Bodies") {
		BodyPtrArray* pBds;
		pBds = m_pDoc->GetModel()->GetBodies();
		CString strObjID = m_tree.GetItemText(hI);
		if( pBds != NULL ) {
			for(int i=0; i<pBds->GetSize(); i++) {
				CBody* pBd = pBds->GetAt(i);
				CString strBodyID = pBd->GetStrIdName();
				if( strBodyID.Find(strObjID) != -1 ) {
					return pBd;
				}
			}
		}
	}

	return NULL;
}

void CDlgObjectManager::OnNMClickTree(NMHDR *pNMHDR, LRESULT *pResult)
{
	if( m_pDoc == NULL)	return;

	CMyObject*	pObj;
	HTREEITEM	hI;
	CPoint		pt;
	UINT		uFlags;
	CRect		rc;

	GetCursorPos(&pt);
	m_tree.GetWindowRect(&rc);
	pt.x -= rc.left;
	pt.y -= rc.top;
	hI = m_tree.HitTest( pt, &uFlags);
	pObj = FindObj(hI);
	if(pObj && (uFlags==64)) {
		pObj->Show(	!m_tree.GetCheck(hI) );
		if(m_pDoc) {
			m_pDoc->UpdateAllViews(NULL, UPDATE_OBJECT_MANAGER_SHOW);
			//if( pObj->IsKindOf(RUNTIME_CLASS( CImgReferenced )) )
			//	m_pDoc->UpdateAllViews(NULL, UPDATE_BITMAP_DELETE);
			//if( pObj->IsKindOf(RUNTIME_CLASS(CGridObj)) )
			//	m_pDoc->UpdateAllViews(NULL, UPDATE_LEADHRZ_DELETE);
		}
	}

	CBody* pBd;
	pBd = (CBody*) FindBody(hI);
	if(pBd && (uFlags==64)) {
		pBd->Show(	!m_tree.GetCheck(hI) );
		if(m_pDoc)
			m_pDoc->UpdateAllViews(NULL, UPDATE_OBJECT_MANAGER_SHOW);
	}

	pNMHDR->code = 0;
	*pResult = 0;
}

void CDlgObjectManager::OnNMRclickTree(NMHDR *pNMHDR, LRESULT *pResult)
{
	if( m_pDoc == NULL)	return;

	CMyObject*	pObj;
	HTREEITEM	hI;
	CPoint		pt, ptMenu;
	UINT		uFlags;
	CRect		rc;

	GetCursorPos(&pt);
	ptMenu=pt;
	m_tree.GetWindowRect(&rc);
	pt.x -= rc.left;
	pt.y -= rc.top;
	hI = m_tree.HitTest( pt, &uFlags);
	pObj = FindObj(hI);
	if(pObj && (uFlags==TVHT_ONITEMLABEL) ){
		m_hI = hI;
		CMenu menu;
		CRect wndRect;
		this->GetWindowRect(wndRect);
		menu.LoadMenu(IDR_MENU_OBJMNG);
		CMenu* subMenu = menu.GetSubMenu(0);
		subMenu->TrackPopupMenu(TPM_LEFTALIGN |TPM_RIGHTBUTTON , ptMenu.x, ptMenu.y, this);
	}

	CBody* pBd;
	pBd = (CBody*) FindBody(hI);
	if(pBd && (uFlags==TVHT_ONITEMLABEL) ) {
		m_hI = hI;
		CMenu menu;
		CRect wndRect;
		this->GetWindowRect(wndRect);
		menu.LoadMenu(IDR_MENU_OBJMNG);
		CMenu* subMenu = menu.GetSubMenu(0);
		subMenu->TrackPopupMenu(TPM_LEFTALIGN |TPM_RIGHTBUTTON , ptMenu.x, ptMenu.y, this);
	}

	*pResult = 0;
}


void CDlgObjectManager::OnObjmngDelete()
{
	if(!m_hI) return;

	CCriticalSection cs;
	cs.Lock();
	CMyObject*	pObj;
	for( int i=0; i<m_pObPtrArray->GetSize(); i++) {
		pObj = m_pObPtrArray->GetAt(i);
		if( m_tree.GetItemText(m_hI) == pObj->GetIdName() ) {
			int res = AfxMessageBox("Delete? Sure?", MB_YESNO | MB_ICONQUESTION);
			if( res == IDYES ) {
				m_pObPtrArray->RemoveAt( i );
				m_tree.DeleteItem(m_hI);
				m_hI = NULL;
				cs.Unlock();
				m_pDoc->SetModifiedFlag();
				if( pObj->IsKindOf(RUNTIME_CLASS( CImgReferenced )) )
					m_pDoc->UpdateAllViews(NULL, UPDATE_BITMAP_DELETE);
				else if( pObj->IsKindOf(RUNTIME_CLASS(CGridObj)) )
					m_pDoc->UpdateAllViews(NULL, UPDATE_LEADHRZ_DELETE);
				else if( pObj->IsKindOf(RUNTIME_CLASS(CData3D)) )
					m_pDoc->UpdateAllViews(NULL, UPDATE_DATA3D_DELETE);
				else
					m_pDoc->UpdateAllViews(NULL);
				delete pObj;
				return;
			}
		}
	}

	CBody* pBd;
	pBd = (CBody*) FindBody(m_hI);
	if(pBd) {
		int res = AfxMessageBox("Delete Body? Sure?", MB_YESNO | MB_ICONQUESTION);
		if( res == IDYES ) {
			m_pDoc->GetModel()->DeleteBody( pBd->GetID() );
			m_tree.DeleteItem(m_hI);
			m_hI = NULL;
			m_pDoc->SetModifiedFlag();
			m_pDoc->UpdateAllViews(NULL, UPDATE_BODY_EDIT_DELETE);
		}
	}

	cs.Unlock();
}

void CDlgObjectManager::OnObjmngProperties()
{
	if(!m_hI) return;

	CCriticalSection cs;
	cs.Lock();
	CMyObject*	pObj;
	pObj = FindObj(m_hI);
	if(pObj && m_pDoc) {
		pObj->Properties();
		m_tree.SetItemText(m_hI, pObj->GetIdName());
		if(m_pDoc) {
			m_pDoc->SetModifiedFlag();
			if( pObj->IsKindOf(RUNTIME_CLASS( CImgReferenced )) )
				m_pDoc->UpdateAllViews(NULL, UPDATE_BITMAP_CAHNGED);
			else if( pObj->IsKindOf(RUNTIME_CLASS(CGridObj)) )
				m_pDoc->UpdateAllViews(NULL, UPDATE_LEADHRZ_CAHNGED);
			else if( pObj->IsKindOf(RUNTIME_CLASS(CData3D)) )
				m_pDoc->UpdateAllViews(NULL, UPDATE_DATA3D_CHANGED);
			else
				m_pDoc->UpdateAllViews(NULL);
			m_hI = NULL;
		}
		return;
	}

	CBody* pBd;
	pBd = (CBody*) FindBody(m_hI);
	if(pBd && m_pDoc) {
		pBd->Properties();
		m_tree.SetItemText( m_hI, pBd->GetStrIdName() );
		m_hI = NULL;
	}
}

void CDlgObjectManager::OnNMDblclkTree(NMHDR *pNMHDR, LRESULT *pResult)
{
	if( m_pDoc == NULL)	return;

	CMyObject*	pObj;
	HTREEITEM	hI;
	CPoint		pt;
	UINT		uFlags;
	CRect		rc;

	GetCursorPos(&pt);
	m_tree.GetWindowRect(&rc);
	pt.x -= rc.left;
	pt.y -= rc.top;
	hI = m_tree.HitTest( pt, &uFlags);
	pObj = FindObj(hI);
	if(pObj && m_pDoc && (TVHT_ONITEM & uFlags)) {
		pObj->Properties();
		m_tree.SetItemText(hI, pObj->GetIdName());
		if(m_pDoc) {
			m_pDoc->SetModifiedFlag();
			if( pObj->IsKindOf(RUNTIME_CLASS( CImgReferenced )) )
				m_pDoc->UpdateAllViews(NULL, UPDATE_BITMAP_CAHNGED);
			else if( pObj->IsKindOf(RUNTIME_CLASS(CGridObj)) )
				m_pDoc->UpdateAllViews(NULL, UPDATE_LEADHRZ_CAHNGED);
			else if( pObj->IsKindOf(RUNTIME_CLASS(CData3D)) )
				m_pDoc->UpdateAllViews(NULL, UPDATE_DATA3D_CHANGED);
			else
				m_pDoc->UpdateAllViews(NULL);
		}
	}

	CBody* pBd;
	pBd = (CBody*) FindBody(hI);
	if(pBd && m_pDoc && (TVHT_ONITEM & uFlags)) {
		pBd->Properties();
		m_tree.SetItemText( hI, pBd->GetStrIdName() );
	}

	pNMHDR->code = 0;
	*pResult = 0;
}


void CDlgObjectManager::OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized)
{
	CDialog::OnActivate(nState, pWndOther, bMinimized);

	CMDIChildWnd* pChildWnd;
	CMod3DDoc*	pDoc;
	CView*		pView;

	CMDIFrameWnd* pMainWnd = (CMDIFrameWnd*) GetParent();
	pChildWnd = (CMDIChildWnd *) pMainWnd->MDIGetActive();
	if( pChildWnd == NULL )		return;

	pView = (CView *) pChildWnd->GetActiveView();
	ASSERT( pView != NULL );
	pDoc = (CMod3DDoc*) pView->GetDocument();

	if( pDoc == NULL )
		return;

	if( pDoc != m_pDoc ) {
		m_pDoc = pDoc;
		OnInitDialog();
	}
}

void CDlgObjectManager::OnSize(UINT nType, int cx, int cy)
{
	CDialog::OnSize(nType, cx, cy);

	if( m_tree.GetSafeHwnd() != NULL ) {
		m_tree.MoveWindow(0, 0, cx, cy);
	}
}
