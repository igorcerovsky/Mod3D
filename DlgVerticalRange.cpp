// DlgVerticalRange.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgVerticalRange.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CDlgVerticalRange dialog


CDlgVerticalRange::CDlgVerticalRange(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgVerticalRange::IDD, pParent)
	, m_dSc(1)
{
	m_dMinZ = 0.0;
	m_strMaxX = _T("");
	m_strMaxY = _T("");
	m_strMinX = _T("");
	m_strMinY = _T("");
	m_dMaxZ = 0.0;
}


void CDlgVerticalRange::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDlgVerticalRange)
	DDX_Text(pDX, IDC_EDIT_MIN_Z, m_dMinZ);
	DDX_Text(pDX, IDC_MAX_X, m_strMaxX);
	DDX_Text(pDX, IDC_MAX_Y, m_strMaxY);
	DDX_Text(pDX, IDC_MIN_X, m_strMinX);
	DDX_Text(pDX, IDC_MIN_Y, m_strMinY);
	DDX_Text(pDX, IDC_EDIT_MAX_Z, m_dMaxZ);
	//}}AFX_DATA_MAP
	DDX_Text(pDX, IDC_EDIT_SC, m_dSc);
}


BEGIN_MESSAGE_MAP(CDlgVerticalRange, CDialog)
	//{{AFX_MSG_MAP(CDlgVerticalRange)
	ON_WM_CTLCOLOR()
	//}}AFX_MSG_MAP
	ON_WM_DRAWITEM()
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDlgVerticalRange message handlers

void CDlgVerticalRange::OnOK() 
{
	UpdateData();
	if(m_dMaxZ < m_dMinZ) {
		AfxMessageBox("maximum < minimum");
	}
	else {
		CDialog::OnOK();
	}
}