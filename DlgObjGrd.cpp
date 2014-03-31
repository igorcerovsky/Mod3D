// DlgObjGrd.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgObjGrd.h"
#include "DlgPen.h"


// CDlgObjGrd dialog

IMPLEMENT_DYNAMIC(CDlgObjGrd, CDialog)
CDlgObjGrd::CDlgObjGrd(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgObjGrd::IDD, pParent)
	, m_nType(0)
	, m_strName(_T(""))
	, m_strID(_T(""))
	, m_strFileName(_T(""))
{
}

CDlgObjGrd::~CDlgObjGrd()
{
}

void CDlgObjGrd::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_BUTTON1, m_butBrowse);
	DDX_Radio(pDX, IDC_RADIO_TYPE, m_nType);
	DDX_Text(pDX, IDC_NAME, m_strName);
	DDX_Text(pDX, IDC_STR_ID, m_strID);
	DDX_Text(pDX, IDC_FILE_NAME, m_strFileName);
}


BEGIN_MESSAGE_MAP(CDlgObjGrd, CDialog)
	ON_BN_CLICKED(IDC_BROWSE, OnBnClickedBrowse)
	ON_BN_CLICKED(IDC_PEN, OnBnClickedPen)
	ON_BN_CLICKED(IDC_GRID, OnBnClickedGrid)
END_MESSAGE_MAP()


// CDlgObjGrd message handlers

BOOL CDlgObjGrd::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_butBrowse.SetBitmap( LoadBitmap(AfxGetApp()->m_hInstance, MAKEINTRESOURCE(IDB_BMP_OPEN) ) );

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CDlgObjGrd::OnBnClickedBrowse()
{
	BOOL b=FALSE;;
	if( m_pGrd != NULL ) {
		CGrid grd;
		b = grd.Open();
		if( b ) {
			m_pGrd->Close();
			m_pGrd->New(grd.GetRows(), grd.GetCols(), grd.GetX0(), grd.GetY0(), grd.GetXSize(), grd.GetYSize() );
			grd.CopyData(m_pGrd);
			m_pGrd->UpdateMinMaxValue();
			m_pGrd->SetModifiedFlag();
			SetDlgItemText( IDC_FILE_NAME, m_pGrd->GetFilePath() );
		}
	}
	else
		return;

}

void CDlgObjGrd::OnBnClickedPen()
{
	CDlgPen dlg;
	dlg.m_lp = m_lp;
	if( dlg.DoModal() == IDOK) {
		m_lp = dlg.m_lp;
	}
}

void CDlgObjGrd::OnBnClickedGrid()
{
	if( m_pGrd != NULL )
		m_pGrd->Properties();
}
