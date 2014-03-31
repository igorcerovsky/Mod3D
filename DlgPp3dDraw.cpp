// DlgPp3dDraw.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgPp3dDraw.h"


// CDlgPp3dDraw dialog

IMPLEMENT_DYNAMIC(CDlgPp3dDraw, CPropertyPage)
CDlgPp3dDraw::CDlgPp3dDraw()
	: CPropertyPage(CDlgPp3dDraw::IDD)
	, m_bBmpShow(FALSE)
	, m_bPrfShow(FALSE)
	, m_bPrfSolid(FALSE)
	, m_fAlphaPrf(0)
	, m_bRelShow(FALSE)
	, m_bRelSolid(FALSE)
	, m_bLeadHrzShow(FALSE)
	, m_bLeadHrzSolid(FALSE)
	, m_fAlphaLeadHrz(0)
	, m_fAlphaRel(0)
{
}

CDlgPp3dDraw::~CDlgPp3dDraw()
{
}

void CDlgPp3dDraw::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Check(pDX, IDC_CHECK_DRAW_BITMAPS, m_bBmpShow);
	DDX_Check(pDX, IDC_CHECK_PROFILE_PLANES, m_bPrfShow);
	DDX_Check(pDX, IDC_CHECK_SOLID_RELIEF3, m_bPrfSolid);
	DDX_Text(pDX, IDC_EDIT_ALPHA_PROF, m_fAlphaPrf);
	DDX_Control(pDX, IDC_SPIN_ALPHA_PROF, m_spinProf);
	DDX_Control(pDX, IDC_SPIN_ALPHA_REL, m_spinRel);
	DDX_Control(pDX, IDC_SPIN_ALPHA_LEADHRZ, m_spinLeadHrz);
	DDX_Check(pDX, IDC_CHECK_RELIEF, m_bRelShow);
	DDX_Check(pDX, IDC_CHECK_SOLID_RELIEF, m_bRelSolid);
	DDX_Check(pDX, IDC_CHECK_LEADING_HRZ, m_bLeadHrzShow);
	DDX_Check(pDX, IDC_CHECK_SOLID_LEADHRZ, m_bLeadHrzSolid);
	DDX_Text(pDX, IDC_EDIT_ALPHA_LEADHRZ, m_fAlphaLeadHrz);
	DDX_Text(pDX, IDC_EDIT_ALPHA_REL, m_fAlphaRel);
}


BEGIN_MESSAGE_MAP(CDlgPp3dDraw, CPropertyPage)
	ON_BN_CLICKED(IDC_CHECK_DRAW_BITMAPS, OnBnClickedCheckDrawBitmaps)
	ON_BN_CLICKED(IDC_CHECK_PROFILE_PLANES, OnBnClickedCheckProfilePlanes)
	ON_BN_CLICKED(IDC_CHECK_SOLID_RELIEF3, OnBnClickedCheckSolidRelief3)
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_ALPHA_PROF, OnDeltaposSpinAlphaProf)
	ON_BN_CLICKED(IDC_CHECK_RELIEF, OnBnClickedCheckRelief)
	ON_BN_CLICKED(IDC_CHECK_SOLID_RELIEF, OnBnClickedCheckSolidRelief)
	ON_BN_CLICKED(IDC_CHECK_LEADING_HRZ, OnBnClickedCheckLeadingHrz)
	ON_BN_CLICKED(IDC_CHECK_SOLID_LEADHRZ, OnBnClickedCheckSolidLeadhrz)
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_ALPHA_REL, OnDeltaposSpinAlphaRel)
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_ALPHA_LEADHRZ, OnDeltaposSpinAlphaLeadhrz)
	ON_WM_VSCROLL()
END_MESSAGE_MAP()


// CDlgPp3dDraw message handlers

BOOL CDlgPp3dDraw::OnInitDialog()
{
	CPropertyPage::OnInitDialog();

	int nAlpha = (int)(m_fAlphaPrf*100);
	m_spinProf.SetRange( 0, 100 );
	m_spinProf.SetPos(nAlpha);

	nAlpha = (int)(m_fAlphaRel*100);
	m_spinRel.SetRange( 0, 100 );
	m_spinRel.SetPos(nAlpha);

	nAlpha = (int)(m_fAlphaLeadHrz*100);
	m_spinLeadHrz.SetRange( 0, 100 );
	m_spinLeadHrz.SetPos(nAlpha);

	UpdateData(FALSE);

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CDlgPp3dDraw::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	CString str;

	if(pScrollBar->GetDlgCtrlID() == IDC_SPIN_ALPHA_PROF) {
		m_fAlphaPrf = (float)nPos/100.0f; 
		str.Format("%.2f", m_fAlphaPrf);
		((CSpinButtonCtrl*) pScrollBar)->GetBuddy()->SetWindowText(str);
	}

	if(pScrollBar->GetDlgCtrlID() == IDC_SPIN_ALPHA_REL) {
		m_fAlphaPrf = (float)nPos/100.0f; 
		str.Format("%.2f", m_fAlphaPrf);
		((CSpinButtonCtrl*) pScrollBar)->GetBuddy()->SetWindowText(str);
	}

	if(pScrollBar->GetDlgCtrlID() == IDC_SPIN_ALPHA_LEADHRZ) {
		m_fAlphaPrf = (float)nPos/100.0f; 
		str.Format("%.2f", m_fAlphaPrf);
		((CSpinButtonCtrl*) pScrollBar)->GetBuddy()->SetWindowText(str);
	}


	CPropertyPage::OnVScroll(nSBCode, nPos, pScrollBar);
}

BOOL CDlgPp3dDraw::OnApply()
{
	// TODO: Add your specialized code here and/or call the base class

	return CPropertyPage::OnApply();
}



void CDlgPp3dDraw::OnBnClickedCheckDrawBitmaps()	{ SetModified();	}
void CDlgPp3dDraw::OnBnClickedCheckProfilePlanes()	{ SetModified();	}
void CDlgPp3dDraw::OnBnClickedCheckSolidRelief3()	{ SetModified();	}
void CDlgPp3dDraw::OnBnClickedCheckRelief()			{ SetModified();	}
void CDlgPp3dDraw::OnBnClickedCheckSolidRelief()	{ SetModified();	}
void CDlgPp3dDraw::OnBnClickedCheckLeadingHrz()		{ SetModified();	}
void CDlgPp3dDraw::OnBnClickedCheckSolidLeadhrz()	{ SetModified();	}

void CDlgPp3dDraw::OnDeltaposSpinAlphaProf(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
	 SetModified();
	*pResult = 0;
}

void CDlgPp3dDraw::OnDeltaposSpinAlphaRel(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
	 SetModified();
	*pResult = 0;
}

void CDlgPp3dDraw::OnDeltaposSpinAlphaLeadhrz(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
	 SetModified();
	*pResult = 0;
}


