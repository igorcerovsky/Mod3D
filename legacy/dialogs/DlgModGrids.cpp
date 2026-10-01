// DlgModGrids.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgModGrids.h"
#include "Mod3DDoc.h"

// CDlgModGrids dialog

IMPLEMENT_DYNAMIC(CDlgModGrids, CDialog)
CDlgModGrids::CDlgModGrids(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgModGrids::IDD, pParent)
	, m_bGx(FALSE)
	, m_bGy(FALSE)
	, m_bGz(TRUE)
	, m_bG(FALSE)
	, m_bMx(TRUE)
	, m_bMy(TRUE)
	, m_bMz(TRUE)
	, m_bM(TRUE)
	, m_bGxx(FALSE)
	, m_bGyy(FALSE)
	, m_bGzz(FALSE)
	, m_bGxy(FALSE)
	, m_bGxz(FALSE)
	, m_bGyz(FALSE)
{
	m_grd = NULL;
}

CDlgModGrids::~CDlgModGrids()
{
}

void CDlgModGrids::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Check(pDX, IDC_FLD_GX, m_bGx);
	DDX_Check(pDX, IDC_FLD_GY, m_bGy);
	DDX_Check(pDX, IDC_FLD_GZ, m_bGz);
	DDX_Check(pDX, IDC_FLD_G, m_bG);
	DDX_Check(pDX, IDC_FLD_MX, m_bMx);
	DDX_Check(pDX, IDC_FLD_MY, m_bMy);
	DDX_Check(pDX, IDC_FLD_MZ, m_bMz);
	DDX_Check(pDX, IDC_FLD_M, m_bM);

	DDX_Check(pDX, IDC_FLD_GXX, m_bGxx);
	DDX_Check(pDX, IDC_FLD_GYY, m_bGyy);
	DDX_Check(pDX, IDC_FLD_GZZ, m_bGzz);
	DDX_Check(pDX, IDC_FLD_GXY, m_bGxy);
	DDX_Check(pDX, IDC_FLD_GXZ, m_bGxz);
	DDX_Check(pDX, IDC_FLD_GYZ, m_bGyz);
}


BEGIN_MESSAGE_MAP(CDlgModGrids, CDialog)
	ON_BN_CLICKED(IDC_BUT_CREATE, OnBnClickedButCreate)
	ON_BN_CLICKED(IDC_BUT_CLOSE, OnBnClickedButClose)
	ON_BN_CLICKED(IDOK, OnBnClickedOk)
END_MESSAGE_MAP()


// CDlgModGrids message handlers

void CDlgModGrids::OnBnClickedButCreate()
{
	CreateAllGrids();
}

void CDlgModGrids::OnBnClickedButClose()
{
	CloseGrids();
}

void CDlgModGrids::OnBnClickedOk()
{
	UpdateData();

	if( !(m_bGx || m_bGy || m_bGz || m_bG || m_bMx || m_bMy || m_bMz || 
		m_bGxx || m_bGyy || m_bGzz || m_bGxy || m_bGxz || m_bGyz) ) {
			AfxMessageBox("At least one component MUST be selected!", MB_OK | MB_ICONEXCLAMATION);
			return;
		}

	CreateAllGrids();

	OnOK();
}


int CDlgModGrids::CreateGrid(int nGrd)
{
	if( m_grd[nGrd].IsEmpty() ) {
		m_grd[nGrd].New(m_nRows, m_nCols, m_x0, m_y0, m_xSize, m_ySize);
		m_grd[nGrd].DummyData();
	}

	return 0;
}

int CDlgModGrids::CreateAllGrids(void)
{
	UpdateData();

	if( m_bGx ) CreateGrid(GRDMOD_GX);
	if( m_bGy ) CreateGrid(GRDMOD_GY);
	if( m_bGz ) CreateGrid(GRDMOD_GZ);
	if( m_bG )	CreateGrid(GRDMOD_G);

	if( m_bMx ) CreateGrid(GRDMOD_MX);
	if( m_bMy ) CreateGrid(GRDMOD_MY);
	if( m_bMz ) CreateGrid(GRDMOD_MZ);
	if( m_bM )	CreateGrid(GRDMOD_M);

	if( m_bGxx ) CreateGrid(GRDMOD_GXX);
	if( m_bGyy ) CreateGrid(GRDMOD_GYY);
	if( m_bGzz ) CreateGrid(GRDMOD_GZZ);
	if( m_bGxy ) CreateGrid(GRDMOD_GXY);
	if( m_bGxz ) CreateGrid(GRDMOD_GXZ);
	if( m_bGyz ) CreateGrid(GRDMOD_GYZ);

	return 1;
}

int CDlgModGrids::CloseGrids(void)
{
	if( AfxMessageBox("Are you want sure to delete unselected components? All data will be lost.", MB_YESNO | MB_ICONINFORMATION) !=IDYES )  return 0;

	UpdateData();

	if( !m_bGx )	m_grd[GRDMOD_GX].Close();
	if( !m_bGy )	m_grd[GRDMOD_GY].Close();
	if( !m_bGz )	m_grd[GRDMOD_GZ].Close();
	if( !m_bG )		m_grd[GRDMOD_G].Close();

	if( !m_bMx )	m_grd[GRDMOD_MX].Close();
	if( !m_bMy )	m_grd[GRDMOD_MY].Close();
	if( !m_bMz )	m_grd[GRDMOD_MZ].Close();
	if( !m_bM )		m_grd[GRDMOD_M].Close();

	if( !m_bGxx )	m_grd[GRDMOD_GXX].Close();
	if( !m_bGyy )	m_grd[GRDMOD_GYY].Close();
	if( !m_bGzz )	m_grd[GRDMOD_GZZ].Close();
	if( !m_bGxy )	m_grd[GRDMOD_GXY].Close();
	if( !m_bGxz )	m_grd[GRDMOD_GXZ].Close();
	if( !m_bGyz )	m_grd[GRDMOD_GYZ].Close();

	return 1;
}

void CDlgModGrids::OnCancel()
{
	AfxMessageBox("You MUST use Close button.", MB_OK | MB_ICONINFORMATION);

	return;

	CDialog::OnCancel();
}
