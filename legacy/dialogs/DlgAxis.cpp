// DlgAxis.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgAxis.h"


// CDlgAxis dialog

IMPLEMENT_DYNAMIC(CDlgAxis, CDialog)
CDlgAxis::CDlgAxis(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgAxis::IDD, pParent)
	, m_fMin(0)
	, m_fMax(0)
	, m_fDataMin(0)
	, m_fDataMax(0)
	, m_bAutoTL(FALSE)
	, m_bTickMajor(FALSE)
	, m_bLabelMajor(FALSE)
	, m_bTickMinor(FALSE)
	, m_bLabelMinor(FALSE)
	, m_nDigitsMinor(0)
	, m_nDigitsMajor(0)
	, m_fTickStart(0)
	, m_fTickStep(0)
	, m_nTicks(0)
	, m_nType(0)
	, m_bGLMajor(FALSE)
	, m_bGLMinor(FALSE)
{
}

CDlgAxis::~CDlgAxis()
{
}

void CDlgAxis::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_DATA_MIN, m_fMin);
	DDX_Text(pDX, IDC_EDIT_DATA_MAX, m_fMax);
	DDX_Text(pDX, IDC_EDIT_DATALMITS_MIN, m_fDataMin);
	DDX_Text(pDX, IDC_EDITDATALIMITS_MAX, m_fDataMax);
	DDX_Check(pDX, IDC_CHECK_AUTOMATIC_TL, m_bAutoTL);
	DDX_Check(pDX, IDC_CHECK_MAJOR_TICK, m_bTickMajor);
	DDX_Check(pDX, IDC_CHECK_MAJOR_LABEL, m_bLabelMajor);
	DDX_Check(pDX, IDC_CHECK_MINOR_TICK, m_bTickMinor);
	DDX_Check(pDX, IDC_CHECK_MINOR_LABEL, m_bLabelMinor);
	DDX_Text(pDX, IDC_EDIT_MINOR_DECIMALS, m_nDigitsMinor);
	DDX_Text(pDX, IDC_EDIT_MAJOR_DECIMALS, m_nDigitsMajor);
	DDX_Text(pDX, IDC_EDIT_MAJOR_START, m_fTickStart);
	DDX_Text(pDX, IDC_EDIT_MAJOR_INCREMENT, m_fTickStep);
	DDX_Text(pDX, IDC_EDIT_MAJOR_NUMBER, m_nTicks);
	DDX_Radio(pDX, IDC_RADIO_TYPE, m_nType);
	DDX_Check(pDX, IDC_CHECK_GL_MAJOR, m_bGLMajor);
	DDX_Check(pDX, IDC_CHECK_GL_MINOR, m_bGLMinor);
}


BEGIN_MESSAGE_MAP(CDlgAxis, CDialog)
	ON_BN_CLICKED(IDC_BUTTON_FONT_MINOR, OnBnClickedButtonFontMinor)
	ON_BN_CLICKED(IDC_BUTTON_FONT_MAJOR_LBL, OnBnClickedButtonFontMajorLbl)
	ON_BN_CLICKED(IDC_CHECK_AUTOMATIC_TL, OnBnClickedCheckAutomaticTl)
END_MESSAGE_MAP()


// CDlgAxis message handlers

void CDlgAxis::OnBnClickedButtonFontMinor()
{
	m_lfLabelMinor.lfHeight /= -10;
	CFontDialog dlg(&m_lfLabelMinor,CF_EFFECTS|CF_SCREENFONTS);
	dlg.DoModal();
	m_lfLabelMinor.lfHeight = dlg.GetSize();
}

void CDlgAxis::OnBnClickedButtonFontMajorLbl()
{
	m_lfLabelMajor.lfHeight /= -10;
	CFontDialog dlg(&m_lfLabelMajor,CF_EFFECTS|CF_SCREENFONTS);
	dlg.DoModal();
	m_lfLabelMajor.lfHeight = dlg.GetSize();
}

BOOL CDlgAxis::OnInitDialog()
{
	CDialog::OnInitDialog();

	if( m_bAutoTL ) {
		GetDlgItem(IDC_EDIT_MAJOR_START)->EnableWindow(FALSE);
		GetDlgItem(IDC_EDIT_MAJOR_INCREMENT)->EnableWindow(FALSE);
		GetDlgItem(IDC_EDIT_MAJOR_DECIMALS)->EnableWindow(FALSE);
		GetDlgItem(IDC_EDIT_DATALMITS_MIN)->EnableWindow(FALSE);
		GetDlgItem(IDC_EDITDATALIMITS_MAX)->EnableWindow(FALSE);
		GetDlgItem(IDC_EDIT_MAJOR_NUMBER)->EnableWindow(TRUE);
	}
	else {
		GetDlgItem(IDC_EDIT_MAJOR_START)->EnableWindow(TRUE);
		GetDlgItem(IDC_EDIT_MAJOR_INCREMENT)->EnableWindow(TRUE);
		GetDlgItem(IDC_EDIT_MAJOR_DECIMALS)->EnableWindow(TRUE);
		GetDlgItem(IDC_EDIT_DATALMITS_MIN)->EnableWindow(TRUE);
		GetDlgItem(IDC_EDITDATALIMITS_MAX)->EnableWindow(TRUE);
		GetDlgItem(IDC_EDIT_MAJOR_NUMBER)->EnableWindow(FALSE);
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CDlgAxis::OnOK()
{
	UpdateData();

	if(m_fDataMax <= m_fDataMin) {
		AfxMessageBox("Axis maximum MUST be > data minimum!", MB_OK | MB_ICONEXCLAMATION);
		return;
	}
	if(m_fMax <= m_fMin) {
		AfxMessageBox("Data maximum MUST be > data minimum!", MB_OK | MB_ICONEXCLAMATION);
		return;
	}
	if(m_fMax > m_fDataMax) {
		AfxMessageBox("Axis maximum MUST be >= data maximum!", MB_OK | MB_ICONEXCLAMATION);
		return;
	}
	if(m_fMin < m_fDataMin) {
		AfxMessageBox("Axis minimum MUST be <= data minimum!", MB_OK | MB_ICONEXCLAMATION);
		return;
	}

	CDialog::OnOK();
}



void CDlgAxis::OnBnClickedCheckAutomaticTl()
{
	UpdateData();

	if( m_bAutoTL ) {
		AfxMessageBox("Automatic data range is ENABLED!\nThe axis is under automatic control.", MB_OK | MB_ICONINFORMATION);
		GetDlgItem(IDC_EDIT_MAJOR_START)->EnableWindow(FALSE);
		GetDlgItem(IDC_EDIT_MAJOR_INCREMENT)->EnableWindow(FALSE);
		GetDlgItem(IDC_EDIT_MAJOR_DECIMALS)->EnableWindow(FALSE);
		GetDlgItem(IDC_EDIT_DATALMITS_MIN)->EnableWindow(FALSE);
		GetDlgItem(IDC_EDITDATALIMITS_MAX)->EnableWindow(FALSE);
		GetDlgItem(IDC_EDIT_MAJOR_NUMBER)->EnableWindow(TRUE);
	}
	else {
		AfxMessageBox("Automatic data range is DISABLED!\nThe axis is under your control.", MB_OK | MB_ICONINFORMATION);
		GetDlgItem(IDC_EDIT_MAJOR_START)->EnableWindow(TRUE);
		GetDlgItem(IDC_EDIT_MAJOR_INCREMENT)->EnableWindow(TRUE);
		GetDlgItem(IDC_EDIT_MAJOR_DECIMALS)->EnableWindow(TRUE);
		GetDlgItem(IDC_EDIT_DATALMITS_MIN)->EnableWindow(TRUE);
		GetDlgItem(IDC_EDITDATALIMITS_MAX)->EnableWindow(TRUE);
		GetDlgItem(IDC_EDIT_MAJOR_NUMBER)->EnableWindow(FALSE);
	}
}
