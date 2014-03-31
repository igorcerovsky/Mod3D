// DlgPpBodyComp.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgPpBodyComp.h"


// CDlgPpBodyComp dialog

IMPLEMENT_DYNAMIC(CDlgPpBodyComp, CPropertyPage)
CDlgPpBodyComp::CDlgPpBodyComp()
	: CPropertyPage(CDlgPpBodyComp::IDD)
	, m_bActive(FALSE)
	, m_bLocked(FALSE)
{
}

CDlgPpBodyComp::~CDlgPpBodyComp()
{
}

void CDlgPpBodyComp::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Check(pDX, IDC_CHECK_ACTIVE, m_bActive);
	DDX_Check(pDX, IDC_CHECK_LOCK, m_bLocked);
}


BEGIN_MESSAGE_MAP(CDlgPpBodyComp, CPropertyPage)
END_MESSAGE_MAP()


// CDlgPpBodyComp message handlers

BOOL CDlgPpBodyComp::OnApply()
{
	// TODO: Add your specialized code here and/or call the base class

	return CPropertyPage::OnApply();
}

BOOL CDlgPpBodyComp::OnSetActive()
{
	SetModified();

	return CPropertyPage::OnSetActive();
}
