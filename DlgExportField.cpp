// DlgExportField.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "Mod3DDoc.h"
#include "DlgExportField.h"
#include "Grid.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CDlgExportField dialog


CDlgExportField::CDlgExportField(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgExportField::IDD, pParent)
{
}


void CDlgExportField::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CDlgExportField, CDialog)
	ON_BN_CLICKED(IDC_BUT_G, OnBnClickedButG)
	ON_BN_CLICKED(IDC_BUT_GX, OnBnClickedButGx)
	ON_BN_CLICKED(IDC_BUT_GY, OnBnClickedButGy)
	ON_BN_CLICKED(IDC_BUT_GZ, OnBnClickedButGz)
	ON_BN_CLICKED(IDC_BUT_GXX, OnBnClickedButGxx)
	ON_BN_CLICKED(IDC_BUT_GXY, OnBnClickedButGxy)
	ON_BN_CLICKED(IDC_BUT_GXZ, OnBnClickedButGxz)
	ON_BN_CLICKED(IDC_BUT_GYY, OnBnClickedButGyy)
	ON_BN_CLICKED(IDC_BUT_GYZ, OnBnClickedButGyz)
	ON_BN_CLICKED(IDC_BUT_GZZ, OnBnClickedButGzz)
	ON_BN_CLICKED(IDC_BUT_M, OnBnClickedButM)
	ON_BN_CLICKED(IDC_BUT_MX, OnBnClickedButMx)
	ON_BN_CLICKED(IDC_BUT_MY, OnBnClickedButMy)
	ON_BN_CLICKED(IDC_BUT_MZ, OnBnClickedButMz)
	ON_BN_CLICKED(IDC_BUT_MXX, OnBnClickedButMxx)
	ON_BN_CLICKED(IDC_BUT_MXY, OnBnClickedButMxy)
	ON_BN_CLICKED(IDC_BUT_MXZ, OnBnClickedButMxz)
	ON_BN_CLICKED(IDC_BUT_MYY, OnBnClickedButMyy)
	ON_BN_CLICKED(IDC_BUT_MYZ, OnBnClickedButMyz)
	ON_BN_CLICKED(IDC_BUT_MZZ, OnBnClickedButMzz)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDlgExportField message handlers

void CDlgExportField::OnBnClickedButG()		{	m_pFld[GRDMOD_G].SaveAs();	}
void CDlgExportField::OnBnClickedButGx()	{	m_pFld[GRDMOD_GX].SaveAs();	}
void CDlgExportField::OnBnClickedButGy()	{	m_pFld[GRDMOD_GY].SaveAs();	}
void CDlgExportField::OnBnClickedButGz()	{	m_pFld[GRDMOD_GZ].SaveAs();	}
void CDlgExportField::OnBnClickedButGxx()	{	m_pFld[GRDMOD_GXX].SaveAs();	}
void CDlgExportField::OnBnClickedButGxy()	{	m_pFld[GRDMOD_GXY].SaveAs();	}
void CDlgExportField::OnBnClickedButGxz()	{	m_pFld[GRDMOD_GXZ].SaveAs();	}
void CDlgExportField::OnBnClickedButGyy()	{	m_pFld[GRDMOD_GYY].SaveAs();	}
void CDlgExportField::OnBnClickedButGyz()	{	m_pFld[GRDMOD_GYZ].SaveAs();	}
void CDlgExportField::OnBnClickedButGzz()	{	m_pFld[GRDMOD_GZZ].SaveAs();	}

void CDlgExportField::OnBnClickedButM()		{	m_pFld[GRDMOD_M].SaveAs();	}
void CDlgExportField::OnBnClickedButMx()	{	m_pFld[GRDMOD_MX].SaveAs();	}
void CDlgExportField::OnBnClickedButMy()	{	m_pFld[GRDMOD_MY].SaveAs();	}
void CDlgExportField::OnBnClickedButMz()	{	m_pFld[GRDMOD_MZ].SaveAs();	}
void CDlgExportField::OnBnClickedButMxx()	{	m_pFld[GRDMOD_MXX].SaveAs();	}
void CDlgExportField::OnBnClickedButMxy()	{	m_pFld[GRDMOD_MXY].SaveAs();	}
void CDlgExportField::OnBnClickedButMxz()	{	m_pFld[GRDMOD_MXY].SaveAs();	}
void CDlgExportField::OnBnClickedButMyy()	{	m_pFld[GRDMOD_MYY].SaveAs();	}
void CDlgExportField::OnBnClickedButMyz()	{	m_pFld[GRDMOD_MYZ].SaveAs();	}
void CDlgExportField::OnBnClickedButMzz()	{	m_pFld[GRDMOD_MZZ].SaveAs();	}
