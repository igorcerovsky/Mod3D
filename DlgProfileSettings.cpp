// DlgProfileSettings.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgProfileSettings.h"


// CDlgProfileSettings dialog

IMPLEMENT_DYNAMIC(CDlgProfileSettings, CDialog)
CDlgProfileSettings::CDlgProfileSettings(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgProfileSettings::IDD, pParent)
	, m_strName(_T(""))
{
}

CDlgProfileSettings::~CDlgProfileSettings()
{
}

void CDlgProfileSettings::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST, m_list);
	DDX_Text(pDX, IDC_EDIT_NAME, m_strName);
}


BEGIN_MESSAGE_MAP(CDlgProfileSettings, CDialog)
	ON_BN_CLICKED(IDC_BUTTON_SAVE, OnBnClickedButtonSave)
	ON_BN_CLICKED(IDC_BUTTON_LOAD, OnBnClickedButtonLoad)
	ON_BN_CLICKED(IDC_BUTTON_REMOVE, OnBnClickedButtonRemove)
	ON_BN_CLICKED(IDC_BUTTON_REMOVE_ALL, OnBnClickedButtonRemoveAll)
	ON_LBN_DBLCLK(IDC_LIST, OnLbnDblclkList)
	ON_BN_CLICKED(IDC_BUTTON_APPLY, OnBnClickedButtonApply)
END_MESSAGE_MAP()

// itnitialize dialog
BOOL CDlgProfileSettings::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_list.ResetContent();
	for(int i=0; i<m_pPrfSetArray->GetSize(); i++) {
		m_list.AddString( m_pPrfSetArray->GetAt(i).strSettingsName );
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}


// CDlgProfileSettings message handlers

void CDlgProfileSettings::OnBnClickedButtonSave()
{
	UpdateData();
	strcpy(m_prfSet.strSettingsName, m_strName);
	m_pPrfSetArray->Add(m_prfSet);
	OnInitDialog();
}

void CDlgProfileSettings::OnBnClickedButtonLoad()
{
	int nSel = m_list.GetCurSel();
	if( nSel != LB_ERR && nSel < m_pPrfSetArray->GetCount() ) {
		m_prfSet = m_pPrfSetArray->GetAt(nSel);
	}
	else {
		AfxMessageBox("Invalid selection", MB_OK | MB_ICONINFORMATION);
	}
	OnInitDialog();
}

void CDlgProfileSettings::OnBnClickedButtonRemove()
{
	int nSel = m_list.GetCurSel();
	if( nSel != LB_ERR && nSel < m_pPrfSetArray->GetCount() ) {
		m_pPrfSetArray->RemoveAt(nSel);
	}
	else {
		AfxMessageBox("Invalid selection", MB_OK | MB_ICONINFORMATION);
	}
	OnInitDialog();
}

void CDlgProfileSettings::OnBnClickedButtonRemoveAll()
{
	if( AfxMessageBox("Remove all setting from list?", MB_YESNO | MB_ICONQUESTION) == IDYES )
		m_pPrfSetArray->RemoveAll();
	OnInitDialog();
}

void CDlgProfileSettings::OnLbnDblclkList()
{
	int nSel = m_list.GetCurSel();
	if( nSel != LB_ERR && nSel < m_pPrfSetArray->GetCount() ) {
		m_prfSet = m_pPrfSetArray->GetAt(nSel);
		m_pView->LoadProfileSettings(m_prfSet);
		OnOK();
	}
	else {
		AfxMessageBox("Invalid selection", MB_OK | MB_ICONINFORMATION);
	}
}

void CDlgProfileSettings::OnBnClickedButtonApply()
{
	m_pView->LoadProfileSettings(m_prfSet);
}
