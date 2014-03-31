// DlgModelImportBody.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgModelImportBody.h"


// CDlgModelImportBody dialog

IMPLEMENT_DYNAMIC(CDlgModelImportBody, CDialog)
CDlgModelImportBody::CDlgModelImportBody(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgModelImportBody::IDD, pParent)
	, m_nTagUB(0)
	, m_nTagLB(0)
	, m_strGidFileUB(_T(""))
	, m_strGridFileLB(_T(""))
	, m_dUB(0)
	, m_dLB(0)
	, m_dDumMin(0)
	, m_dDumMax(0)
	, m_dConstUB(0)
	, m_dConstLB(0)
{
}

CDlgModelImportBody::~CDlgModelImportBody()
{
}

void CDlgModelImportBody::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Radio(pDX, IDC_RADIO_UB, m_nTagUB);
	DDX_Radio(pDX, IDC_RADIO_LB, m_nTagLB);
	DDX_Text(pDX, IDC_EDIT_UB_STR, m_strGidFileUB);
	DDX_Text(pDX, IDC_EDIT_LB_STR, m_strGridFileLB);
	DDX_Text(pDX, IDC_EDIT_UB_VAL, m_dUB);
	DDX_Text(pDX, IDC_EDIT_LB_VAL, m_dLB);
	DDV_MinMaxDouble(pDX, m_dUB, 0.001, 1.0e9);
	DDV_MinMaxDouble(pDX, m_dLB, 0.001, 1.0e9);
	DDX_Text(pDX, IDC_EDIT_DUMMY_MIN, m_dDumMin);
	DDX_Text(pDX, IDC_EDIT_DUMMY_MAX, m_dDumMax);
	DDX_Text(pDX, IDC_EDIT_UB_CONST, m_dConstUB);
	DDX_Text(pDX, IDC_EDIT_LB_CONST, m_dConstLB);
}


BEGIN_MESSAGE_MAP(CDlgModelImportBody, CDialog)
	ON_BN_CLICKED(IDC_BUTTON_UB, OnBnClickedButtonUb)
	ON_BN_CLICKED(IDC_BUTTON_LB, OnBnClickedButtonLb)
END_MESSAGE_MAP()


// CDlgModelImportBody message handlers

void CDlgModelImportBody::OnBnClickedButtonUb()
{
	OpenGrid( &m_grdUp, IDC_EDIT_UB_STR );
}

void CDlgModelImportBody::OnBnClickedButtonLb()
{
	OpenGrid( &m_grdDwn, IDC_EDIT_LB_STR );
}

void CDlgModelImportBody::OpenGrid(CGrid* pGrd, int nGrid)
{
	if( !pGrd->Open() ) {
		AfxMessageBox("Failed open grid!", MB_OK | MB_ICONEXCLAMATION);
		return;
	}
	if( !pGrd->IsSameSize(m_nRows, m_nCols, m_x0, m_y0, m_xSize, m_ySize) ) {
		AfxMessageBox("Grid size does not match!", MB_OK | MB_ICONEXCLAMATION);
		pGrd->Close();
		return;
	}

	// set new file path
	CEdit* pEdit;
	pEdit = (CEdit*)GetDlgItem( nGrid );
	pEdit->SetWindowText( pGrd->GetFilePath() );
}

void CDlgModelImportBody::OnOK()
{
	UpdateData();
	
	if( m_nTagUB == 0 && m_grdUp.IsEmpty() ) {
		AfxMessageBox("Specify upper boundary grid or another import tag.", MB_OK | MB_ICONEXCLAMATION);
		return;
	}

	if( m_nTagLB == 0 && m_grdDwn.IsEmpty() ) {
		AfxMessageBox("Specify lower boundary grid or another import tag.", MB_OK | MB_ICONEXCLAMATION);
		return;
	}

	if( m_nTagUB == 1 && m_nTagLB == 2 ||
		m_nTagUB == 1 && m_nTagLB == 3 ||
		m_nTagUB == 2 && m_nTagLB == 1 ||
		m_nTagUB == 3 && m_nTagLB == 1 ||
		m_nTagUB == 2 && m_nTagLB == 3 ||
		m_nTagUB == 3 && m_nTagLB == 2 ||
		m_nTagUB == 1 && m_nTagLB == 1 ||
		m_nTagUB == 2 && m_nTagLB == 2 ||
		m_nTagUB == 3 && m_nTagLB == 3
		) 
	{
		AfxMessageBox("Invalid import mag. Grid MUST be specified.", MB_OK | MB_ICONEXCLAMATION);
		return;
	}

    if( m_dDumMin >= m_dDumMax ) {
		AfxMessageBox("Dummy value minimum MUST be < Dummy value maximum", MB_OK | MB_ICONEXCLAMATION);
		return;
	}

	CDialog::OnOK();
}
