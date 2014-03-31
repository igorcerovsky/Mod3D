// DlgEditBodies.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgEditBodies.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CDlgEditBodies dialog


CDlgEditBodies::CDlgEditBodies(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgEditBodies::IDD, pParent)
{
	//{{AFX_DATA_INIT(CDlgEditBodies)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

BOOL CDlgEditBodies::Create() 
{
	// TODO: Add your specialized code here and/or call the base class
	
	return CDialog::Create(CDlgEditBodies::IDD);
}

void CDlgEditBodies::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDlgEditBodies)
	DDX_Control(pDX, IDC_SHOW, m_butShow);
	DDX_Control(pDX, IDC_LIST_BODIES, m_list);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CDlgEditBodies, CDialog)
	//{{AFX_MSG_MAP(CDlgEditBodies)
	ON_BN_CLICKED(IDC_ADD_NEW, OnAddNew)
	ON_BN_CLICKED(IDC_DELETE, OnDelete)
	ON_BN_CLICKED(IDC_PROPRTIES, OnProprties)
	ON_BN_CLICKED(IDC_IMPORT, OnImport)
	ON_BN_CLICKED(IDC_EXPORT, OnExport)
	ON_LBN_SELCHANGE(IDC_LIST_BODIES, OnSelchangeListBodies)
	ON_WM_PAINT()
	ON_BN_CLICKED(IDC_SHOW, OnShowBody)
	//}}AFX_MSG_MAP
	ON_WM_DRAWITEM()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDlgEditBodies message handlers
BOOL CDlgEditBodies::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	if(!m_pBodyArray->IsEmpty() ) {
		int n = m_pBodyArray->GetSize();
		for(int i = 0; i< n; i++) {
			CBody *pBody = m_pBodyArray->GetAt(i);
			CString str;
			str.Format("%d -> %s", pBody->GetID(), pBody->GetName() );
			m_list.AddString(str);
			if(m_pBody == pBody)
				m_list.SetCurSel(i);
		}
		if( m_pBody == NULL )
			m_pBody = m_pBodyArray->GetAt(0);
	}
	else 
		AfxMessageBox("No body is defined.");
	
	Invalidate();
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CDlgEditBodies::OnAddNew() 
{
	AfxMessageBox("Not implemented!");
	//CBody *pBody = m_pDoc->NewBody();	
	//m_list.AddString(pBody->GetID() + " -> " + pBody->GetName());
	//m_pBody = pBody;
	//Invalidate();
}

void CDlgEditBodies::OnDelete() 
{
	int nIndex = m_list.GetCurSel();
	if (nIndex != LB_ERR) {
		int res = AfxMessageBox("You are going to delete a body. Proceed?", MB_YESNO | MB_ICONQUESTION);
		if( res == IDYES) {
			CBody*	pBody = m_pBodyArray->GetAt(nIndex);
			m_pModel->DeleteBody(pBody->GetID());
			m_pDoc->UpdateAllViews(NULL, UPDATE_BODY_EDIT_DELETE);
			m_list.DeleteString(nIndex);
		}
		//TRACE("Selected body : %s\n", m_pBody->GetID());
	}
}

void CDlgEditBodies::OnProprties() 
{
	m_pBody->Properties();	
	m_pDoc->UpdateAllViews(NULL, UPDATE_PROPERTIES_REDRAW);
	Invalidate();
}

void CDlgEditBodies::OnShowBody() 
{
	m_pBody->ShowInv();
	m_pDoc->UpdateAllViews(NULL, UPDATE_PROPERTIES_REDRAW);
	Invalidate();
}

void CDlgEditBodies::OnImport() 
{
	// TODO: Add your control notification handler code here
	
}

void CDlgEditBodies::OnExport() 
{
	// TODO: Add your control notification handler code here
	
}

void CDlgEditBodies::OnSelchangeListBodies() 
{
	int nIndex = m_list.GetCurSel();
	if (nIndex != LB_ERR) {
		m_pBody = m_pBodyArray->GetAt(nIndex);
		Invalidate();
		//TRACE("Selected body : %s\n", m_pBody->GetID());
	}
}

void CDlgEditBodies::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
	CPen *pPen;
	CBrush *pBrush;
	dc.SetBkMode(TRANSPARENT);

	// body pen
	CPen penBody;
	penBody.CreatePenIndirect( m_pBody->GetPLogPen() );
	pPen = dc.SelectObject(&penBody);
	// body brush
	CBrush brushBoby;
	if(m_pBody->IsVisisble()) {
		brushBoby.CreateBrushIndirect( m_pBody->GetPLogBrush() );
		pBrush = dc.SelectObject(&brushBoby);
	}
	// else create hollow brush
	else {
		LOGBRUSH logBrush;
		logBrush.lbStyle = BS_SOLID;
		logBrush.lbColor = ::GetSysColor(COLOR_MENU);;
		logBrush.lbHatch = 0;
		brushBoby.CreateBrushIndirect( &logBrush );
		pBrush = dc.SelectObject(&brushBoby);
	}
	CRect wndRect;
	GetClientRect(wndRect);
	dc.Rectangle(wndRect);
	dc.SelectObject(pPen);
	dc.SelectObject(pBrush);
	
	// Do not call CDialog::OnPaint() for painting messages
}




void CDlgEditBodies::OnCancel() 
{
	CDialog::OnCancel();
}

void CDlgEditBodies::OnOK() 
{
	// TODO: Add extra validation here
	
	CDialog::OnOK();
}