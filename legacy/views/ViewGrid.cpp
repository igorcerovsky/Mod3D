// GridViewDemoView.cpp : implementation of the CViewGrid class
//

#include "stdafx.h"
#include "Mod3D.h"

#include "ViewGrid.h"
#include "Mod3DDoc.h"
#include "ViewProf.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CViewGrid

IMPLEMENT_DYNCREATE(CViewGrid, CView)

BEGIN_MESSAGE_MAP(CViewGrid, CView)
	ON_WM_SIZE()
    //ON_COMMAND(ID_TOGGLE_READONLY, OnToggleReadonly)
	ON_WM_ERASEBKGND()
	// Standard printing commands
	ON_COMMAND(ID_FILE_PRINT, CView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_DIRECT, CView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_PREVIEW, CView::OnFilePrintPreview)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CViewGrid construction/destruction

CViewGrid::CViewGrid()
{
	m_pGridCtrl = NULL;
}

CViewGrid::~CViewGrid()
{
	if (m_pGridCtrl)
		delete m_pGridCtrl;
}

BOOL CViewGrid::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	return CView::PreCreateWindow(cs);
}

/////////////////////////////////////////////////////////////////////////////
// CViewGrid drawing

void CViewGrid::OnDraw(CDC* pDC)
{
	CMod3DDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);

	// TODO: add draw code for native data here
}

/////////////////////////////////////////////////////////////////////////////
// CViewGrid printing

BOOL CViewGrid::OnPreparePrinting(CPrintInfo* pInfo)
{
	// default preparation
	return DoPreparePrinting(pInfo);
}

void CViewGrid::OnBeginPrinting(CDC* pDC, CPrintInfo* pInfo)
{
	if (m_pGridCtrl)
		m_pGridCtrl->OnBeginPrinting(pDC, pInfo);
}

void CViewGrid::OnPrint(CDC* pDC, CPrintInfo* pInfo) 
{
	if (m_pGridCtrl)
		m_pGridCtrl->OnPrint(pDC, pInfo);
}

void CViewGrid::OnEndPrinting(CDC* pDC, CPrintInfo* pInfo)
{
	if (m_pGridCtrl)
		m_pGridCtrl->OnEndPrinting(pDC, pInfo);
}

/////////////////////////////////////////////////////////////////////////////
// CViewGrid diagnostics

#ifdef _DEBUG
void CViewGrid::AssertValid() const
{
	CView::AssertValid();
}

void CViewGrid::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}

CMod3DDoc* CViewGrid::GetDocument() // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CMod3DDoc)));
	return (CMod3DDoc*)m_pDocument;
}
#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CViewGrid message handlers

void CViewGrid::OnInitialUpdate() 
{
	CView::OnInitialUpdate();
	
	if (m_pGridCtrl == NULL)
	{
		// Create the Gridctrl object
		m_pGridCtrl = new CGridCtrl;
		if (!m_pGridCtrl) return;

		// Create the Gridctrl window
		CRect rect;
		GetClientRect(rect);
		m_pGridCtrl->Create(rect, this, 100);

		// fill it up with stuff
		m_pGridCtrl->SetEditable(TRUE);
		m_pGridCtrl->EnableDragAndDrop(TRUE);

		InitProfile();

		//try {
		//	m_pGridCtrl->SetRowCount(50);
		//	m_pGridCtrl->SetColumnCount(10);
		//	m_pGridCtrl->SetFixedRowCount(1);
		//	m_pGridCtrl->SetFixedColumnCount(1);
		//}
		//catch (CMemoryException* e)
		//{
		//	e->ReportError();
		//	e->Delete();
		//	return;
		//}

		//// fill rows/cols with text
		//for (int row = 0; row < m_pGridCtrl->GetRowCount(); row++)
		//	for (int col = 0; col < m_pGridCtrl->GetColumnCount(); col++)
		//	{ 
		//		GV_ITEM Item;
		//		Item.mask = GVIF_TEXT|GVIF_FORMAT;
		//		Item.row = row;
		//		Item.col = col;
		//		if (row < 1) {
		//			Item.nFormat = DT_LEFT|DT_WORDBREAK;
		//			Item.strText.Format(_T("Column %d"),col);
		//		} else if (col < 1) {
		//			Item.nFormat = DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;
		//			Item.strText.Format(_T("Row %d"),row);
		//		} else {
		//			Item.nFormat = DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;
		//			Item.strText.Format(_T("%d"),row*col);
		//		}
		//		m_pGridCtrl->SetItem(&Item);
		//}

		m_pGridCtrl->AutoSize();
	}
}

void CViewGrid::OnSize(UINT nType, int cx, int cy) 
{
	CView::OnSize(nType, cx, cy);
	
	if (m_pGridCtrl->GetSafeHwnd())
	{
		CRect rect;
		GetClientRect(rect);
		m_pGridCtrl->MoveWindow(rect);
	}
}

BOOL CViewGrid::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo) 
{
    if (m_pGridCtrl && IsWindow(m_pGridCtrl->m_hWnd))
        if (m_pGridCtrl->OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
            return TRUE;

	return CView::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}

//void CViewGrid::OnToggleReadonly()
//{
//    if (m_pGridCtrl && IsWindow(m_pGridCtrl->m_hWnd))
//    {
//        CCellID cell = m_pGridCtrl->GetFocusCell();
//        if (m_pGridCtrl->IsValid(cell))
//        {
//            int nState = m_pGridCtrl->GetItemState(cell.row, cell.col);
//            if (m_pGridCtrl->IsCellEditable(cell))
//                m_pGridCtrl->SetItemState(cell.row, cell.col, nState | GVIS_READONLY);
//            else
//                m_pGridCtrl->SetItemState(cell.row, cell.col, nState & ~GVIS_READONLY);
//        }
//    }
//}

BOOL CViewGrid::OnEraseBkgnd(CDC* pDC) 
{
	return TRUE;
	//return CView::OnEraseBkgnd(pDC);
}

//void CViewGrid::InitProfile(void)
//{
//	CMod3DDoc* pDoc;
//	CModel* pMod;
//	pDoc = GetDocument();			ASSERT(pDoc!=NULL);
//	pMod = pDoc->GetModel();		ASSERT(pMod!=NULL);
//
//	int modRow, modCol;
//	m_nFixGrdRows=4;
//	m_nFixGrdCols=1;
//
//	m_modRows = pMod->GetRows();
//	m_modCols = pMod->GetCols();
//	modRow=-1;
//	modCol=-1;
//
//	// get the "LAST" view
//	POSITION pos = pDoc->GetFirstViewPosition();
//	while (pos != NULL) {
//		CView* pView = pDoc->GetNextView(pos);
//		if( pView->IsKindOf(RUNTIME_CLASS( CViewProf))) {
//			CViewProf *pProfView = (CViewProf*) pView;
//			if(pProfView->GetProfType() == PRF_HRZ && GetFocus()==pView) {
//				modRow = pProfView->GetRow();
//				m_nPrfType = pProfView->GetProfType();
//			}
//		}
//	}
//
//	if(modRow==-1) return;
//	m_modRow = modRow;
//
//	// find maximum number of columns
//	int nMaxGrdRows=0;
//	for(int i=0; i<m_modCols; i++) {
//		int n = pMod->GetCount(modRow, i);
//		nMaxGrdRows = max(n, nMaxGrdRows);
//	}
//
//	try {
//		m_pGridCtrl->SetRowCount(nMaxGrdRows+m_nFixGrdRows);
//		m_pGridCtrl->SetColumnCount(m_modCols+m_nFixGrdCols);	//+1 fixed
//		m_pGridCtrl->SetFixedRowCount(m_nFixGrdRows);
//		m_pGridCtrl->SetFixedColumnCount(m_nFixGrdCols);
//	}
//	catch (CMemoryException* e)
//	{
//		e->ReportError();
//		e->Delete();
//		return;
//	}
//
//	// fill rows/cols with text
//	for (int col = 0; col < m_pGridCtrl->GetColumnCount(); col++)		{ 
//		for (int row = 0; row < 1; row++) {
//			GV_ITEM Item;
//			Item.mask = GVIF_TEXT|GVIF_FORMAT;
//			Item.row = row;
//			Item.col = col;
//			if (row < 1 && col>0) {
//				Item.nFormat = DT_LEFT|DT_WORDBREAK;
//				Item.strText.Format(_T("Column %d"),col-2);
//			} 
//			else if (col < 1 && row>=4) {
//				Item.nFormat = DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;
//				Item.strText.Format(_T("Row %d"),row-4);
//			} 
//			else if (col == 0 && row==1) {
//				Item.nFormat = DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;
//				Item.strText.Format(_T("x [m]"));
//			} 
//			else if (col == 0 && row==2) {
//				Item.nFormat = DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;
//				Item.strText.Format(_T("y [m]"));
//			} 
//			else if (col == 0 && row==3) {
//				Item.nFormat = DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;
//				Item.strText.Format(_T("z [m]"));
//			} 
//			//else {
//			//	Item.nFormat = DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;
//			//	Item.strText.Format(_T("%f"),row*col);
//			//}
//			m_pGridCtrl->SetItem(&Item);
//		}
//	}
//	for (int row = 0; row < m_pGridCtrl->GetRowCount(); row++) {
//		for (int col = 0; col < 1; col++)		{ 
//			GV_ITEM Item;
//			Item.mask = GVIF_TEXT|GVIF_FORMAT;
//			Item.row = row;
//			Item.col = col;
//			if (row < 1 && col>0) {
//				Item.nFormat = DT_LEFT|DT_WORDBREAK;
//				Item.strText.Format(_T("Column %d"),col-2);
//			} 
//			else if (col < 1 && row>=4) {
//				Item.nFormat = DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;
//				Item.strText.Format(_T("Row %d"),row-4);
//			} 
//			else if (col == 0 && row==1) {
//				Item.nFormat = DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;
//				Item.strText.Format(_T("x [m]"));
//			} 
//			else if (col == 0 && row==2) {
//				Item.nFormat = DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;
//				Item.strText.Format(_T("y [m]"));
//			} 
//			else if (col == 0 && row==3) {
//				Item.nFormat = DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;
//				Item.strText.Format(_T("z [m]"));
//			} 
//			//else {
//			//	Item.nFormat = DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;
//			//	Item.strText.Format(_T("%f"),row*col);
//			//}
//			m_pGridCtrl->SetItem(&Item);
//		}
//	}
//	for (int col = 1; col < m_pGridCtrl->GetColumnCount(); col++)		{ 
//		int n=pMod->GetCount(modRow, col-1);
//		for (int row = 1; row < n+2; row++) {
//			GV_ITEM Item;
//			Item.mask = GVIF_TEXT|GVIF_FORMAT;
//			Item.row = row;
//			Item.col = col;
//
//			if (row == 1) {
//				Item.nFormat = DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;
//				Item.strText.Format(_T("%f"), pMod->GetXe(col-1));
//			}
//			else if (row == 2) {
//				Item.nFormat = DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;
//				Item.strText.Format(_T("%f"), pMod->GetYe(modRow));
//			}
//			else {
//				Item.nFormat = DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;
//				Item.strText.Format(_T("%f"), pMod->GetZ(modRow, col-1, row-3));
//				CBody* pBody = pMod->GetBody(row-3, modRow, col-1);
//				if(pBody!=NULL) {
//					Item.crBkClr = pBody->GetColor();
//					Item.mask |= GVIF_BKCLR;
//				}
//			}
//
//			m_pGridCtrl->SetItem(&Item);
//		}
//		for (int row = n+2; row < m_pGridCtrl->GetRowCount(); row++) {
//			GV_ITEM Item;
//			Item.mask = GVIF_TEXT|GVIF_FORMAT;
//			Item.row = row;
//			Item.col = col;
//			Item.strText = "";
//			Item.crBkClr = RGB(255, 255, 255);
//			Item.mask |= GVIF_BKCLR;
//			m_pGridCtrl->SetItem(&Item);
//		}
//	}
//
//	// set window title
//	CString str1;
//	CString str;
//	int n;
//	if(m_nPrfType == PROF_X)	{
//		//m_nRC = mod->GetCols();
//		//m_cs = mod->GetSizeX();
//		str1 = "row ";
//		n = m_modRow;
//	}
//	if(m_nPrfType == PROF_Y)	{
//		//m_nRC = mod->GetRows();
//		//m_cs = mod->GetSizeY();
//		str1 = "column ";
//		n = m_modCol;
//	}
//	str.Format("%d", n);
//	GetParent()->SetWindowText(GetDocument()->GetTitle()+ ": Profile -> " + str1 + str);
//}


void CViewGrid::UpdateGrid()
{
	if(m_pGridCtrl==NULL) return;

	CModel* pMod=GetDocument()->GetModel();
	for (int col = 1; col < m_pGridCtrl->GetColumnCount(); col++)		{ 
		int n = pMod->GetCount(m_modRow, col-1);
		for (int row = 1; row < n+2; row++) {
			GV_ITEM Item;
			Item.mask = GVIF_TEXT|GVIF_FORMAT;
			Item.row = row;
			Item.col = col;

			if (row == 1) {
				Item.nFormat = DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;
				Item.strText.Format(_T("%f"), pMod->GetXe(col-1));
			}
			else if (row == 2) {
				Item.nFormat = DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;
				Item.strText.Format(_T("%f"), pMod->GetYe(m_modRow));
			}
			else {
				Item.nFormat = DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;
				Item.strText.Format(_T("%f"), pMod->GetZ(m_modRow, col-1, row-3));
				CBody* pBody = pMod->GetBody(row-3, m_modRow, col-1);
				if(pBody!=NULL) {
					Item.crBkClr = pBody->GetColor();
					Item.mask |= GVIF_BKCLR;
				}
			}

			m_pGridCtrl->SetItem(&Item);
		}
		for (int row = n+2; row < m_pGridCtrl->GetRowCount(); row++) {
			GV_ITEM Item;
			Item.mask = GVIF_TEXT|GVIF_FORMAT;
			Item.row = row;
			Item.col = col;
			Item.strText = "";
			Item.crBkClr = RGB(255, 255, 255);
			Item.mask |= GVIF_BKCLR;
			m_pGridCtrl->SetItem(&Item);
		}
	}
}

void CViewGrid::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
	switch( (int)lHint ) {
		case UPDATE_MOVE_PROFILE:
			//UpdateGrid();
			InitProfile();
			break;
		case UPDATE_MOVE_VERTEX:
			//UpdateGrid();
			InitProfile();
			break;
		case UPDATE_MOVE_EDGE: 
			InitProfile();
			break;
		case UPDATE_BODY_EDIT_DELETE:
			InitProfile();
			break;
		case UPDATE_COPY_PROFILE:
			InitProfile();
			break;
		case UPDATE_INSERT_BODY:
			InitProfile();
			break;
		case UPDATE_DELETE_BODY:
			InitProfile();
			break;
		case UPDATE_REMOVE_BODY:
			InitProfile();
			break;
		case UPDATE_REMOVE_BODY_FROM_PROF:
			InitProfile();
			break;
		default: 
			Invalidate();
	}
}



void CViewGrid::InitProfile(void)
{
	CMod3DDoc* pDoc;
	CModel* pMod;
	pDoc = GetDocument();			ASSERT(pDoc!=NULL);
	pMod = pDoc->GetModel();		ASSERT(pMod!=NULL);

	m_nFixGrdRows=4;
	m_nFixGrdCols=1;

	m_modRows = pMod->GetRows();
	m_modCols = pMod->GetCols();

	// get the "LAST" view
	BOOL bContinue=FALSE;
	POSITION pos = pDoc->GetFirstViewPosition();
	while (pos != NULL) {
		CView* pView = pDoc->GetNextView(pos);
		if( pView->IsKindOf(RUNTIME_CLASS( CViewProf))) {
			CViewProf *pProfView = (CViewProf*) pView;
			if(GetFocus()==pView) {
				m_modRow = pProfView->GetRow();
				m_modCol = pProfView->GetCol();
				m_nPrfType = pProfView->GetProfType();
				bContinue = TRUE;
				break;
			}
		}
	}

	if(!bContinue) {
		if(m_nPrfType==PRF_HRZ && m_modRow==-1)
			return;
		if(m_nPrfType==PRF_VRT && m_modCol==-1)
			return;
	}

	if(m_nPrfType==PRF_HRZ)
		InitProfileHRZ();
	if(m_nPrfType==PRF_VRT)
		InitProfileVRT();

	Invalidate();
}

void CViewGrid::InitProfileHRZ(void)
{
	CModel* pMod = GetDocument()->GetModel();
	ASSERT(pMod!=NULL);

	if(m_modRow==-1) return;

	// find maximum number of columns
	int nMaxGrdRows=0;
	for(int i=0; i<m_modCols; i++) {
		int n = pMod->GetCount(m_modRow, i);
		nMaxGrdRows = max(n, nMaxGrdRows);
	}

	try {
		m_pGridCtrl->SetRowCount(nMaxGrdRows+m_nFixGrdRows);
		m_pGridCtrl->SetColumnCount(m_modCols+m_nFixGrdCols);	//+1 fixed
		m_pGridCtrl->SetFixedRowCount(m_nFixGrdRows);
		m_pGridCtrl->SetFixedColumnCount(m_nFixGrdCols);
	}
	catch (CMemoryException* e)
	{
		e->ReportError();
		e->Delete();
		return;
	}

	// fill rows/cols with text
	for (int col = 0; col < m_pGridCtrl->GetColumnCount(); col++)		{ 
		int n=0;
		if(col>0)
			n=pMod->GetCount(m_modRow, col-1);
		for (int row = 0; row < m_pGridCtrl->GetRowCount(); row++) {
			GV_ITEM Item;
			Item.mask = GVIF_TEXT|GVIF_FORMAT;
			Item.row = row;
			Item.col = col;

			if (row==0 && col==0) {
			} 
			else if (row < 1 && col>0) {
				Item.nFormat = DT_LEFT|DT_WORDBREAK;
				Item.strText.Format(_T("Model Col %d"),col-2);
			} 
			else if (col < 1 && row>=4) {
				Item.nFormat = DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;
				Item.strText.Format(_T("Index %d"),row-4);
			} 
			else if (col == 0 && row==1) {
				Item.nFormat = DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;
				Item.strText.Format(_T("x [m]"));
			} 
			else if (col == 0 && row==2) {
				Item.nFormat = DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;
				Item.strText.Format(_T("y [m]"));
			} 
			else if (col == 0 && row==3) {
				Item.nFormat = DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;
				Item.strText.Format(_T("z [m]"));
			} 
			else if ( row<(n+2) && row==1) {
				Item.nFormat = DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;
				Item.strText.Format(_T("%f"), pMod->GetXe(col-1));
			}
			else if ( row<(n+2) && row == 2) {
				Item.nFormat = DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;
				Item.strText.Format(_T("%f"), pMod->GetYe(m_modRow));
			}
			else if( row<(n+2) )
			{
				Item.nFormat = DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;
				Item.strText.Format(_T("%f"), pMod->GetZ(m_modRow, col-1, row-3));
				CBody* pBody = pMod->GetBody(row-3, m_modRow, col-1);
				if(pBody!=NULL) {
					Item.crBkClr = pBody->GetColorFill();
					Item.mask |= GVIF_BKCLR;
				}
			}
			else {
				Item.mask = GVIF_TEXT|GVIF_FORMAT;
				Item.row = row;
				Item.col = col;
				Item.strText = "";
				Item.crBkClr = RGB(255, 255, 255);
				Item.mask |= GVIF_BKCLR;
			}
			m_pGridCtrl->SetItem(&Item);
		}
	}

	// set window title
	CString str1;
	CString str;
	int n;
	if(m_nPrfType == PROF_X)	{
		//m_nRC = mod->GetCols();
		//m_cs = mod->GetSizeX();
		str1 = "row ";
		n = m_modRow;
	}
	if(m_nPrfType == PROF_Y)	{
		//m_nRC = mod->GetRows();
		//m_cs = mod->GetSizeY();
		str1 = "column ";
		n = m_modCol;
	}
	str.Format("%d", n);
	GetParent()->SetWindowText(GetDocument()->GetTitle()+ ": Profile -> " + str1 + str);
}

void CViewGrid::InitProfileVRT(void)
{
	CModel* pMod = GetDocument()->GetModel();
	ASSERT(pMod!=NULL);

	if(m_modCol==-1) return;

	// find maximum number of columns
	int nMaxGrdCols=0;
	for(int i=0; i<m_modRows; i++) {
		int n = pMod->GetCount(i, m_modCol);
		nMaxGrdCols= max(n, nMaxGrdCols);
	}

	try {
		m_pGridCtrl->SetRowCount(nMaxGrdCols+m_nFixGrdRows);
		m_pGridCtrl->SetColumnCount(m_modRows+m_nFixGrdCols);	//+1 fixed
		m_pGridCtrl->SetFixedRowCount(m_nFixGrdRows);
		m_pGridCtrl->SetFixedColumnCount(m_nFixGrdCols);
	}
	catch (CMemoryException* e)
	{
		e->ReportError();
		e->Delete();
		return;
	}

	// fill rows/cols with text
	for (int col = 0; col < m_pGridCtrl->GetColumnCount(); col++)		{ 
		int n=0;
		if(col>0)
			n=pMod->GetCount(col-1, m_modCol);
		for (int row = 0; row < m_pGridCtrl->GetRowCount(); row++) {
			GV_ITEM Item;
			Item.mask = GVIF_TEXT|GVIF_FORMAT;
			Item.row = row;
			Item.col = col;

			if (row==0 && col==0) {
			} 
			else if (row < 1 && col>0) {
				Item.nFormat = DT_LEFT|DT_WORDBREAK;
				Item.strText.Format(_T("Model Row %d"), col-2);
			} 
			else if (col < 1 && row>=4) {
				Item.nFormat = DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;
				Item.strText.Format(_T("Index %d"), row-4);
			} 
			else if (col == 0 && row==1) {
				Item.nFormat = DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;
				Item.strText.Format(_T("x [m]"));
			} 
			else if (col == 0 && row==2) {
				Item.nFormat = DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;
				Item.strText.Format(_T("y [m]"));
			} 
			else if (col == 0 && row==3) {
				Item.nFormat = DT_RIGHT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;
				Item.strText.Format(_T("z [m]"));
			} 
			else if ( row<(n+2) && row==1) {
				Item.nFormat = DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;
				Item.strText.Format(_T("%f"), pMod->GetXe(m_modCol));
			}
			else if ( row<(n+2) && row == 2) {
				Item.nFormat = DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;
				Item.strText.Format(_T("%f"), pMod->GetYe(col-1));
			}
			else if( row<(n+2) )
			{
				Item.nFormat = DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX;
				Item.strText.Format(_T("%f"), pMod->GetZ(col-1, m_modCol, row-3));
				CBody* pBody = pMod->GetBody(row-3, col-1, m_modCol);
				if(pBody!=NULL) {
					Item.crBkClr = pBody->GetColorFill();
					Item.mask |= GVIF_BKCLR;
				}
			}
			else {
				Item.mask = GVIF_TEXT|GVIF_FORMAT;
				Item.row = row;
				Item.col = col;
				Item.strText = "";
				Item.crBkClr = RGB(255, 255, 255);
				Item.mask |= GVIF_BKCLR;
			}
			m_pGridCtrl->SetItem(&Item);
		}
	}

	// set window title
	CString str1;
	CString str;
	int n;
	if(m_nPrfType == PROF_X)	{
		//m_nRC = mod->GetCols();
		//m_cs = mod->GetSizeX();
		str1 = "row ";
		n = m_modRow;
	}
	if(m_nPrfType == PROF_Y)	{
		//m_nRC = mod->GetRows();
		//m_cs = mod->GetSizeY();
		str1 = "column ";
		n = m_modCol;
	}
	str.Format("%d", n);
	GetParent()->SetWindowText(GetDocument()->GetTitle()+ ": Profile -> " + str1 + str);
}

