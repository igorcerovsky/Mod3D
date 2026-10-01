// DlgCheckMark.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgCheckMark.h"

#include "DlgPen.h"


// CDlgCheckMark dialog

IMPLEMENT_DYNAMIC(CDlgCheckMark, CDialog)
CDlgCheckMark::CDlgCheckMark(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgCheckMark::IDD, pParent)
	, m_strText(_T("Check point"))
	, m_x(0)
	, m_y(0)
	, m_z(0)
	, m_nRow(0)
	, m_nCol(0)
	, m_bVisible(FALSE)
	, m_bShowText(FALSE)
	, m_nType(0)
{
	m_lp.lopnColor = RGB(100,100,100);
	m_lp.lopnStyle = PS_DOT;
	m_lp.lopnWidth.x = 0;
}

CDlgCheckMark::~CDlgCheckMark()
{
}

void CDlgCheckMark::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_DESCRIPTION, m_strText);
	DDX_Text(pDX, IDC_EDIT_X, m_x);
	DDX_Text(pDX, IDC_EDIT_Y, m_y);
	DDX_Text(pDX, IDC_EDIT_z, m_z);
	DDX_Text(pDX, IDC_EDIT_COLUMN, m_nCol);
	DDX_Text(pDX, IDC_EDIT_ROW, m_nRow);
	DDX_Check(pDX, IDC_CHECK_VISIBLE, m_bVisible);
	DDX_Check(pDX, IDC_CHECK_TEXT, m_bShowText);
	DDX_Radio(pDX, IDC_CHM_TYPE, m_nType);
}


BEGIN_MESSAGE_MAP(CDlgCheckMark, CDialog)
	ON_WM_DRAWITEM()
	ON_BN_CLICKED(IDC_BUTTON_PEN, OnBnClickedButtonPen)
END_MESSAGE_MAP()


// CDlgCheckMark message handlers

void CDlgCheckMark::OnBnClickedButtonPen()
{
	CDlgPen dlg;

	dlg.m_lp = m_lp;
	if(dlg.DoModal() == IDOK ) {
		m_lp = dlg.m_lp;
	}
}
