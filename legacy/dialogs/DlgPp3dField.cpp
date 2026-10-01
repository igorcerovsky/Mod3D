// DlgPp3dField.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgPp3dField.h"


// CDlgPp3dField dialog

IMPLEMENT_DYNAMIC(CDlgPp3dField, CPropertyPage)
CDlgPp3dField::CDlgPp3dField()
	: CPropertyPage(CDlgPp3dField::IDD)
	, m_bFldShow(FALSE)
	, m_bFldSolid(FALSE)
	, m_fFldAlpha(0)
	, m_strFldSel(_T(""))
	, m_nFldPlacement(0)
{
}

CDlgPp3dField::~CDlgPp3dField()
{
}

void CDlgPp3dField::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Check(pDX, IDC_CHECK_FIELD, m_bFldShow);
	DDX_Check(pDX, IDC_CHECK_SRF_SOLID, m_bFldSolid);
	DDX_Control(pDX, IDC_COMBO1, m_comboFld);
	DDX_Text(pDX, IDC_EDIT_ALPHA_FIELD, m_fFldAlpha);
	DDX_Control(pDX, IDC_SPIN_ALPHA_FLD, m_spinAlpha);
	DDX_CBString(pDX, IDC_COMBO1, m_strFldSel);
	DDX_Radio(pDX, IDC_SRF_TYPE, m_nFldPlacement);
}


BEGIN_MESSAGE_MAP(CDlgPp3dField, CPropertyPage)
	ON_BN_CLICKED(IDC_CHECK_FIELD, OnBnClickedCheckField)
	ON_BN_CLICKED(IDC_CHECK_SRF_SOLID, OnBnClickedCheckSrfSolid)
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_ALPHA_FLD, OnDeltaposSpinAlphaFld)
	ON_EN_CHANGE(IDC_EDIT_ALPHA_FIELD, OnEnChangeEditAlphaField)
	ON_CBN_SELCHANGE(IDC_COMBO1, OnCbnSelchangeCombo1)
	ON_WM_VSCROLL()
	ON_BN_CLICKED(IDC_SRF_TYPE, OnBnClickedSrfType)
	ON_BN_CLICKED(IDC_RADIO2, OnBnClickedRadio2)
	ON_BN_CLICKED(IDC_RADIO3, OnBnClickedRadio3)
END_MESSAGE_MAP()


// CDlgPp3dField message handlers

BOOL CDlgPp3dField::OnInitDialog()
{
	CPropertyPage::OnInitDialog();

	for(int i=0; i<m_strFld.GetCount(); i++) {
		m_comboFld.AddString( m_strFld.GetAt(i) );
	}

	int nAlpha = (int)(m_fFldAlpha*100);
	m_spinAlpha.SetRange( 0, 100 );
	m_spinAlpha.SetPos(nAlpha);

	UpdateData(FALSE);

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CDlgPp3dField::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	CString str;

	if(pScrollBar->GetDlgCtrlID() == IDC_SPIN_ALPHA_FLD) {
		m_fFldAlpha = (float)nPos/100.0f; 
		str.Format("%.2f", m_fFldAlpha);
		((CSpinButtonCtrl*) pScrollBar)->GetBuddy()->SetWindowText(str);
	}

	CPropertyPage::OnVScroll(nSBCode, nPos, pScrollBar);
}

BOOL CDlgPp3dField::OnApply()
{
	UpdateData();

	return CPropertyPage::OnApply();
}



void CDlgPp3dField::OnDeltaposSpinAlphaFld(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
	SetModified();
	*pResult = 0;
}

void CDlgPp3dField::OnBnClickedCheckField()		{	SetModified();	}
void CDlgPp3dField::OnBnClickedCheckSrfSolid()	{	SetModified();	}
void CDlgPp3dField::OnEnChangeEditAlphaField()	{	SetModified();	}
void CDlgPp3dField::OnCbnSelchangeCombo1()		{	SetModified();	}

void CDlgPp3dField::OnBnClickedSrfType()		{	SetModified();	}
void CDlgPp3dField::OnBnClickedRadio2()			{	SetModified();	}
void CDlgPp3dField::OnBnClickedRadio3()			{	SetModified();	}
