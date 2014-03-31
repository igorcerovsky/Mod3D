// DlgBodyExport.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgBodyExport.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CDlgBodyExport dialog


CDlgBodyExport::CDlgBodyExport(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgBodyExport::IDD, pParent)
{
	m_strFilePathBdy = _T("");
	m_strFilePathBot = _T("");
	m_strFilePathTop = _T("");
}


void CDlgBodyExport::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDlgBodyExport)
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CDlgBodyExport, CDialog)
	ON_BN_CLICKED(IDC_BUT_BDY, OnButBdy)
	ON_BN_CLICKED(IDC_BUT_TOP, OnButTop)
	ON_BN_CLICKED(IDC_BUT_BOT, OnButBot)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDlgBodyExport message handlers
void CDlgBodyExport::OnButBdy() 
{
	AfxMessageBox("Not available in the moment.");	
}

void CDlgBodyExport::OnButTop() 
{
	//if( m_pBody != NULL)
	//	m_pBody->Export(EF_TOP);
}

void CDlgBodyExport::OnButBot() 
{
	//if( m_pBody != NULL)
	//	m_pBody->Export(EF_BOTTOM);
}

BOOL CDlgBodyExport::PreCreateWindow(CREATESTRUCT& cs) 
{
	// TODO: Add your specialized code here and/or call the base class
	
	return CDialog::PreCreateWindow(cs);
}
