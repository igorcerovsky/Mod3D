// DlgPpBodyGrav.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgPpBodyGrav.h"


// CDlgPpBodyGrav dialog

IMPLEMENT_DYNAMIC(CDlgPpBodyGrav, CPropertyPage)
CDlgPpBodyGrav::CDlgPpBodyGrav()
	: CPropertyPage(CDlgPpBodyGrav::IDD)
	, m_dDensity(0)
	, m_dGradX(0)
	, m_dGradY(0)
	, m_dGradZ(0)
	, m_dOrigX(0)
	, m_dOrigY(0)
	, m_dOrigZ(0)
{
}

CDlgPpBodyGrav::~CDlgPpBodyGrav()
{
}

void CDlgPpBodyGrav::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_DENSITY, m_dDensity);
	DDX_Text(pDX, IDC_EDIT_GRD_X, m_dGradX);
	DDX_Text(pDX, IDC_EDIT_GRD_Y, m_dGradY);
	DDX_Text(pDX, IDC_EDIT_GRD_Z, m_dGradZ);
	DDX_Text(pDX, IDC_EDIT_DO_X, m_dOrigX);
	DDX_Text(pDX, IDC_EDIT_DO_Y, m_dOrigY);
	DDX_Text(pDX, IDC_EDIT_DO_Z, m_dOrigZ);
}


BEGIN_MESSAGE_MAP(CDlgPpBodyGrav, CPropertyPage)
	ON_CONTROL_RANGE(EN_CHANGE, IDC_EDIT_DENSITY, IDC_EDIT_DO_Z, OnGravClicked)
END_MESSAGE_MAP()


// CDlgPpBodyGrav message handlers

BOOL CDlgPpBodyGrav::OnApply()
{
	BOOL b = CPropertyPage::OnApply();

	CFrameWnd* pFrameWnd = STATIC_DOWNCAST(CFrameWnd, AfxGetMainWnd());
	pFrameWnd->SendMessage(WM_BODY_PROPERTIES, m_nBodyID, 0);
	SetModified(FALSE);

	return b;
}

BOOL CDlgPpBodyGrav::OnSetActive()
{
	//SetModified();

	return CPropertyPage::OnSetActive();
}

void CDlgPpBodyGrav::OnGravClicked(UINT nCmdID)
{
	SetModified();
}