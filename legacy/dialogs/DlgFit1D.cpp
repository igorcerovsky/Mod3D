// DlgFit1D.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgFit1D.h"


// CDlgFit1D dialog

IMPLEMENT_DYNAMIC(CDlgFit1D, CDialog)
CDlgFit1D::CDlgFit1D(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgFit1D::IDD, pParent)
	, m_dEpsBrak(0.1)
	, m_dFitTol(3.0e-8)
	, m_strFld(_T(""))
	, m_bFitLog(FALSE)
	, m_nFitType(0)
	, m_nMethod(0)
	, m_nMaxIter(100)
	, m_bAutoBracket(FALSE)
	, m_dAutoBraket(0.1)
	, m_strLogFile(_T(""))
{
}

CDlgFit1D::~CDlgFit1D()
{
}

void CDlgFit1D::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_BRAK_EPS, m_dEpsBrak);
	DDX_Text(pDX, IDC_EDIT_TOL, m_dFitTol);
	DDV_MinMaxDouble(pDX, m_dFitTol, 3.0e-8, 1.0e15);
	DDX_Control(pDX, IDC_COMBO_FLD, m_combo);
	DDX_CBString(pDX, IDC_COMBO_FLD, m_strFld);
	DDX_Check(pDX, IDC_CHECK_LOG_FILE, m_bFitLog);
	DDX_Radio(pDX, IDC_RADIO_FIT, m_nFitType);
	DDX_Radio(pDX, IDC_RADIO_METHOD, m_nMethod);
	DDX_Text(pDX, IDC_EDIT_ITER, m_nMaxIter);
	DDV_MinMaxInt(pDX, m_nMaxIter, 1, 1000000);
	DDX_Check(pDX, IDC_CHECK_AUTO_BRACKET, m_bAutoBracket);
	DDX_Text(pDX, IDC_EDIT_AUTO_BRACKET, m_dAutoBraket);
	DDV_MinMaxDouble(pDX, m_dAutoBraket, 1.0e-9, 1.0);
	DDX_Text(pDX, IDC_EDIT_LOG, m_strLogFile);
}


BEGIN_MESSAGE_MAP(CDlgFit1D, CDialog)
	ON_BN_CLICKED(IDC_BUT_LOG_BROWSE, OnBnClickedButLogBrowse)
END_MESSAGE_MAP()


// CDlgFit1D message handlers

BOOL CDlgFit1D::OnInitDialog()
{
	CDialog::OnInitDialog();

	int i;
	if(!m_strAr.IsEmpty()) {
		for(i=0; i<m_strAr.GetSize(); i++) {
			CString str;
			str = m_strAr.GetAt(i);
			m_combo.AddString(str);
			if(str== m_strFld)
				m_combo.SetCurSel(i);
		}
	}
	else {
		AfxMessageBox("Difference field not found. Canceling.", MB_OK | MB_ICONINFORMATION);
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}


void CDlgFit1D::OnBnClickedButLogBrowse()
{
	CString strPath;
	CFileDialog dlg(TRUE, NULL, NULL, OFN_OVERWRITEPROMPT);
	if( dlg.DoModal() == IDOK ) {
		strPath = dlg.GetPathName();
		if(!strPath.IsEmpty()) {
			m_strLogFile = strPath;

			// set new file path
			CEdit* pEdit;
			pEdit = (CEdit*)GetDlgItem( IDC_EDIT_LOG );
			pEdit->SetWindowText( strPath );
		}
	}
}
