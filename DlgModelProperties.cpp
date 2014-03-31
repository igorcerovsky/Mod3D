// DlgModelProperties.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgModelProperties.h"


// CDlgModelProperties dialog

IMPLEMENT_DYNAMIC(CDlgModelProperties, CDialog)
CDlgModelProperties::CDlgModelProperties(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgModelProperties::IDD, pParent)
	, m_fMinX(0)
	, m_fMinY(0)
	, m_fMaxX(0)
	, m_fMaxY(0)
	, m_fMinZ(0)
	, m_fMaxZ(0)
	, m_strDescription(_T(""))
	, m_bExtendBoundaries(FALSE)
	, m_fExW(0)
	, m_fExE(0)
	, m_fExN(0)
	, m_fExS(0)
	, m_xSize(0)
	, m_ySize(0)
{
}

CDlgModelProperties::~CDlgModelProperties()
{
}

void CDlgModelProperties::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_MIN_X, m_fMinX);
	DDX_Text(pDX, IDC_EDIT_MAX_X, m_fMaxX);
	DDX_Text(pDX, IDC_EDIT_MIN_Y, m_fMinY);
	DDX_Text(pDX, IDC_EDIT_MAX_Y, m_fMaxY);
	DDX_Text(pDX, IDC_EDIT_MIN_Z, m_fMinZ);
	DDX_Text(pDX, IDC_EDIT_MAX_Z, m_fMaxZ);
	DDX_Text(pDX, IDC_EDIT_DESCRIPTION, m_strDescription);
	DDX_Check(pDX, IDC_CHECK_EXTEND_BOUNDARIES, m_bExtendBoundaries);
	DDX_Text(pDX, IDC_EDIT_EXTEND_WEST, m_fExW);
	DDX_Text(pDX, IDC_EDIT_EXTEND_EAST, m_fExE);
	DDX_Text(pDX, IDC_EDIT_EXTEND_NORTH, m_fExN);
	DDX_Text(pDX, IDC_EDIT_EXTEND_SOUTH, m_fExS);
	DDX_Control(pDX, IDC_EDIT_DESCRIPTION, m_editInfo);
	DDX_Text(pDX, IDC_EDIT_INCR_X, m_xSize);
	DDX_Text(pDX, IDC_EDIT_INCR_Y, m_ySize);
}


BEGIN_MESSAGE_MAP(CDlgModelProperties, CDialog)
END_MESSAGE_MAP()


// CDlgModelProperties message handlers

BOOL CDlgModelProperties::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  Add extra initialization here
	m_editInfo.SetWindowText(m_strInfo);

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CDlgModelProperties::OnOK()
{
	m_editInfo.GetWindowText(m_strInfo);

	CDialog::OnOK();
}
