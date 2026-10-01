// DlgData3D.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgData3D.h"
#include "DlgPen.h"


// CDlgData3D dialog

IMPLEMENT_DYNAMIC(CDlgData3D, CDialog)
CDlgData3D::CDlgData3D(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgData3D::IDD, pParent)
	, m_dMin(0)
	, m_dMax(1)
	, m_bCustomRange(FALSE)
	, m_strFilePath(_T(""))
	, m_bConnectPts(FALSE)
	, m_strName(_T(""))
	, m_bColorDistribution(FALSE)
	, m_dMinData(0)
	, m_dMaxData(0)
	, m_dRadius(0)
	, m_bPoints(FALSE)
	, m_nSlices(0)
	, m_nStacks(0)
{
}

CDlgData3D::~CDlgData3D()
{
}

void CDlgData3D::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_MIN, m_dMin);
	DDX_Text(pDX, IDC_MAX, m_dMax);
	DDX_Check(pDX, IDC_CHECK1, m_bCustomRange);
	DDX_Text(pDX, IDC_FILE_PATH, m_strFilePath);
	DDX_Check(pDX, IDC_CHECK3, m_bConnectPts);
	DDX_Text(pDX, IDC_NAME_D3D, m_strName);
	DDX_Check(pDX, IDC_D3D_COLORDITRIBUTIN, m_bColorDistribution);
	DDX_Text(pDX, IDC_D3D_MINDATA, m_dMinData);
	DDX_Text(pDX, IDC_D3D_MAXDATA, m_dMaxData);
	DDX_Text(pDX, IDC_D3D_SYMBOLR, m_dRadius);
	DDX_Check(pDX, IDC_D3D_PTS, m_bPoints);
	DDX_Text(pDX, IDC_D3D_SLICES, m_nSlices);
	DDX_Text(pDX, IDC_D3D_STACKS, m_nStacks);
	DDV_MinMaxInt(pDX, m_nSlices, 2, 100);
	DDV_MinMaxInt(pDX, m_nStacks, 2, 200);
}


BEGIN_MESSAGE_MAP(CDlgData3D, CDialog)
	ON_BN_CLICKED(IDC_D3D_COLORGRAD, OnBnClickedD3dColorgrad)
	ON_BN_CLICKED(IDC_COLORBUT, OnBnClickedColorbut)
	ON_BN_CLICKED(IDC_CHECK1, OnBnClickedCheck1)
END_MESSAGE_MAP()


// CDlgData3D message handlers

BOOL CDlgData3D::OnInitDialog()
{
	CDialog::OnInitDialog();

	if( m_bCustomRange ) {
		GetDlgItem(IDC_MIN)->EnableWindow(TRUE);
		GetDlgItem(IDC_MAX)->EnableWindow(TRUE);
	}
	else {
		GetDlgItem(IDC_MIN)->EnableWindow(FALSE);
		GetDlgItem(IDC_MAX)->EnableWindow(FALSE);
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CDlgData3D::OnBnClickedColorbut()
{
	CDlgPen dlg;

	dlg.m_lp = m_lp;
	dlg.DoModal();
	m_lp = dlg.m_lp;
}

void CDlgData3D::OnBnClickedD3dColorgrad()
{
	UpdateData();
	if(m_bCustomRange) {
		m_pGradCtrl->SetHstMin( m_dMin );
		m_pGradCtrl->SetHstMax( m_dMax );
	}
	else {
		m_pGradCtrl->SetHstMin( m_dMinData );
		m_pGradCtrl->SetHstMax( m_dMaxData );
	}

	CDlgColorGrad dlg;

	dlg.m_pGrad = m_pGradCtrl;
	dlg.m_dMin = m_dMin;
	dlg.m_dMax = m_dMax;

	dlg.DoModal();
}


void CDlgData3D::OnBnClickedCheck1()
{
	UpdateData();
	if( m_bCustomRange ) {
		GetDlgItem(IDC_MIN)->EnableWindow(TRUE);
		GetDlgItem(IDC_MAX)->EnableWindow(TRUE);
	}
	else {
		GetDlgItem(IDC_MIN)->EnableWindow(FALSE);
		GetDlgItem(IDC_MAX)->EnableWindow(FALSE);
	}
}
