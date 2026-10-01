// DlgGridProperties.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgGridProperties.h"



// CDlgGridProperties dialog

IMPLEMENT_DYNAMIC(CDlgGridProperties, CDialog)
CDlgGridProperties::CDlgGridProperties(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgGridProperties::IDD, pParent)
	, m_dMinX(0)
	, m_dMinY(0)
	, m_dMaxY(0)
	, m_dCsY(0)
	, m_dCsX(0)
	, m_dMinZ(0)
	, m_dMaxZ(0)
	, m_bHst(FALSE)
	, m_dMaxX(0)
	, m_strName(_T(""))
	, m_strFilePath(_T(""))
	, m_bCustomRange(FALSE)
	, m_dCustomMin(0)
	, m_dCustomMax(0)
{
}

CDlgGridProperties::~CDlgGridProperties()
{
}

void CDlgGridProperties::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_MIN_X, m_dMinX);
	DDX_Text(pDX, IDC_MIN_Y, m_dMinY);
	DDX_Text(pDX, IDC_MAX_Y, m_dMaxY);
	DDX_Text(pDX, IDC_CS_Y, m_dCsY);
	DDX_Text(pDX, IDC_CS_X, m_dCsX);
	DDX_Text(pDX, IDC_MIN_Z, m_dMinZ);
	DDX_Text(pDX, IDC_MAX_Z, m_dMaxZ);
	DDX_Check(pDX, IDC_HST_EQ, m_bHst);
	DDX_Text(pDX, IDC_MAX_X, m_dMaxX);
	DDX_Text(pDX, IDC_NAME, m_strName);
	DDX_Text(pDX, IDC_FILE_PATH, m_strFilePath);
	DDX_Check(pDX, IDC_CUSTOM_ENABLE, m_bCustomRange);
	DDX_Text(pDX, IDC_CUSTOM_MIN, m_dCustomMin);
	DDX_Text(pDX, IDC_CUSTOM_MAX, m_dCustomMax);
}


BEGIN_MESSAGE_MAP(CDlgGridProperties, CDialog)
	ON_BN_CLICKED(IDC_BROWSE, OnBnClickedBrowse)
	ON_BN_CLICKED(IDC_COLOR_TABLE, OnBnClickedColorTable)
	ON_BN_CLICKED(IDCANCEL, OnBnClickedCancel)
END_MESSAGE_MAP()


// CDlgGridProperties message handlers

BOOL CDlgGridProperties::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  Add extra initialization here

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CDlgGridProperties::OnBnClickedBrowse()
{
	m_pGrid->SaveAs();
	GetDlgItem(IDC_FILE_PATH)->SetWindowText( m_pGrid->GetFilePath() );
}

void CDlgGridProperties::OnBnClickedColorTable()
{
	UpdateData();
	if(m_bCustomRange) {
		m_pGrad->SetHstMin( m_dCustomMin );
		m_pGrad->SetHstMax( m_dCustomMax );
	}

	CDlgColorGrad dlg;

	dlg.m_pGrad = m_pGrad;
	dlg.m_dMin = m_dMinZ;
	dlg.m_dMax = m_dMaxZ;

	dlg.DoModal();
}

void CDlgGridProperties::OnBnClickedCancel()
{
	// TODO: Add your control notification handler code here
	OnCancel();
}
