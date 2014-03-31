// DlgInducingField.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgInducingField.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CDlgInducingField dialog


CDlgInducingField::CDlgInducingField(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgInducingField::IDD, pParent)
{
	m_dDecl = 1.0;
	m_dIncl = 60.0;
	m_dTot = 48000.0;
}


void CDlgInducingField::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_I_DECL, m_dDecl);
	DDV_MinMaxDouble(pDX, m_dDecl, -180., 180.);
	DDX_Text(pDX, IDC_EDIT_I_INCL, m_dIncl);
	DDV_MinMaxDouble(pDX, m_dIncl, -90., 90.);
	DDX_Text(pDX, IDC_EDIT_I_TOT, m_dTot);
}


BEGIN_MESSAGE_MAP(CDlgInducingField, CDialog)
		// NOTE: the ClassWizard will add message map macros here
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDlgInducingField message handlers
