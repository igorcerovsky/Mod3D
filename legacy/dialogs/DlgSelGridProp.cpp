// DlgSelGridProp.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgSelGridProp.h"


// CDlgSelGridProp dialog

IMPLEMENT_DYNAMIC(CDlgSelGridProp, CDialog)
CDlgSelGridProp::CDlgSelGridProp(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgSelGridProp::IDD, pParent)
{
	m_pDoc = NULL;
}

CDlgSelGridProp::~CDlgSelGridProp()
{
}

void CDlgSelGridProp::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_COMBO_FLD, m_combo);
}


BEGIN_MESSAGE_MAP(CDlgSelGridProp, CDialog)
	ON_CBN_SELCHANGE(IDC_COMBO_FLD, OnCbnSelchangeComboFld)
END_MESSAGE_MAP()


// CDlgSelGridProp message handlers

BOOL CDlgSelGridProp::OnInitDialog()
{
	CDialog::OnInitDialog();

	ASSERT(m_pDoc!=NULL);

	CStringArray strAr;

	m_pDoc->GetGridsNonEmptyString(strAr);

	if(strAr.IsEmpty()) {
		AfxMessageBox("Nothing to select.", MB_OK | MB_ICONINFORMATION);
		OnCancel();
	}

	for(int i=0; i<strAr.GetCount(); i++) {
		m_combo.AddString( strAr.GetAt(i) );
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CDlgSelGridProp::OnCbnSelchangeComboFld()
{
	int		nItem=CB_ERR, nGrd=-1;
	CString str;
	CGrid*	pGrd;

	nItem = m_combo.GetCurSel();

	if( nItem == CB_ERR )  return;
    
	m_combo.GetLBText(nItem, str);
	pGrd = m_pDoc->GetGrid(str, nGrd);

	if( pGrd == NULL ) return;

	pGrd->Properties();

	pGrd->SetModifiedFlag();
	m_pDoc->UpdateAllViews(NULL, (LPARAM) UPDATE_PROPERTIES_GRID_COLOR);
	
}
