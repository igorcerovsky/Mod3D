// DlgPpCompGrav.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgPpCompGrav.h"


// CDlgPpCompGrav dialog

IMPLEMENT_DYNAMIC(CDlgPpCompGrav, CPropertyPage)
CDlgPpCompGrav::CDlgPpCompGrav()
	: CPropertyPage(CDlgPpCompGrav::IDD)
	, m_dGravSens(0)
	, m_nUnits(0)
	, m_dUnits(0)
	, m_dDensRef(0)
	, m_dDGx(0)
	, m_dDGy(0)
	, m_dDGz(0)
	, m_dDOx(0)
	, m_dDOy(0)
	, m_dDOz(0)
	, m_dGradUnits(0)
	, m_bMeanGrv(FALSE)
	, m_bMeanTns(FALSE)
{
}

CDlgPpCompGrav::~CDlgPpCompGrav()
{
}

void CDlgPpCompGrav::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Radio(pDX, IDC_RADIO_UNITS, m_nUnits);
	DDX_Text(pDX, IDC_UNITS, m_dUnits);
	DDX_Text(pDX, IDC_EDIT_DENSITY, m_dDensRef);
	DDX_Text(pDX, IDC_EDIT_GRD_X, m_dDGx);
	DDX_Text(pDX, IDC_EDIT_GRD_Y, m_dDGy);
	DDX_Text(pDX, IDC_EDIT_GRD_Z, m_dDGz);
	DDX_Text(pDX, IDC_EDIT_DO_X, m_dDOx);
	DDX_Text(pDX, IDC_EDIT_DO_Y, m_dDOy);
	DDX_Text(pDX, IDC_EDIT_DO_Z, m_dDOz);
	DDX_Text(pDX, IDC_EDIT_TENS_UNITS, m_dGradUnits);
	DDX_Check(pDX, IDC_GRV_REM_MEAN, m_bMeanGrv);
	DDX_Check(pDX, IDC_TNS_REM_MEAN, m_bMeanTns);
}


BEGIN_MESSAGE_MAP(CDlgPpCompGrav, CPropertyPage)
END_MESSAGE_MAP()


// CDlgPpCompGrav message handlers
BOOL CDlgPpCompGrav::OnInitDialog()
{
	CPropertyPage::OnInitDialog();

	m_nUnits = 2;
	if( m_dUnits == 1.0e6 ) 
		m_nUnits = 0;
	if( m_dUnits == 1.0e5 )
		m_nUnits = 1;
	UpdateData(FALSE);

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CDlgPpCompGrav::OnOK()
{
	if(m_nUnits == 0 ) 
		m_dUnits = 1.0e6;
	if(m_nUnits == 1 ) 
		m_dUnits = 1.0e5;

	CPropertyPage::OnOK();
}

