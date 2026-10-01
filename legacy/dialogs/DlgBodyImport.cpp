// DlgBodyImport.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgBodyImport.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CDlgBodyImport dialog


CDlgBodyImport::CDlgBodyImport(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgBodyImport::IDD, pParent)
{
	m_dLow = 0.0;
	m_dUp = 0.0;
	m_nLB = 0;
	m_nUB = 0;
	m_strFilePathLow = _T("Brovse bottom grid.");
	m_strFilePathUp = _T("Brovse top grid.");
	m_strFilePathLB = _T("");
	m_strFilePathUB = _T("");
}


void CDlgBodyImport::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_LB_VAL, m_dLow);
	DDX_Text(pDX, IDC_EDIT_UB_VAL, m_dUp);
	DDX_Radio(pDX, IDC_RADIO_LB, m_nLB);
	DDX_Radio(pDX, IDC_RADIO_UB, m_nUB);
	DDX_Text(pDX, IDC_EDIT_LB_STR, m_strFilePathLB);
	DDX_Text(pDX, IDC_EDIT_UB_STR, m_strFilePathUB);
}


BEGIN_MESSAGE_MAP(CDlgBodyImport, CDialog)
	ON_BN_CLICKED(IDC_BUTTON_LB, OnButtonLb)
	ON_BN_CLICKED(IDC_BUTTON_UB, OnButtonUb)
	ON_WM_CTLCOLOR()
	ON_BN_CLICKED(IDOK, OnBnClickedOk)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDlgBodyImport message handlers

void CDlgBodyImport::OnButtonLb() 
{
	m_grdBot.Open();
	if( !m_grdBot.IsEmpty() ) {
		SetDlgItemText(IDC_EDIT_LB_STR , m_grdBot.GetInfo().strFilePath);
	}
}

void CDlgBodyImport::OnButtonUb() 
{
	m_grdTop.Open();
	if( !m_grdTop.IsEmpty() ) {
		SetDlgItemText(IDC_EDIT_UB_STR , m_grdTop.GetInfo().strFilePath);
	}
	
}

void CDlgBodyImport::OnBnClickedOk()
{
	// TODO: Add your control notification handler code here
	OnOK();
}
