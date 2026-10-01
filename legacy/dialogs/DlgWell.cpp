// DlgWell.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgWell.h"
#include "DlgPen.h"


// CDlgWell dialog

IMPLEMENT_DYNAMIC(CDlgWell, CDialog)
CDlgWell::CDlgWell(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgWell::IDD, pParent)
	, m_nDrawWidth(50)
	, m_nDrawSide(0)
	, m_dRadius(0)
	, m_bSolid3D(FALSE)
	, m_nDrawWidth3D(0)
	, m_dRadiusLog(0)
	, m_dRadiusLog3D(0)
	, m_nSamplingRing(0)
	, m_nSamplingLog(0)
	, m_dPieStart(0)
	, m_dPieEnd(0)
{
	m_pWell = NULL;
}

CDlgWell::~CDlgWell()
{
}

void CDlgWell::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST_WELL, m_listWell);
	DDX_Control(pDX, IDC_LIST_LITHOLOGY, m_listLith);
	DDX_Control(pDX, IDC_LIST_DRAW_CHAN, m_listDrawChan);
	DDX_Text(pDX, IDC_DRAW_WIDTH, m_nDrawWidth);
	DDX_CBIndex(pDX, IDC_COMBO_DRAW_SIDE, m_nDrawSide);
	DDX_Text(pDX, IDC_RADIUS, m_dRadius);
	DDX_Check(pDX, IDC_CHECK_SOLID_3D, m_bSolid3D);
	DDX_Text(pDX, IDC_DRAW_WIDTH_3D, m_nDrawWidth3D);
	DDX_Text(pDX, IDC_RADIUS_LOG, m_dRadiusLog);
	DDX_Text(pDX, IDC_RADIUS_LOG_3D, m_dRadiusLog3D);
	DDX_Text(pDX, IDC_SAMPLING_RING, m_nSamplingRing);
	DDX_Text(pDX, IDC_LOG_SAMPLING, m_nSamplingLog);
	DDV_MinMaxInt(pDX, m_nSamplingLog, 1, 999);
	DDX_Text(pDX, IDC_PIE_START, m_dPieStart);
	DDV_MinMaxDouble(pDX, m_dPieStart, 0, 360);
	DDX_Text(pDX, IDC_PIE_END, m_dPieEnd);
	DDV_MinMaxDouble(pDX, m_dPieEnd, 0, 360);
}


BEGIN_MESSAGE_MAP(CDlgWell, CDialog)
	ON_NOTIFY(NM_DBLCLK, IDC_LIST_DRAW_CHAN, OnNMDblclkListDrawChan)
	ON_NOTIFY(NM_DBLCLK, IDC_LIST_LITHOLOGY, OnNMDblclkListLithology)
END_MESSAGE_MAP()


// CDlgWell message handlers

BOOL CDlgWell::OnInitDialog()
{
	CDialog::OnInitDialog();

	if( m_pWell == NULL ) {
		OnCancel();
		return 0;
	}

	int i, j;

	//long lStyle, lStyleOld;
	//lStyle = LVS_REPORT;
	//lStyleOld = GetWindowLong(m_listWell.m_hWnd, GWL_STYLE);
	//lStyleOld &= ~(LVS_TYPEMASK);  // turn off all the style (view mode) bits
	//lStyleOld |= lStyle;        // Set the new style for the control
	//SetWindowLong(m_listWell.m_hWnd, GWL_STYLE, lStyleOld);

	m_listDrawChan.SetExtendedStyle( m_listDrawChan.GetExtendedStyle() | LVS_EX_CHECKBOXES );
	WELLIT* pWl;
	
	for(i=0; i<m_pWell->m_nFormat-1; i++) {
		CString strDraw;
		m_listDrawChan.InsertItem(i, m_pWell->m_strData.GetAt(i) );
		m_listDrawChan.SetCheck(i, m_pWell->m_showChannel.GetAt(i) );
	}

	// setcolunms
	CRect rect;
	m_listWell.GetClientRect(&rect);
	int colWidth = int ((rect.Width()-m_pWell->m_nFormat*2) * 1.0/m_pWell->m_nFormat);
	if(colWidth < 40) colWidth=20;
	m_listWell.InsertColumn(0, "", LVCFMT_LEFT, colWidth, 0);
	for(j=0; j<m_pWell->m_strData.GetCount(); j++) {
		m_listWell.InsertColumn(j+1, m_pWell->m_strData.GetAt(j), LVCFMT_LEFT, colWidth, 0);
	}

	LV_ITEM lvitem;
	int n = m_pWell->m_data.GetSize() / m_pWell->m_nFormat;
	CString	str;
	for(i=0; i<n; i++) {
		lvitem.mask = LVIF_TEXT;
		lvitem.iItem = i;
		lvitem.iSubItem = 0;
		str.Format("%d", i);
		lvitem.pszText = str.GetBuffer();
		m_listWell.InsertItem(&lvitem);
		for(j=0; j<m_pWell->m_nFormat; j++) {
			str.Format("%g", m_pWell->m_data.GetAt(i*m_pWell->m_nFormat + j));
			lvitem.mask = LVIF_TEXT;
			lvitem.iItem = i;
			lvitem.iSubItem = j+1;
			lvitem.pszText = str.GetBuffer();
			m_listWell.SetItem(&lvitem);
		}
	}

	// setcolunms
	m_listLith.GetWindowRect(&rect);
	m_listLith.InsertColumn(0, "Lithology type", LVCFMT_LEFT, rect.Width() * 1/2, 0);
	m_listLith.InsertColumn(1, "ID", LVCFMT_LEFT, rect.Width() * 1/2, 0);

	n = m_pWell->m_welLit.GetSize();
	for(i=0; i<n; i++) {
		pWl = &m_pWell->m_welLit.GetAt(i);

		lvitem.mask = LVIF_TEXT;
		lvitem.iItem = i;
		lvitem.iSubItem = 0;
		lvitem.pszText = pWl->strType;
		m_listLith.InsertItem(&lvitem);

		lvitem.mask = LVIF_TEXT;
		lvitem.iItem = i;
		lvitem.iSubItem = 1;
		str.Format("%d", pWl->nType);
		lvitem.pszText = str.GetBuffer();
		m_listLith.SetItem(&lvitem);
	}

	CComboBox* pCombo = (CComboBox*)GetDlgItem(IDC_COMBO_DRAW_SIDE);
	pCombo->SetCurSel( m_nDrawSide );
	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CDlgWell::OnOK()
{
	UpdateData();

	if( m_dPieEnd < m_dPieStart ) {
		AfxMessageBox("Pie start MUST be < then Pie end!", MB_OK | MB_ICONEXCLAMATION );
		CEdit* pEdit;
		pEdit = (CEdit*) GetDlgItem(IDC_PIE_START);
		GotoDlgCtrl( pEdit );
		return;
	}

	for(int i=0; i<m_pWell->m_nFormat-1; i++) {
		m_pWell->m_showChannel[i] = m_listDrawChan.GetCheck(i);
	}

	CDialog::OnOK();
}


void CDlgWell::OnNMDblclkListDrawChan(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMITEMACTIVATE lpi = (LPNMITEMACTIVATE) pNMHDR;
	
	int iItem = lpi->iItem;
	if(iItem < 4) return;

	CDlgPen dlg;
	CPen* pPen;
	pPen = &(m_pWell->m_penArray.GetAt(iItem));
	pPen->GetLogPen( &dlg.m_lp );

	if( dlg.DoModal() == IDOK ) {
		pPen->DeleteObject();
		pPen->CreatePenIndirect(&dlg.m_lp);
	}

	*pResult = 0;
}


void CDlgWell::OnNMDblclkListLithology(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMITEMACTIVATE lpi = (LPNMITEMACTIVATE) pNMHDR;
	
	int iItem = lpi->iItem;
	if(iItem == -1) return;

	CDlgPen dlg;
	PWELLIT pWl;
	pWl = &m_pWell->m_welLit.GetAt(iItem);
	dlg.m_lp.lopnColor = RGB(pWl->r, pWl->g, pWl->b);
	dlg.m_lp.lopnWidth.x = 0;
	dlg.m_lp.lopnStyle = PS_SOLID;

	if( dlg.DoModal() == IDOK ) {
		COLORREF color;
		color = dlg.m_lp.lopnColor;
		pWl->r = GetRValue(color);
		pWl->g = GetGValue(color);
		pWl->b = GetBValue(color);
		pWl->rd = GetRValue(color)/255.0;
		pWl->gd = GetGValue(color)/255.0;
		pWl->bd = GetBValue(color)/255.0;
	}

	*pResult = 0;
}
