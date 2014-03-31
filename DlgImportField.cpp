// DlgImportField.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgImportField.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CDlgImportField dialog


CDlgImportField::CDlgImportField(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgImportField::IDD, pParent)
	, m_nType(0)
{
}


void CDlgImportField::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Radio(pDX, IDC_RADIO_EXPIMP, m_nType);
	DDX_Control(pDX, IDC_COMBO_FLD, m_combo);
}


BEGIN_MESSAGE_MAP(CDlgImportField, CDialog)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDlgImportField message handlers
BOOL CDlgImportField::CheckGrid( GridInfo grdInf )
{
	if( m_obsHdr.nCols == grdInf.nCol && m_obsHdr.nRows == grdInf.nRow &&
		m_obsHdr.x0 == grdInf.x0 && m_obsHdr.y0 == grdInf.y0 &&
		m_obsHdr.xSize == grdInf.xSize && m_obsHdr.ySize == grdInf.ySize
		) 
	{
		return TRUE;
	}
	AfxMessageBox("Gz grid does not match.");
	return FALSE;
}

BOOL CDlgImportField::ImportGrid(int nGrid)
{
	CGrid		*pGrd = &(m_fldGrd[nGrid]);
	CGrid		grd;
	GridInfo	gi;

	if(pGrd->IsEmpty()) {
		gi = grd.GetInfoBrowse();
		if( CheckGrid(gi) ) {
			pGrd->Open( gi.strFilePath, gi.nType );
			return TRUE;
		}
	}
	else {
		int ret = AfxMessageBox("Grid is not empty. Replace?", MB_ICONQUESTION | MB_YESNO);
		if( ret == IDYES) {
			gi = grd.GetInfoBrowse();
			if( CheckGrid( gi) ) {
				pGrd->Close();
				pGrd->Open( gi.strFilePath, gi.nType );
				return TRUE;
			}
		}
	}

	return FALSE;

}

BOOL CDlgImportField::NewDifferenceGrid( int nGrid ) 
{
	if( m_fldGrd[nGrid].IsEmpty() ) {
		m_fldGrd[nGrid].New(m_obsHdr.nRows, m_obsHdr.nCols, m_obsHdr.x0, m_obsHdr.y0, m_obsHdr.xSize, m_obsHdr.xSize);
		m_fldGrd[nGrid].ZeroData();
		m_fldGrd[nGrid].SetColorTable( CTBL_DIF );
		m_grdActive[nGrid] = TRUE;
	}
	return TRUE;
}

//void CDlgImportField::OnBnClickedButG()		{ if(ImportGrid(GRDMES_G)	)	{NewDifferenceGrid( GRDDIF_G );		m_grdActive[GRDMES_G] = TRUE;}}
//void CDlgImportField::OnBnClickedButGx()	{ if(ImportGrid(GRDMES_GX)	)	{NewDifferenceGrid( GRDDIF_GX );	m_grdActive[GRDMES_GX] = TRUE;}	}
//void CDlgImportField::OnBnClickedButGy()	{ if(ImportGrid(GRDMES_GY)	)	{NewDifferenceGrid( GRDDIF_GY );	m_grdActive[GRDMES_GY] = TRUE;}	}
//void CDlgImportField::OnBnClickedButGz()	{ if(ImportGrid(GRDMES_GZ)	)	{NewDifferenceGrid( GRDDIF_GZ );	m_grdActive[GRDMES_GZ] = TRUE;}	}
//void CDlgImportField::OnBnClickedButGxx()	{ if(ImportGrid(GRDMES_GXX)	)	{NewDifferenceGrid( GRDDIF_GXX );	m_grdActive[GRDMES_GXX] = TRUE;}	}
//void CDlgImportField::OnBnClickedButGxy()	{ if(ImportGrid(GRDMES_GXY)	)	{NewDifferenceGrid( GRDDIF_GXY );	m_grdActive[GRDMES_GXY] = TRUE;}	}
//void CDlgImportField::OnBnClickedButGxz()	{ if(ImportGrid(GRDMES_GXZ)	)	{NewDifferenceGrid( GRDDIF_GXZ );	m_grdActive[GRDMES_GXZ] = TRUE;}	}
//void CDlgImportField::OnBnClickedButGyy()	{ if(ImportGrid(GRDMES_GYY)	)	{NewDifferenceGrid( GRDDIF_GYY );	m_grdActive[GRDMES_GYY] = TRUE;}	}
//void CDlgImportField::OnBnClickedButGyz()	{ if(ImportGrid(GRDMES_GYZ)	)	{NewDifferenceGrid( GRDDIF_GYZ );	m_grdActive[GRDMES_GYZ] = TRUE;}	}
//void CDlgImportField::OnBnClickedButGzz()	{ if(ImportGrid(GRDMES_GZZ)	)	{NewDifferenceGrid( GRDDIF_GZZ );	m_grdActive[GRDMES_GZZ] = TRUE;}	}
//
//void CDlgImportField::OnBnClickedButM()		{ if(ImportGrid(GRDMES_M)	)	{NewDifferenceGrid( GRDDIF_M );		m_grdActive[GRDMES_M] = TRUE;}	}
//void CDlgImportField::OnBnClickedButMx()	{ if(ImportGrid(GRDMES_MX)	)	{NewDifferenceGrid( GRDDIF_MX );	m_grdActive[GRDMES_MX] = TRUE;}	}
//void CDlgImportField::OnBnClickedButMy()	{ if(ImportGrid(GRDMES_MY)	)	{NewDifferenceGrid( GRDDIF_MY );	m_grdActive[GRDMES_MY] = TRUE;}	}
//void CDlgImportField::OnBnClickedButMz()	{ if(ImportGrid(GRDMES_MZ)	)	{NewDifferenceGrid( GRDDIF_MZ );	m_grdActive[GRDMES_MZ] = TRUE;}	}
//void CDlgImportField::OnBnClickedButMxx()	{ if(ImportGrid(GRDMES_MXX)	)	{NewDifferenceGrid( GRDDIF_MXX );	m_grdActive[GRDMES_MXX] = TRUE;}	}
//void CDlgImportField::OnBnClickedButMxy()	{ if(ImportGrid(GRDMES_MXY)	)	{NewDifferenceGrid( GRDDIF_MXY );	m_grdActive[GRDMES_MXY] = TRUE;}	}
//void CDlgImportField::OnBnClickedButMxz()	{ if(ImportGrid(GRDMES_MXZ)	)	{NewDifferenceGrid( GRDDIF_MXZ );	m_grdActive[GRDMES_MXZ] = TRUE;}	}
//void CDlgImportField::OnBnClickedButMyy()	{ if(ImportGrid(GRDMES_MYY)	)	{NewDifferenceGrid( GRDDIF_MYY );	m_grdActive[GRDMES_MYY] = TRUE;}	}
//void CDlgImportField::OnBnClickedButMyz()	{ if(ImportGrid(GRDMES_MYZ)	)	{NewDifferenceGrid( GRDDIF_MYZ );	m_grdActive[GRDMES_MYZ] = TRUE;}	}
//void CDlgImportField::OnBnClickedButMzz()	{ if(ImportGrid(GRDMES_MZZ)	)	{NewDifferenceGrid( GRDDIF_MZZ );	m_grdActive[GRDMES_MZZ] = TRUE;}	}



BOOL CDlgImportField::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  Add extra initialization here

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}
