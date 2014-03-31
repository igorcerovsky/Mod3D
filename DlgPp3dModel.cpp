// DlgPp3dModel.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgPp3dModel.h"


// CDlgPp3dModel dialog

IMPLEMENT_DYNAMIC(CDlgPp3dModel, CPropertyPage)
CDlgPp3dModel::CDlgPp3dModel()
	: CPropertyPage(CDlgPp3dModel::IDD)
	, m_bModelTransparent(FALSE)
	, m_bModelSolid(FALSE)
	, m_bModelVertFct(FALSE)
	, m_bModelOutFct(FALSE)
	, m_nRowStart(0)
	, m_nRowEnd(0)
	, m_nColStart(0)
	, m_nColEnd(0)
	, m_bCullFace(FALSE)
	, m_nCullFacet(0)
	, m_fDarkenInside(0)
	, m_bDarkenInside(FALSE)
	, m_bBottom(FALSE)
{
}

CDlgPp3dModel::~CDlgPp3dModel()
{
}

void CDlgPp3dModel::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Check(pDX, IDC_CHECK_TRANSPARENT_MODEL, m_bModelTransparent);
	DDX_Check(pDX, IDC_CHECK_SOLID_MODEL, m_bModelSolid);
	DDX_Check(pDX, IDC_CHECK_VRT_FACETS, m_bModelVertFct);
	DDX_Check(pDX, IDC_CHECK_OUTFCT, m_bModelOutFct);
	DDX_Text(pDX, IDC_ROW_START, m_nRowStart);
	DDX_Text(pDX, IDC_ROW_END, m_nRowEnd);
	DDX_Text(pDX, IDC_COL_START, m_nColStart);
	DDX_Text(pDX, IDC_COL_END, m_nColEnd);
	DDX_Control(pDX, IDC_SPIN_ROW_START, m_spinRowStart);
	DDX_Control(pDX, IDC_SPIN_ROW_END, m_spinRowEnd);
	DDX_Control(pDX, IDC_SPIN_COL_START, m_spinColStart);
	DDX_Control(pDX, IDC_SPIN_COL_END, m_spinColEnd);
	DDX_Check(pDX, IDC_CHECK_CULL_FACET, m_bCullFace);
	DDX_Radio(pDX, IDC_RADIO_CULL_FACE, m_nCullFacet);
	DDX_Text(pDX, IDC_EDIT_DARKEN_INSIDE, m_fDarkenInside);
	DDX_Check(pDX, IDC_CHECK_DARKEN_INSIDE, m_bDarkenInside);
	DDX_Check(pDX, IDC_3D_DRAW_BOTTOM, m_bBottom);
}


BEGIN_MESSAGE_MAP(CDlgPp3dModel, CPropertyPage)
	ON_BN_CLICKED(IDC_BUT_ALL_ROWS, OnBnClickedButAllRows)
	ON_BN_CLICKED(IDC_BUT_ALL_COLS, OnBnClickedButAllCols)
	ON_BN_CLICKED(IDC_CHECK_TRANSPARENT_MODEL, OnBnClickedCheckTransparentModel)
	ON_BN_CLICKED(IDC_CHECK_SOLID_MODEL, OnBnClickedCheckSolidModel)
	ON_BN_CLICKED(IDC_CHECK_VRT_FACETS, OnBnClickedCheckVrtFacets)
	ON_BN_CLICKED(IDC_CHECK_OUTFCT, OnBnClickedCheckOutfct)
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_ROW_START, OnDeltaposSpinRowStart)
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_ROW_END, OnDeltaposSpinRowEnd)
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_COL_START, OnDeltaposSpinColStart)
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN_COL_END, OnDeltaposSpinColEnd)
	ON_BN_CLICKED(IDC_CHECK_CULL_FACET, OnBnClickedCheckCullFacet)
	ON_BN_CLICKED(IDC_RADIO_CULL_FACE, OnBnClickedRadioCullFace)
	ON_BN_CLICKED(IDC_RADIO2, OnBnClickedRadio2)
	ON_BN_CLICKED(IDC_3D_DRAW_BOTTOM, OnBnClicked3dDrawBottom)
END_MESSAGE_MAP()


// CDlgPp3dModel message handlers
BOOL CDlgPp3dModel::OnInitDialog()
{
	CPropertyPage::OnInitDialog();

	m_spinColStart.SetRange(0, m_nColMax);
	m_spinColStart.SetPos(m_nColStart);

	m_spinColEnd.SetRange(0, m_nColMax);
	m_spinColEnd.SetPos(m_nColEnd);

	m_spinRowStart.SetRange(0, m_nRowMax);
	m_spinRowStart.SetPos(m_nRowStart);

	m_spinRowEnd.SetRange(0, m_nRowMax);
	m_spinRowEnd.SetPos(m_nRowEnd);

	UpdateData(FALSE);

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}


void CDlgPp3dModel::OnBnClickedButAllRows()
{
	UpdateData(TRUE);
	m_nRowStart = 0;
	m_nRowEnd = m_nRowMax;
	UpdateData(FALSE);
	SetModified();
}

void CDlgPp3dModel::OnBnClickedButAllCols()
{
	UpdateData(TRUE);
	m_nColStart = 0;
	m_nColEnd = m_nColMax;
	UpdateData(FALSE);
	SetModified();
}

BOOL CDlgPp3dModel::OnApply()
{
	return CPropertyPage::OnApply();
}




void CDlgPp3dModel::OnBnClickedCheckTransparentModel()		{ SetModified();	}
void CDlgPp3dModel::OnBnClickedCheckSolidModel()			{ SetModified();	}
void CDlgPp3dModel::OnBnClickedCheckVrtFacets()				{ SetModified();	}
void CDlgPp3dModel::OnBnClickedCheckOutfct()				{ SetModified();	}
void CDlgPp3dModel::OnBnClickedCheckCullFacet()				{ SetModified();	}
void CDlgPp3dModel::OnBnClickedRadioCullFace()				{ SetModified();	}
void CDlgPp3dModel::OnBnClickedRadio2()						{ SetModified();	}
void CDlgPp3dModel::OnBnClicked3dDrawBottom()				{ SetModified();	}

void CDlgPp3dModel::OnDeltaposSpinRowStart(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
	SetModified();
	*pResult = 0;
}

void CDlgPp3dModel::OnDeltaposSpinRowEnd(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
	SetModified();
	*pResult = 0;
}

void CDlgPp3dModel::OnDeltaposSpinColStart(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
	SetModified();
	*pResult = 0;
}

void CDlgPp3dModel::OnDeltaposSpinColEnd(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
	SetModified();
	*pResult = 0;
}


