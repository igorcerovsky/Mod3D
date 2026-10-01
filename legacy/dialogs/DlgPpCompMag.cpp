// DlgPpCompMag.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgPpCompMag.h"


// CDlgPpCompMag dialog

IMPLEMENT_DYNAMIC(CDlgPpCompMag, CPropertyPage)
CDlgPpCompMag::CDlgPpCompMag()
	: CPropertyPage(CDlgPpCompMag::IDD)
	, m_dTot(0)
	, m_dIncl(0)
	, m_dDecl(0)
	, m_dUnits(1.0)
	, m_bMeanMag(FALSE)
{
}

CDlgPpCompMag::~CDlgPpCompMag()
{
}

void CDlgPpCompMag::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_I_TOT2, m_dTot);
	DDX_Text(pDX, IDC_EDIT_I_INCL2, m_dIncl);
	DDX_Text(pDX, IDC_EDIT_I_DECL2, m_dDecl);
	DDX_Text(pDX, IDC_EDIT_UNITS_MAG, m_dUnits);
	DDX_Check(pDX, IDC_MAG_REM_MEAN, m_bMeanMag);
}


BEGIN_MESSAGE_MAP(CDlgPpCompMag, CPropertyPage)
END_MESSAGE_MAP()


// CDlgPpCompMag message handlers
