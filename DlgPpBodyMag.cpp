// DlgPpBodyMag.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgPpBodyMag.h"


// CDlgPpBodyMag dialog

IMPLEMENT_DYNAMIC(CDlgPpBodyMag, CPropertyPage)
CDlgPpBodyMag::CDlgPpBodyMag()
	: CPropertyPage(CDlgPpBodyMag::IDD)
	, m_dSusc(0)
	, m_dRemIncl(0)
	, m_dRemDecl(0)
	, m_dRemInt(0)
{
}

CDlgPpBodyMag::~CDlgPpBodyMag()
{
}

void CDlgPpBodyMag::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_SUSCEPTIBILITY, m_dSusc);
	DDX_Text(pDX, IDC_EDIT_R_TOT, m_dRemInt);
	DDX_Text(pDX, IDC_EDIT_R_DECL, m_dRemDecl);
	DDX_Text(pDX, IDC_EDIT_R_INCL, m_dRemIncl);
}


BEGIN_MESSAGE_MAP(CDlgPpBodyMag, CPropertyPage)
END_MESSAGE_MAP()


// CDlgPpBodyMag message handlers

BOOL CDlgPpBodyMag::OnApply()
{
	// TODO: Add your specialized code here and/or call the base class

	return CPropertyPage::OnApply();
}

BOOL CDlgPpBodyMag::OnSetActive()
{
	SetModified();

	return CPropertyPage::OnSetActive();
}
