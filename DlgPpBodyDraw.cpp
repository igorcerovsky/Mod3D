// DlgPpBodyDraw.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgPpBodyDraw.h"
#include "DlgPen.h"


// CDlgPpBodyDraw dialog

IMPLEMENT_DYNAMIC(CDlgPpBodyDraw, CPropertyPage)
CDlgPpBodyDraw::CDlgPpBodyDraw()
	: CPropertyPage(CDlgPpBodyDraw::IDD)
	, m_bTransparent(FALSE)
	, m_bFill(FALSE)
	, m_bVisible3D(FALSE)
	, m_fAlpha(0)
	, m_bInit(FALSE)
{
}

CDlgPpBodyDraw::~CDlgPpBodyDraw()
{
}

void CDlgPpBodyDraw::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Check(pDX, IDC_CHECK_TRASPARENT, m_bTransparent);
	DDX_Check(pDX, IDC_CHECK_FILLED, m_bFill);
	DDX_Check(pDX, IDC_CHECK_VISIBLE_3D, m_bVisible3D);
	DDX_Text(pDX, IDC_EDIT_ALPHA, m_fAlpha);
	DDX_Control(pDX, IDC_COMBO_BRUSH_STYLE, m_comboBrushStyle);
	DDX_Control(pDX, IDC_COMBO_BRUSH, m_comboBrushHatch);
	DDX_Control(pDX, IDC_SPIN_ALPHA, m_spinAlpha);
	DDX_Control(pDX, IDC_BUTTON_BRUSH, m_butBrush);
}


BEGIN_MESSAGE_MAP(CDlgPpBodyDraw, CPropertyPage)
	ON_BN_CLICKED(IDC_BUTTON_PEN, OnBnClickedButtonPen)
	ON_BN_CLICKED(IDC_BUTTON_PEN_NEXT, OnBnClickedButtonPenNext)
	ON_BN_CLICKED(IDC_BUTTON_PEN_PREV, OnBnClickedButtonPenPrev)
	ON_WM_VSCROLL()
	ON_WM_LBUTTONDOWN()
END_MESSAGE_MAP()


// CDlgPpBodyDraw message handlers

void CDlgPpBodyDraw::OnBnClickedButtonPen()
{
	CDlgPen dlg;

	dlg.m_lp = m_logPen;
	dlg.DoModal();
	m_logPen = dlg.m_lp;
	SetModified();
}

void CDlgPpBodyDraw::OnBnClickedButtonPenNext()
{
	CDlgPen dlg;

	dlg.m_lp = m_logPenPrev;
	dlg.DoModal();
	m_logPenPrev = dlg.m_lp;
	SetModified();
}

void CDlgPpBodyDraw::OnBnClickedButtonPenPrev()
{
	CDlgPen dlg;

	dlg.m_lp = m_logPenPrev;
	dlg.DoModal();
	m_logPenNext = dlg.m_lp;
	SetModified();
}

BOOL CDlgPpBodyDraw::OnInitDialog()
{
	CPropertyPage::OnInitDialog();

	m_butBrush.SetColor(m_logBrush.lbColor);
	switch( m_logBrush.lbStyle) {
		case BS_SOLID:
			m_comboBrushStyle.SetCurSel(0);
			break;
		case BS_HATCHED:
			m_comboBrushStyle.SetCurSel(1);
			break;
		case BS_HOLLOW:
			m_comboBrushStyle.SetCurSel(2);
			break;
	}
	switch( m_logBrush.lbHatch ) {
		case HS_CROSS:
			m_comboBrushHatch.SetCurSel(0);
			break;
		case HS_BDIAGONAL:
			m_comboBrushHatch.SetCurSel(1);
			break;
		case HS_FDIAGONAL:
			m_comboBrushHatch.SetCurSel(2);
			break;
		case HS_HORIZONTAL:
			m_comboBrushHatch.SetCurSel(3);
			break;
		case HS_VERTICAL:
			m_comboBrushHatch.SetCurSel(4);
			break;
		case HS_DIAGCROSS:
			m_comboBrushHatch.SetCurSel(5);
			break;
		default:
			m_comboBrushHatch.SetCurSel(0);
	}

	int nAlpha = (int)(m_fAlpha*100);
	m_spinAlpha.SetRange( 0, 100 );
	m_spinAlpha.SetPos(nAlpha);
	SetModified(FALSE);
	m_bInit = TRUE;

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CDlgPpBodyDraw::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	CString str;

	if(pScrollBar->GetDlgCtrlID() == IDC_SPIN_ALPHA) {
		m_fAlpha = (float)nPos/100.0f; 
		str.Format("%.2f", m_fAlpha);
		((CSpinButtonCtrl*) pScrollBar)->GetBuddy()->SetWindowText(str);
	}

	SetModified();
	CPropertyPage::OnVScroll(nSBCode, nPos, pScrollBar);
}

BOOL CDlgPpBodyDraw::OnApply()
{
	m_logBrush.lbColor = m_butBrush.GetColor();

	return CPropertyPage::OnApply();
}

BOOL CDlgPpBodyDraw::OnSetActive()
{
	SetModified();

	return CPropertyPage::OnSetActive();
}


void CDlgPpBodyDraw::OnLButtonDown(UINT nFlags, CPoint point)
{
	SetModified();

	//CPropertyPage::OnLButtonDown(nFlags, point);
}

void CDlgPpBodyDraw::OnOK()
{

	if( m_bInit ) {
		m_logBrush.lbColor = m_butBrush.GetColor();

		switch( m_comboBrushStyle.GetCurSel() ) {
			case 0:
				m_logBrush.lbStyle = BS_SOLID;
				break;
			case 1 :
				m_logBrush.lbStyle = BS_HATCHED;
				break;
			case 2:
				m_logBrush.lbStyle = BS_HOLLOW;
				break;
			default:
				m_logBrush.lbStyle = BS_SOLID;
		}
		switch( m_logBrush.lbHatch ) {
			case 0:
				m_logBrush.lbHatch = HS_CROSS;
				break;
			case 1:
				m_logBrush.lbHatch = HS_BDIAGONAL;
				break;
			case 2:
				m_logBrush.lbHatch = HS_FDIAGONAL;
				break;
			case 3:
				m_logBrush.lbHatch = HS_HORIZONTAL;
				break;
			case 4:
				m_logBrush.lbHatch = HS_VERTICAL;
				break;
			case 5:
				m_logBrush.lbHatch = HS_DIAGCROSS;
				break;
			default:
				m_logBrush.lbHatch = HS_CROSS;
		}
	}
	else
		m_butBrush.SetColor(m_logBrush.lbColor);

	CPropertyPage::OnOK();

}
