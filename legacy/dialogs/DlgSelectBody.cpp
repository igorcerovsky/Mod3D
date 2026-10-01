// DlgSelectBody.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgSelectBody.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CDlgSelectBody dialog


CDlgSelectBody::CDlgSelectBody(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgSelectBody::IDD, pParent)
{
	//{{AFX_DATA_INIT(CDlgSelectBody)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	m_pBody = NULL;
	m_pBodyArr = NULL;
}


void CDlgSelectBody::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDlgSelectBody)
	DDX_Control(pDX, IDC_LIST_BODIES, m_list);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CDlgSelectBody, CDialog)
	//{{AFX_MSG_MAP(CDlgSelectBody)
	ON_LBN_SELCHANGE(IDC_LIST_BODIES, OnSelchangeListBodies)
	ON_WM_PAINT()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDlgSelectBody message handlers

BOOL CDlgSelectBody::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	// TODO: Add extra initialization here
	if( !m_pBodyArr ) return 0;

	if( !m_pBodyArr->IsEmpty() ) {
		for(int i=0; i<m_pBodyArr->GetCount(); i++ ) {
			CBody *pBody = m_pBodyArr->GetAt(i);
			m_list.AddString( (pBody->GetID() + " -> " + pBody->GetName() ) );
			if(m_pBody == pBody)
				m_list.SetCurSel(i);
		}
		if( m_pBody == NULL )
			m_pBody = m_pBodyArr->GetAt(0);
	}
	else 
		AfxMessageBox("No body is defined.");
	
	Invalidate();
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CDlgSelectBody::OnSelchangeListBodies() 
{
	int nIndex = m_list.GetCurSel();
	if (nIndex != LB_ERR) {
		m_pBody = m_pBodyArr->GetAt(nIndex);
		Invalidate();
		//TRACE("Selected body : %s\n", m_pBody->GetID());
	}
}

void CDlgSelectBody::OnPaint() 
{
	CPaintDC dc(this); // device context for painting
	CPen *pPen;
	CBrush *pBrush;
	dc.SetBkMode(TRANSPARENT);

	// body pen
	CPen penBody;
	penBody.CreatePenIndirect(m_pBody->GetPLogPen() );
	pPen = dc.SelectObject(&penBody);
	// body brush
	CBrush brushBoby;
	brushBoby.CreateBrushIndirect( m_pBody->GetPLogBrush() );
	pBrush = dc.SelectObject(&brushBoby);
	
	CRect wndRect;
	GetClientRect(wndRect);
	dc.Rectangle(wndRect);
	dc.SelectObject(pPen);
	dc.SelectObject(pBrush);
	// Do not call CDialog::OnPaint() for painting messages
}
