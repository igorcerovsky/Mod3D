// DlgPp3DAxes.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgPp3DAxes.h"


// CDlgPp3DAxes dialog

IMPLEMENT_DYNAMIC(CDlgPp3DAxes, CPropertyPage)
CDlgPp3DAxes::CDlgPp3DAxes()
	: CPropertyPage(CDlgPp3DAxes::IDD)
	, m_nAxLabels(0)
	, m_bGrdLineUpX(FALSE)
	, m_bGrdLineUpY(FALSE)
	, m_bGrdLineDwnX(FALSE)
	, m_bGrdLineDwnY(FALSE)
	, m_bGrdLineVrt(FALSE)
{
}

CDlgPp3DAxes::~CDlgPp3DAxes()
{
}

void CDlgPp3DAxes::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	DDX_Radio(pDX, IDC_RADIO_AXIS_LABELS, m_nAxLabels);
	DDX_Check(pDX, IDC_CHECK_GRD_UP_X, m_bGrdLineUpX);
	DDX_Check(pDX, IDC_CHECK_GRD_UP_Y, m_bGrdLineUpY);
	DDX_Check(pDX, IDC_CHECK_GRD_DWN_X, m_bGrdLineDwnX);
	DDX_Check(pDX, IDC_CHECK_GRD_DWN_Y, m_bGrdLineDwnY);
	DDX_Check(pDX, IDC_CHECK_GRD_LINE_VRT, m_bGrdLineVrt);
}


BEGIN_MESSAGE_MAP(CDlgPp3DAxes, CPropertyPage)
	ON_BN_CLICKED(IDC_CHECK_GRD_UP_X, OnBnClickedCheckGrdUpX)
	ON_BN_CLICKED(IDC_CHECK_GRD_UP_Y, OnBnClickedCheckGrdUpY)
	ON_BN_CLICKED(IDC_CHECK_GRD_DWN_X, OnBnClickedCheckGrdDwnX)
	ON_BN_CLICKED(IDC_CHECK_GRD_DWN_Y, OnBnClickedCheckGrdDwnY)
	ON_BN_CLICKED(IDC_CHECK_GRD_LINE_VRT, OnBnClickedCheckGrdLineVrt)
	ON_BN_CLICKED(IDC_RADIO_AXIS_LABELS, OnBnClickedRadioAxisLabels)
	ON_BN_CLICKED(IDC_RADIO5, OnBnClickedRadio5)
	ON_BN_CLICKED(IDC_RADIO6, OnBnClickedRadio6)
END_MESSAGE_MAP()


// CDlgPp3DAxes message handlers

BOOL CDlgPp3DAxes::OnApply()
{
	// TODO: Add your specialized code here and/or call the base class

	return CPropertyPage::OnApply();
}



void CDlgPp3DAxes::OnBnClickedCheckGrdUpX()			{	SetModified();	}
void CDlgPp3DAxes::OnBnClickedCheckGrdUpY()			{	SetModified();	}
void CDlgPp3DAxes::OnBnClickedCheckGrdDwnX()		{	SetModified();	}
void CDlgPp3DAxes::OnBnClickedCheckGrdDwnY()		{	SetModified();	}
void CDlgPp3DAxes::OnBnClickedCheckGrdLineVrt()		{	SetModified();	}
void CDlgPp3DAxes::OnBnClickedRadioAxisLabels()		{	SetModified();	}
void CDlgPp3DAxes::OnBnClickedRadio5()				{	SetModified();	}
void CDlgPp3DAxes::OnBnClickedRadio6()				{	SetModified();	}