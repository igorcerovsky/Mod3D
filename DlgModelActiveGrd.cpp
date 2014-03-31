// DlgModelActiveGrd.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgModelActiveGrd.h"


// CDlgModelActiveGrd dialog

IMPLEMENT_DYNAMIC(CDlgModelActiveGrd, CDialog)
CDlgModelActiveGrd::CDlgModelActiveGrd(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgModelActiveGrd::IDD, pParent)
	, m_bGx(TRUE)	, m_bGy(TRUE)	, m_bGz(TRUE)	, m_bG(TRUE)	
	, m_bGxx(FALSE)	, m_bGyy(FALSE)	, m_bGzz(FALSE)	, m_bGxy(FALSE)	, m_bGxz(FALSE)	, m_bGyz(FALSE)
	, m_bMx(TRUE)	, m_bMy(TRUE)	, m_bMz(TRUE)	, m_bM(TRUE)	
	, m_bMxx(FALSE)	, m_bMyy(FALSE)	, m_bMzz(FALSE)	, m_bMxy(FALSE)	, m_bMxz(FALSE)	, m_bMyz(FALSE)

	, m_bdGx(FALSE)	, m_bdGy(FALSE)	, m_bdGz(FALSE)	, m_bdG(FALSE)	
	, m_bdGxx(FALSE), m_bdGyy(FALSE), m_bdGzz(FALSE), m_bdGxy(FALSE), m_bdGxz(FALSE), m_bdGyz(FALSE)
	, m_bdMx(FALSE)	, m_bdMy(FALSE)	, m_bdMz(FALSE)	, m_bdM(FALSE)	
	, m_bdMxx(FALSE), m_bdMyy(FALSE), m_bdMzz(FALSE), m_bdMxy(FALSE), m_bdMxz(FALSE), m_bdMyz(FALSE)

	, m_bmGx(FALSE)	, m_bmGy(FALSE)	, m_bmGz(FALSE)	, m_bmG(FALSE)	
	, m_bmGxx(FALSE), m_bmGyy(FALSE), m_bmGzz(FALSE), m_bmGxy(FALSE), m_bmGxz(FALSE), m_bmGyz(FALSE)
	, m_bmMx(FALSE)	, m_bmMy(FALSE)	, m_bmMz(FALSE)	, m_bmM(FALSE)	
	, m_bmMxx(FALSE), m_bmMyy(FALSE), m_bmMzz(FALSE), m_bmMxy(FALSE), m_bmMxz(FALSE), m_bmMyz(FALSE)
{
}

CDlgModelActiveGrd::~CDlgModelActiveGrd()
{
}

void CDlgModelActiveGrd::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	// modeled field
	DDX_Check(pDX, IDC_FLD_GX, m_bGx);
	DDX_Check(pDX, IDC_FLD_GY, m_bGy);
	DDX_Check(pDX, IDC_FLD_GZ, m_bGz);
	DDX_Check(pDX, IDC_FLD_G, m_bG);
	DDX_Check(pDX, IDC_FLD_GXX, m_bGxx);
	DDX_Check(pDX, IDC_FLD_GXY, m_bGxy);
	DDX_Check(pDX, IDC_FLD_GXZ, m_bGxz);
	DDX_Check(pDX, IDC_FLD_GYY, m_bGyy);
	DDX_Check(pDX, IDC_FLD_GYZ, m_bGyz);
	DDX_Check(pDX, IDC_FLD_GZZ, m_bGzz);

	DDX_Check(pDX, IDC_FLD_MX, m_bMx);
	DDX_Check(pDX, IDC_FLD_MY, m_bMy);
	DDX_Check(pDX, IDC_FLD_MZ, m_bMz);
	DDX_Check(pDX, IDC_FLD_M, m_bM);
	DDX_Check(pDX, IDC_FLD_MXX, m_bMxx);
	DDX_Check(pDX, IDC_FLD_MXY, m_bMxy);
	DDX_Check(pDX, IDC_FLD_MXZ, m_bMxz);
	DDX_Check(pDX, IDC_FLD_MYY, m_bMyy);
	DDX_Check(pDX, IDC_FLD_MYZ, m_bMyz);
	DDX_Check(pDX, IDC_FLD_MZZ, m_bMzz);

	// measured field
	DDX_Check(pDX, IDC_FLD_MGX, m_bmGx);
	DDX_Check(pDX, IDC_FLD_MGY, m_bmGy);
	DDX_Check(pDX, IDC_FLD_MGZ, m_bmGz);
	DDX_Check(pDX, IDC_FLD_MG, m_bmG);
	DDX_Check(pDX, IDC_FLD_MGXX, m_bmGxx);
	DDX_Check(pDX, IDC_FLD_MGXY, m_bmGxy);
	DDX_Check(pDX, IDC_FLD_MGXZ, m_bmGxz);
	DDX_Check(pDX, IDC_FLD_MGYY, m_bmGyy);
	DDX_Check(pDX, IDC_FLD_MGYZ, m_bmGyz);
	DDX_Check(pDX, IDC_FLD_MGZZ, m_bmGzz);

	DDX_Check(pDX, IDC_FLD_MMX, m_bmMx);
	DDX_Check(pDX, IDC_FLD_MMY, m_bmMy);
	DDX_Check(pDX, IDC_FLD_MMZ, m_bmMz);
	DDX_Check(pDX, IDC_FLD_MM, m_bmM);
	DDX_Check(pDX, IDC_FLD_MMXX, m_bmMxx);
	DDX_Check(pDX, IDC_FLD_MMXY, m_bmMxy);
	DDX_Check(pDX, IDC_FLD_MMXZ, m_bmMxz);
	DDX_Check(pDX, IDC_FLD_MMYY, m_bmMyy);
	DDX_Check(pDX, IDC_FLD_MMYZ, m_bmMyz);
	DDX_Check(pDX, IDC_FLD_MMZZ, m_bmMzz);

	// difference field
	DDX_Check(pDX, IDC_FLD_DGX, m_bdGx);
	DDX_Check(pDX, IDC_FLD_DGY, m_bdGy);
	DDX_Check(pDX, IDC_FLD_DGZ, m_bdGz);
	DDX_Check(pDX, IDC_FLD_DG, m_bdG);
	DDX_Check(pDX, IDC_FLD_DGXX, m_bdGxx);
	DDX_Check(pDX, IDC_FLD_DGXY, m_bdGxy);
	DDX_Check(pDX, IDC_FLD_DGXZ, m_bdGxz);
	DDX_Check(pDX, IDC_FLD_DGYY, m_bdGyy);
	DDX_Check(pDX, IDC_FLD_DGYZ, m_bdGyz);
	DDX_Check(pDX, IDC_FLD_DGZZ, m_bdGzz);

	DDX_Check(pDX, IDC_FLD_DMX, m_bdMx);
	DDX_Check(pDX, IDC_FLD_DMY, m_bdMy);
	DDX_Check(pDX, IDC_FLD_DMZ, m_bdMz);
	DDX_Check(pDX, IDC_FLD_DM, m_bdM);
	DDX_Check(pDX, IDC_FLD_DMXX, m_bdMxx);
	DDX_Check(pDX, IDC_FLD_DMXY, m_bdMxy);
	DDX_Check(pDX, IDC_FLD_DMXZ, m_bdMxz);
	DDX_Check(pDX, IDC_FLD_DMYY, m_bdMyy);
	DDX_Check(pDX, IDC_FLD_DMYZ, m_bdMyz);
	DDX_Check(pDX, IDC_FLD_DMZZ, m_bdMzz);

}


BEGIN_MESSAGE_MAP(CDlgModelActiveGrd, CDialog)
	ON_BN_CLICKED(IDC_FLD_G, OnBnClickedFldG)
	ON_BN_CLICKED(IDC_FLD_M, OnBnClickedFldM)
END_MESSAGE_MAP()


// CDlgModelActiveGrd message handlers

void CDlgModelActiveGrd::OnBnClickedFldG()
{
	CButton* pBtn;

	pBtn = (CButton*) GetDlgItem( IDC_FLD_G );
	if( pBtn->GetCheck() == BST_CHECKED ) {
		pBtn = (CButton*) GetDlgItem( IDC_FLD_GX );
		pBtn->SetCheck( BST_CHECKED );
		pBtn = (CButton*) GetDlgItem( IDC_FLD_GY );
		pBtn->SetCheck( BST_CHECKED );
		pBtn = (CButton*) GetDlgItem( IDC_FLD_GZ );
		pBtn->SetCheck( BST_CHECKED );
	}
}

void CDlgModelActiveGrd::OnBnClickedFldM()
{
	CButton* pBtn;

	pBtn = (CButton*) GetDlgItem( IDC_FLD_M );
	if( pBtn->GetCheck() == BST_CHECKED ) {
		pBtn = (CButton*) GetDlgItem( IDC_FLD_MX );
		pBtn->SetCheck( BST_CHECKED );
		pBtn = (CButton*) GetDlgItem( IDC_FLD_MY );
		pBtn->SetCheck( BST_CHECKED );
		pBtn = (CButton*) GetDlgItem( IDC_FLD_MZ );
		pBtn->SetCheck( BST_CHECKED );
	}
}

void CDlgModelActiveGrd::OnOK()
{
	CDialog::OnOK();
}

BOOL CDlgModelActiveGrd::OnInitDialog()
{

	CButton* pBtn;

	if( m_bG == -1)			{pBtn = (CButton*) GetDlgItem( IDC_FLD_G );		pBtn->EnableWindow( FALSE ); }
	if( m_bGx == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_GX );	pBtn->EnableWindow( FALSE ); }
	if( m_bGy == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_GY );	pBtn->EnableWindow( FALSE ); }
	if( m_bGz == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_GZ );	pBtn->EnableWindow( FALSE ); }
	if( m_bGxx == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_GXX );	pBtn->EnableWindow( FALSE ); }
	if( m_bGyy == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_GYY );	pBtn->EnableWindow( FALSE ); }
	if( m_bGzz == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_GZZ );	pBtn->EnableWindow( FALSE ); }
	if( m_bGxy == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_GXY );	pBtn->EnableWindow( FALSE ); }
	if( m_bGxz == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_GXZ );	pBtn->EnableWindow( FALSE ); }
	if( m_bGyz == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_GYZ );	pBtn->EnableWindow( FALSE ); }
	if( m_bM == -1)			{pBtn = (CButton*) GetDlgItem( IDC_FLD_M );		pBtn->EnableWindow( FALSE ); }
	if( m_bMx == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_MX );	pBtn->EnableWindow( FALSE ); }
	if( m_bMy == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_MY );	pBtn->EnableWindow( FALSE ); }
	if( m_bMz == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_MZ );	pBtn->EnableWindow( FALSE ); }
	if( m_bMxx == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_MXX );	pBtn->EnableWindow( FALSE ); }
	if( m_bMyy == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_MYY );	pBtn->EnableWindow( FALSE ); }
	if( m_bMzz == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_MZZ );	pBtn->EnableWindow( FALSE ); }
	if( m_bMxy == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_MXY );	pBtn->EnableWindow( FALSE ); }
	if( m_bMxz == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_MXZ );	pBtn->EnableWindow( FALSE ); }
	if( m_bMyz == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_MYZ );	pBtn->EnableWindow( FALSE ); }

	if( m_bdG == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_DG );	pBtn->EnableWindow( FALSE ); }
	if( m_bdGx == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_DGX );	pBtn->EnableWindow( FALSE ); }
	if( m_bdGy == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_DGY );	pBtn->EnableWindow( FALSE ); }
	if( m_bdGz == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_DGZ );	pBtn->EnableWindow( FALSE ); }
	if( m_bdGxx == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_DGXX );	pBtn->EnableWindow( FALSE ); }
	if( m_bdGyy == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_DGYY );	pBtn->EnableWindow( FALSE ); }
	if( m_bdGzz == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_DGZZ );	pBtn->EnableWindow( FALSE ); }
	if( m_bdGxy == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_DGXY );	pBtn->EnableWindow( FALSE ); }
	if( m_bdGxz == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_DGXZ );	pBtn->EnableWindow( FALSE ); }
	if( m_bdGyz == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_DGYZ );	pBtn->EnableWindow( FALSE ); }
	if( m_bdM == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_DM );	pBtn->EnableWindow( FALSE ); }
	if( m_bdMx == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_DMX );	pBtn->EnableWindow( FALSE ); }
	if( m_bdMy == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_DMY );	pBtn->EnableWindow( FALSE ); }
	if( m_bdMz == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_DMZ );	pBtn->EnableWindow( FALSE ); }
	if( m_bdMxx == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_DMXX );	pBtn->EnableWindow( FALSE ); }
	if( m_bdMyy == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_DMYY );	pBtn->EnableWindow( FALSE ); }
	if( m_bdMzz == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_DMZZ );	pBtn->EnableWindow( FALSE ); }
	if( m_bdMxy == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_DMXY );	pBtn->EnableWindow( FALSE ); }
	if( m_bdMxz == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_DMXZ );	pBtn->EnableWindow( FALSE ); }
	if( m_bdMyz == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_DMYZ );	pBtn->EnableWindow( FALSE ); }

	if( m_bmG == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_MG );	pBtn->EnableWindow( FALSE ); }
	if( m_bmGx == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_MGX );	pBtn->EnableWindow( FALSE ); }
	if( m_bmGy == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_MGY );	pBtn->EnableWindow( FALSE ); }
	if( m_bmGz == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_MGZ );	pBtn->EnableWindow( FALSE ); }
	if( m_bmGxx == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_MGXX );	pBtn->EnableWindow( FALSE ); }
	if( m_bmGyy == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_MGYY );	pBtn->EnableWindow( FALSE ); }
	if( m_bmGzz == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_MGZZ );	pBtn->EnableWindow( FALSE ); }
	if( m_bmGxy == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_MGXY );	pBtn->EnableWindow( FALSE ); }
	if( m_bmGxz == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_MGXZ );	pBtn->EnableWindow( FALSE ); }
	if( m_bmGyz == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_MGYZ );	pBtn->EnableWindow( FALSE ); }
	if( m_bmM == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_MM );	pBtn->EnableWindow( FALSE ); }
	if( m_bmMx == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_MMX );	pBtn->EnableWindow( FALSE ); }
	if( m_bmMy == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_MMY );	pBtn->EnableWindow( FALSE ); }
	if( m_bmMz == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_MMZ );	pBtn->EnableWindow( FALSE ); }
	if( m_bmMxx == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_MMXX );	pBtn->EnableWindow( FALSE ); }
	if( m_bmMyy == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_MMYY );	pBtn->EnableWindow( FALSE ); }
	if( m_bmMzz == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_MMZZ );	pBtn->EnableWindow( FALSE ); }
	if( m_bmMxy == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_MMXY );	pBtn->EnableWindow( FALSE ); }
	if( m_bmMxz == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_MMXZ );	pBtn->EnableWindow( FALSE ); }
	if( m_bmMyz == -1)		{pBtn = (CButton*) GetDlgItem( IDC_FLD_MMYZ );	pBtn->EnableWindow( FALSE ); }

	CDialog::OnInitDialog();

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}