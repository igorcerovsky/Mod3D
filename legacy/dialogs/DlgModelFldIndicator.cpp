// DlgModelFldIndicator.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgModelFldIndicator.h"


// CDlgModelFldIndicator dialog

IMPLEMENT_DYNAMIC(CDlgModelFldIndicator, CDialog)
CDlgModelFldIndicator::CDlgModelFldIndicator(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgModelFldIndicator::IDD, pParent)
	, m_nFld(0)
{
}

CDlgModelFldIndicator::~CDlgModelFldIndicator()
{
}

void CDlgModelFldIndicator::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_CBIndex(pDX, IDC_COMBO_FLD_INDICATOR, m_nFld);
}


BEGIN_MESSAGE_MAP(CDlgModelFldIndicator, CDialog)
END_MESSAGE_MAP()


// CDlgModelFldIndicator message handlers
