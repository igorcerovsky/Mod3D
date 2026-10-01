// DlgInsExistBoby.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgInsExistBoby.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CDlgInsExistBoby dialog


CDlgInsExistBoby::CDlgInsExistBoby(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgInsExistBoby::IDD, pParent)
{
	m_dWidth = 1.0;
}


void CDlgInsExistBoby::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST_BODIES, m_list);
	DDX_Text(pDX, IDC_EDIT_WIDTH, m_dWidth);
}


BEGIN_MESSAGE_MAP(CDlgInsExistBoby, CDialog)
	ON_WM_PAINT()
	ON_LBN_SELCHANGE(IDC_LIST_BODIES, OnSelchangeListBodies)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDlgInsExistBoby message handlers

BOOL CDlgInsExistBoby::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	// TODO: Add extra initialization here
	if(!m_pBodyLst->IsEmpty()) {
		POSITION pos = m_pBodyLst->GetHeadPosition();
		while(pos) {
			CBody *pBody = m_pBodyLst->GetNext(pos);
			m_list.AddString( pBody->GetID() + " -> " + pBody->GetName() );
		}
		m_pBody = m_pBodyLst->GetHead();
		m_list.SetCurSel(0);
	}
	else 
		AfxMessageBox("No body is defined.");
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CDlgInsExistBoby::OnPaint() 
{
	CPen *pPen;
	CBrush *pBrush;
	CPaintDC dc(this); // device context for painting
	dc.SetBkMode(TRANSPARENT);

	if(m_pBody != NULL) {
		// body pen
		CPen penBody;
		penBody.CreatePenIndirect( m_pBody->GetPLogPen() );
		pPen = dc.SelectObject(&penBody);
		// body brush
		CBrush brushBoby;
		brushBoby.CreateBrushIndirect( m_pBody->GetPLogBrush() );
		pBrush = dc.SelectObject(&brushBoby);
		
		CRect clientRect;
		GetClientRect(clientRect);
		dc.Rectangle(clientRect);
		dc.SelectObject(pPen);
		dc.SelectObject(pBrush);
		// Do not call CDialog::OnPaint() for painting messages
	}
}

void CDlgInsExistBoby::OnSelchangeListBodies() 
{
	int nIndex = m_list.GetCurSel();
	if (nIndex != LB_ERR) {
		POSITION pos = m_pBodyLst->FindIndex(nIndex);
		m_pBody = m_pBodyLst->GetAt(pos);
		Invalidate();
		//TRACE("Selected body : %s\n", m_pBody->GetID());
	}
}
