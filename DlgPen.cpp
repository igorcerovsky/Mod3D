// DlgPen.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgPen.h"


// CDlgPen dialog

IMPLEMENT_DYNAMIC(CDlgPen, CDialog)
CDlgPen::CDlgPen(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgPen::IDD, pParent)
	, m_nWidth(0)
	, m_nStyle(0)
{
}

CDlgPen::~CDlgPen()
{
}

void CDlgPen::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_WIDTH, m_nWidth);
	DDV_MinMaxInt(pDX, m_nWidth, 0, 1000);
	DDX_Control(pDX, IDC_SPIN_WIDTH, m_spin);
	DDX_CBIndex(pDX, IDC_COMBO_STYLE, m_nStyle);
	DDX_Control(pDX, IDC_COMBO_STYLE, m_combStyle);
	DDX_Control(pDX, IDC_BUTTON_COLOR, m_butColor);
}


BEGIN_MESSAGE_MAP(CDlgPen, CDialog)
	//ON_BN_CLICKED(IDC_BUTTON_COLOR, OnBnClickedButtonColor)
END_MESSAGE_MAP()


// CDlgPen message handlers

//void CDlgPen::OnBnClickedButtonColor()
//{
//	CColorDialog dlg(m_color);
//	if( dlg.DoModal() == IDOK ) {
//		m_color = dlg.GetColor();
//	}
//}

BOOL CDlgPen::OnInitDialog()
{
	int nSel;

	m_butColor.SetColor(m_lp.lopnColor);
	m_nWidth = m_lp.lopnWidth.x;
	switch( m_lp.lopnStyle ) {
		case PS_SOLID:
			nSel = 0;
			break;
		case PS_DOT:
			nSel = 1;
			break;
		case PS_DASH:
			nSel = 2;
			break;
		case PS_DASHDOT:
			nSel = 3;
			break;
		case PS_DASHDOTDOT:
			nSel = 4;
			break;
		default:
			nSel = 0;
	}
	m_nStyle = nSel;
	UpdateData(FALSE);
	CDialog::OnInitDialog();

	m_spin.SetRange(0,1000);
	m_combStyle.SetCurSel(nSel);

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CDlgPen::OnOK()
{
	UpdateData();

	switch( m_nStyle ) {
		case 0:
			m_lp.lopnStyle = PS_SOLID;
			break;
		case 1:
			m_lp.lopnStyle = PS_DOT;
			break;
		case 2:
			m_lp.lopnStyle = PS_DASH;
			break;
		case 3:
			m_lp.lopnStyle = PS_DASHDOT;
			break;
		case 4:
			m_lp.lopnStyle = PS_DASHDOTDOT;
			break;
		default:
			m_lp.lopnStyle = PS_SOLID;
	}
	m_lp.lopnWidth.x = m_nWidth;
	m_lp.lopnColor = m_butColor.GetColor();

	CDialog::OnOK();
}
