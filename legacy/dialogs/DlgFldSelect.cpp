// DlgFldSelect.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgFldSelect.h"


// CDlgFldSelect dialog

IMPLEMENT_DYNAMIC(CDlgFldSelect, CDialog)
CDlgFldSelect::CDlgFldSelect(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgFldSelect::IDD, pParent)
	, m_strCombo(_T(""))
{
}

CDlgFldSelect::~CDlgFldSelect()
{
}

void CDlgFldSelect::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_COMBO_FLD, m_comboFld);
	DDX_CBString(pDX, IDC_COMBO_FLD, m_strCombo);
}


BEGIN_MESSAGE_MAP(CDlgFldSelect, CDialog)
END_MESSAGE_MAP()


// CDlgFldSelect message handlers

BOOL CDlgFldSelect::OnInitDialog()
{
	CDialog::OnInitDialog();

	for(int i=0; i<m_strFld.GetCount(); i++) {
		m_comboFld.AddString( m_strFld.GetAt(i) );
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}
