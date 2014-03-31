// DlgViewMapProperties.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgViewMapProperties.h"


// CDlgViewMapProperties dialog

IMPLEMENT_DYNAMIC(CDlgViewMapProperties, CDialog)
CDlgViewMapProperties::CDlgViewMapProperties(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgViewMapProperties::IDD, pParent)
	, m_bObsPoints(FALSE)
	, m_bShowObjects(FALSE)
	, m_bShowProfiles(FALSE)
	, m_bBodies(FALSE)
	, m_bScaleEq(FALSE)
	, m_bObsImg(FALSE)
	, m_bObsCnt(FALSE)
	, m_bGrdHrz(FALSE)
	, m_bGrdVrt(FALSE)
	, m_nImgQual(10)
	, m_fAlphaImg(0.8f)
	, m_fAlphaRel(0.8f)
{
}

CDlgViewMapProperties::~CDlgViewMapProperties()
{
}

void CDlgViewMapProperties::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_SLIDER_IMG_QUALITY, m_slideQual);
	DDX_Check(pDX, IDC_CHECK_OBS_POINTS, m_bObsPoints);
	DDX_Check(pDX, IDC_CHECK_OBJECTS, m_bShowObjects);
	DDX_Check(pDX, IDC_CHECK_PROFILES, m_bShowProfiles);
	DDX_Check(pDX, IDC_CHECK_BODIES, m_bBodies);
	DDX_Check(pDX, IDC_CHECK_SCALE_EQUAL, m_bScaleEq);
	DDX_Check(pDX, IDC_CHECK_OBS_CONTOURS, m_bObsImg);
	DDX_Check(pDX, IDC_CHECK_OBS_BITMAP, m_bObsCnt);
	DDX_Check(pDX, IDC_CHECK_GRID_HRZ, m_bGrdHrz);
	DDX_Check(pDX, IDC_CHECK_GRD_VRT, m_bGrdVrt);
	DDX_Text(pDX, IDC_ALPHA_FLD, m_fAlphaImg);
	DDV_MinMaxFloat(pDX, m_fAlphaImg, 0.0f, 1.0f);
	DDX_Text(pDX, IDC_ALPHA_REL, m_fAlphaRel);
	DDV_MinMaxFloat(pDX, m_fAlphaRel, 0.0f, 1.0f);
}


BEGIN_MESSAGE_MAP(CDlgViewMapProperties, CDialog)
END_MESSAGE_MAP()


// CDlgViewMapProperties message handlers

BOOL CDlgViewMapProperties::OnInitDialog()
{
	CDialog::OnInitDialog();

	m_slideQual.SetRange(0, 100);
	m_slideQual.SetPos(m_nImgQual);

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CDlgViewMapProperties::OnOK()
{
	m_nImgQual = m_slideQual.GetPos();

	CDialog::OnOK();
}

