// DlgPp3dSaveLoad.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgPp3dSaveLoad.h"


// CDlgPp3dSaveLoad dialog

IMPLEMENT_DYNAMIC(CDlgPp3dSaveLoad, CPropertyPage)
CDlgPp3dSaveLoad::CDlgPp3dSaveLoad()
	: CPropertyPage(CDlgPp3dSaveLoad::IDD)
	, m_strSettings(_T(""))
{
}

CDlgPp3dSaveLoad::~CDlgPp3dSaveLoad()
{
}

void CDlgPp3dSaveLoad::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_NAME, m_strSettings);
	DDX_Control(pDX, IDC_LIST, m_list);
}


BEGIN_MESSAGE_MAP(CDlgPp3dSaveLoad, CPropertyPage)
	ON_BN_CLICKED(IDC_BUTTON_SAVE, OnBnClickedButtonSave)
	ON_BN_CLICKED(IDC_BUTTON_LOAD, OnBnClickedButtonLoad)
	ON_BN_CLICKED(IDC_BUTTON_REMOVE, OnBnClickedButtonRemove)
	ON_BN_CLICKED(IDC_BUTTON_REMOVE_ALL, OnBnClickedButtonRemoveAll)
	ON_LBN_DBLCLK(IDC_LIST, OnLbnDblclkList)
END_MESSAGE_MAP()


// CDlgPp3dSaveLoad message handlers

void CDlgPp3dSaveLoad::OnBnClickedButtonSave()
{
	UpdateData();
	CFrameWnd* pFrameWnd = STATIC_DOWNCAST(CFrameWnd, AfxGetMainWnd());
	CView* pView = pFrameWnd->GetActiveFrame()->GetActiveView();
	pView->SendMessage(WM_VIEW_3D_SAVE_SETTINGS, (WPARAM) &m_strSettings, 0);
	OnInitDialog();
}

void CDlgPp3dSaveLoad::OnBnClickedButtonLoad()
{
	int nSel = m_list.GetCurSel();
	if( nSel != LB_ERR && nSel < m_pView3DSet->GetCount() ) {
		CFrameWnd* pFrameWnd = STATIC_DOWNCAST(CFrameWnd, AfxGetMainWnd());
		CView* pView = pFrameWnd->GetActiveFrame()->GetActiveView();
		pView->SendMessage(WM_VIEW_3D_LOAD_SETTINGS, (WPARAM) nSel, 0);
		SetModified(FALSE);
	}
	else {
		AfxMessageBox("Invalid selection", MB_OK | MB_ICONINFORMATION);
	}
	OnInitDialog();
}

void CDlgPp3dSaveLoad::OnBnClickedButtonRemove()
{
	int nSel = m_list.GetCurSel();
	if( nSel != LB_ERR && nSel < m_pView3DSet->GetCount() ) {
		m_pView3DSet->RemoveAt(nSel);
	}
	else {
		AfxMessageBox("Invalid selection", MB_OK | MB_ICONINFORMATION);
	}
	OnInitDialog();
}

void CDlgPp3dSaveLoad::OnBnClickedButtonRemoveAll()
{
	if( AfxMessageBox("Remove all setting from list?", MB_YESNO | MB_ICONQUESTION) == IDYES )
		m_pView3DSet->RemoveAll();
	OnInitDialog();
}

BOOL CDlgPp3dSaveLoad::OnInitDialog()
{
	CPropertyPage::OnInitDialog();

	m_list.ResetContent();
	for(int i=0; i<m_pView3DSet->GetSize(); i++) {
		m_list.AddString( m_pView3DSet->GetAt(i).strSettingsName );
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CDlgPp3dSaveLoad::OnLbnDblclkList()
{
	int nSel = m_list.GetCurSel();
	if( nSel != LB_ERR && nSel < m_pView3DSet->GetCount() ) {
		CFrameWnd* pFrameWnd = STATIC_DOWNCAST(CFrameWnd, AfxGetMainWnd());
		CView* pView = pFrameWnd->GetActiveFrame()->GetActiveView();
		pView->SendMessage(WM_VIEW_3D_LOAD_SETTINGS, (WPARAM) nSel, 0);
		SetModified(FALSE);
	}
	else {
		AfxMessageBox("Invalid selection", MB_OK | MB_ICONINFORMATION);
	}
}
