// DlgCheckPoint.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgCheckPoint.h"


// CDlgCheckPoint dialog

IMPLEMENT_DYNAMIC(CDlgCheckPoint, CDialog)
CDlgCheckPoint::CDlgCheckPoint(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgCheckPoint::IDD, pParent)
	, m_strDescription(_T("Check point"))
	, m_x(0)
	, m_y(0)
	, m_z(0)
	, m_nRow(0)
	, m_nCol(0)
	, m_bVisible(FALSE)
{
}

CDlgCheckPoint::~CDlgCheckPoint()
{
}

void CDlgCheckPoint::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_DESCRIPTION, m_strDescription);
	DDX_Text(pDX, IDC_EDIT_X, m_x);
	DDX_Text(pDX, IDC_EDIT_Y, m_y);
	DDX_Text(pDX, IDC_EDIT_z, m_z);
	DDX_Text(pDX, IDC_EDIT_COLUMN, m_nCol);
	DDX_Text(pDX, IDC_EDIT_ROW, m_nRow);
	DDX_Check(pDX, IDC_CHECK_VISIBLE, m_bVisible);
}


BEGIN_MESSAGE_MAP(CDlgCheckPoint, CDialog)
END_MESSAGE_MAP()


// CDlgCheckPoint message handlers
