// DlgPpBodyDescr.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgPpBodyDescr.h"


// CDlgPpBodyDescr dialog

IMPLEMENT_DYNAMIC(CDlgPpBodyDescr, CPropertyPage)
CDlgPpBodyDescr::CDlgPpBodyDescr()
	: CPropertyPage(CDlgPpBodyDescr::IDD)
	, m_strName(_T(""))
	, m_nID(0)
	, m_strDescription(_T(""))
{
}

CDlgPpBodyDescr::~CDlgPpBodyDescr()
{
}

void CDlgPpBodyDescr::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_NAME, m_strName);
	DDX_Text(pDX, IDC_EDIT_ID, m_nID);
	DDX_Text(pDX, IDC_EDIT_DESC, m_strDescription);
	DDX_Control(pDX, IDC_EDIT_DESC, m_editDescr);
}


BEGIN_MESSAGE_MAP(CDlgPpBodyDescr, CPropertyPage)
END_MESSAGE_MAP()


// CDlgPpBodyDescr message handlers

BOOL CDlgPpBodyDescr::OnInitDialog()
{
	CPropertyPage::OnInitDialog();

	m_editDescr.SetWindowText( m_strDescription );

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CDlgPpBodyDescr::OnOK()
{
	m_editDescr.GetWindowText(m_strDescription);

	CPropertyPage::OnOK();
}

BOOL CDlgPpBodyDescr::OnApply()
{
	return CPropertyPage::OnApply();
}

BOOL CDlgPpBodyDescr::OnSetActive()
{
	SetModified();

	return CPropertyPage::OnSetActive();
}
