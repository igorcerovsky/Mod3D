// DlgGridLines.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgGridLines.h"


// CDlgGridLines dialog

IMPLEMENT_DYNAMIC(CDlgGridLines, CDialog)
CDlgGridLines::CDlgGridLines(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgGridLines::IDD, pParent)
	, m_bHrz(FALSE)
	, m_bVrt(FALSE)
	, m_dTop(0)
	, m_dBot(0)
	, m_dStep(0)
{
}

CDlgGridLines::~CDlgGridLines()
{
}

void CDlgGridLines::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Check(pDX, IDC_CHECK_HRZ, m_bHrz);
	DDX_Check(pDX, IDC_CHECK_VRT, m_bVrt);
	DDX_Text(pDX, IDC_EDIT_TOP, m_dTop);
	DDX_Text(pDX, IDC_EDIT_BOT, m_dBot);
	DDX_Text(pDX, IDC_EDIT_STEP, m_dStep);
}


BEGIN_MESSAGE_MAP(CDlgGridLines, CDialog)
END_MESSAGE_MAP()


// CDlgGridLines message handlers
