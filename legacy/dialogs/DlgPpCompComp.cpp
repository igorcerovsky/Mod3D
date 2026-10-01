// DlgPpCompComp.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgPpCompComp.h"


// CDlgPpCompComp dialog

IMPLEMENT_DYNAMIC(CDlgPpCompComp, CPropertyPage)
CDlgPpCompComp::CDlgPpCompComp()
	: CPropertyPage(CDlgPpCompComp::IDD)
	, m_bSpherComp(FALSE)
	, m_nComputationType(0)
{
}

CDlgPpCompComp::~CDlgPpCompComp()
{
}

void CDlgPpCompComp::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Check(pDX, IDC_CHECK_SPHER_COMP, m_bSpherComp);
	DDX_Radio(pDX, IDC_RADIO_COMP_TYPE, m_nComputationType);
}


BEGIN_MESSAGE_MAP(CDlgPpCompComp, CPropertyPage)
END_MESSAGE_MAP()

