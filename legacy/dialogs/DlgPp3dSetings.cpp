// DlgPp3dSetings.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgPp3dSetings.h"


// CDlgPp3dSetings dialog

IMPLEMENT_DYNAMIC(CDlgPp3dSetings, CPropertyPage)
CDlgPp3dSetings::CDlgPp3dSetings()
	: CPropertyPage(CDlgPp3dSetings::IDD)
	, m_nMode(0)
	, m_mX(0)
	, m_mY(0)
	, m_mZ(0)
	, m_rX(0)
	, m_rY(0)
	, m_rZ(0)
	, m_dProjAngle(0)
	, m_fSelSensitivity(0)
	, m_dScX(1.0)
	, m_dScY(1.0)
	, m_dScZ(1.0)
{
}

CDlgPp3dSetings::~CDlgPp3dSetings()
{
}

void CDlgPp3dSetings::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Radio(pDX, IDC_RADIO_MODE, m_nMode);
	DDX_Text(pDX, IDC_EDIT_MOTION_X, m_mX);
	DDX_Text(pDX, IDC_EDIT_MOTION_Y, m_mY);
	DDX_Text(pDX, IDC_EDIT_MOTION_Z, m_mZ);
	DDX_Text(pDX, IDC_EDIT_ROTATION_X, m_rX);
	DDX_Text(pDX, IDC_EDIT_ROTATION_Y, m_rY);
	DDX_Text(pDX, IDC_EDIT_ROTATION_Z, m_rZ);
	DDX_Text(pDX, IDC_EDIT_PROJ_ANGLE, m_dProjAngle);
	DDV_MinMaxDouble(pDX, m_dProjAngle, 0.1, 89.9);
	DDX_Text(pDX, IDC_EDIT_SEL_SENSITIVITY, m_fSelSensitivity);
	DDX_Text(pDX, IDC_SC3D_X, m_dScX);
	DDX_Text(pDX, IDC__SC3D_Y, m_dScY);
	DDX_Text(pDX, IDC__SC3D_Z, m_dScZ);
	DDV_MinMaxDouble(pDX, m_dScX, 0.001, 1000);
	DDV_MinMaxDouble(pDX, m_dScY, 0.001, 1000);
	DDV_MinMaxDouble(pDX, m_dScZ, 0.001, 1000);
}


BEGIN_MESSAGE_MAP(CDlgPp3dSetings, CPropertyPage)
	ON_BN_CLICKED(IDC_RADIO_MODE, OnBnClickedRadioMode)
	ON_BN_CLICKED(IDC_RADIO7, OnBnClickedRadio7)
	ON_EN_CHANGE(IDC_EDIT_PROJ_ANGLE, OnEnChangeEditProjAngle)
	ON_EN_CHANGE(IDC_EDIT_MOTION_Y, OnEnChangeEditMotionY)
	ON_EN_CHANGE(IDC_EDIT_MOTION_Z, OnEnChangeEditMotionZ)
	ON_EN_CHANGE(IDC_EDIT_ROTATION_X, OnEnChangeEditRotationX)
	ON_EN_CHANGE(IDC_EDIT_ROTATION_Y, OnEnChangeEditRotationY)
	ON_EN_CHANGE(IDC_EDIT_ROTATION_Z, OnEnChangeEditRotationZ)
	ON_BN_CLICKED(IDC_BUT_SN, OnBnClickedButSn)
	ON_BN_CLICKED(IDC_BUT_NS, OnBnClickedButNs)
	ON_BN_CLICKED(IDC_BUT_EW, OnBnClickedButEw)
	ON_BN_CLICKED(IDC_BUT_WE, OnBnClickedButWe)
	ON_BN_CLICKED(IDC_BUT_MAP, OnBnClickedButMap)
	ON_EN_CHANGE(IDC_SC3D_X, OnEnChangeSc3dX)
	ON_EN_CHANGE(IDC__SC3D_Y, OnEnChangeSc3dY)
	ON_EN_CHANGE(IDC__SC3D_Z, OnEnChangeSc3dZ)
	ON_EN_CHANGE(IDC_EDIT_SEL_SENSITIVITY, OnEnChangeEditSelSensitivity)
END_MESSAGE_MAP()


// CDlgPp3dSetings message handlers

BOOL CDlgPp3dSetings::OnApply()
{
	CFrameWnd* pFrameWnd = STATIC_DOWNCAST(CFrameWnd, AfxGetMainWnd());
	CView* pView = pFrameWnd->GetActiveFrame()->GetActiveView();
	pView->SendMessage(WM_VIEW_3D_PROPERTIES, 0, 0);
	SetModified(FALSE);

	return CPropertyPage::OnApply();
}



void CDlgPp3dSetings::OnBnClickedRadioMode()	{	SetModified(); }
void CDlgPp3dSetings::OnBnClickedRadio7()		{	SetModified(); }

void CDlgPp3dSetings::OnEnChangeEditProjAngle()		{	SetModified(); }
void CDlgPp3dSetings::OnEnChangeEditMotionY()		{	SetModified(); }
void CDlgPp3dSetings::OnEnChangeEditMotionZ()		{	SetModified(); }
void CDlgPp3dSetings::OnEnChangeEditRotationX()		{	SetModified(); }
void CDlgPp3dSetings::OnEnChangeEditRotationY()		{	SetModified(); }
void CDlgPp3dSetings::OnEnChangeEditRotationZ()		{	SetModified(); }
void CDlgPp3dSetings::OnEnChangeSc3dX()				{	SetModified(); }
void CDlgPp3dSetings::OnEnChangeSc3dY()				{	SetModified(); }
void CDlgPp3dSetings::OnEnChangeSc3dZ()				{	SetModified(); }
void CDlgPp3dSetings::OnEnChangeEditSelSensitivity(){	SetModified(); }


void CDlgPp3dSetings::OnBnClickedButSn()
{
    m_fRotX = 0;
	m_fRotY = 180.0f;
	m_fRotZ = 0;
	SetModified();
}

void CDlgPp3dSetings::OnBnClickedButNs()
{
    m_fRotX = 0;
	m_fRotY = 0.0f;
	m_fRotZ = 0;
	SetModified();
}

void CDlgPp3dSetings::OnBnClickedButEw()
{
    m_fRotX = 0.0f;
	m_fRotY = 90.0f;
	m_fRotZ = 0.0f;
	SetModified();
}

void CDlgPp3dSetings::OnBnClickedButWe()
{
    m_fRotX = 0.0f;
	m_fRotY = 270.0f;
	m_fRotZ = 0.0f;
	SetModified();
}

void CDlgPp3dSetings::OnBnClickedButMap()
{
    m_fRotX = 90.0f;
	m_fRotY = 180.0f;
	m_fRotZ = 0.0f;
	SetModified();
}


