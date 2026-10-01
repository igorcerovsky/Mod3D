// DlgStart.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgStart.h"


// CDlgStart dialog

IMPLEMENT_DYNAMIC(CDlgStart, CDialog)
CDlgStart::CDlgStart(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgStart::IDD, pParent)
	, m_nTag(0)
{
}

CDlgStart::~CDlgStart()
{
}

void CDlgStart::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Radio(pDX, IDC_RADIO1, m_nTag);
}


BEGIN_MESSAGE_MAP(CDlgStart, CDialog)
END_MESSAGE_MAP()


// CDlgStart message handlers
