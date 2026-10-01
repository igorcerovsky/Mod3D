// DlgObservations.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgObservations.h"
#include "Routines.h"


// CDlgObservations dialog

IMPLEMENT_DYNAMIC(CDlgObservations, CDialog)
CDlgObservations::CDlgObservations(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgObservations::IDD, pParent)
	, m_dSensHeight(0.001)
	, m_dConstElev(0)
	, m_strElevGrid(_T(""))
	, m_strBlank(_T(""))
	, m_nFld(0)
	, m_bRecompute(0)
	, m_nObsTag(0)
{
}

CDlgObservations::~CDlgObservations()
{
}

void CDlgObservations::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_SENSOR_HEIGHT, m_dSensHeight);
	DDX_Text(pDX, IDC_EDIT_SENSOR_CONST, m_dConstElev);
	DDX_Control(pDX, IDC_BUT_PROCEED, m_butProceed);
	DDX_Text(pDX, IDC_EDIT_GRD_ELEV_FILE, m_strElevGrid);
	DDX_Text(pDX, IDC_EDIT_BLANK_FILE, m_strBlank);
	DDX_Control(pDX, IDC_RADIO_FIELD, m_radioObs);
	DDV_MinMaxDouble(pDX, m_dSensHeight, 1.0e-9, 1.0e100);
	DDX_Radio(pDX, IDC_RADIO_FIELD, m_nFld);
	DDX_Radio(pDX, IDC_RADIO_OBS_TYPE, m_nObsTag);
}


BEGIN_MESSAGE_MAP(CDlgObservations, CDialog)
	ON_BN_CLICKED(IDC_BUTTON_BLANK, OnBnClickedButtonBlank)
	ON_BN_CLICKED(IDC_RADIO_FIELD, OnBnClickedRadioField)
	ON_BN_CLICKED(IDC_RADIO2, OnBnClickedRadio2)
	ON_BN_CLICKED(IDC_RADIO3, OnBnClickedRadio3)
	ON_BN_CLICKED(IDC_RADIO6, OnBnClickedRadio6)
	ON_BN_CLICKED(IDC_RADIO_OBS_TYPE, OnBnClickedRadioObsType)
	ON_BN_CLICKED(IDC_RADIO5, OnBnClickedRadio5)
	ON_EN_CHANGE(IDC_EDIT_SENSOR_HEIGHT, OnEnChangeEditSensorHeight)
	ON_EN_CHANGE(IDC_EDIT_SENSOR_CONST, OnEnChangeEditSensorConst)
END_MESSAGE_MAP()


// CDlgObservations message handlers
BOOL CDlgObservations::OnInitDialog()
{
	CDialog::OnInitDialog();

	if(m_nFld==0)	OnBnClickedRadioField();
	if(m_nFld==1)	OnBnClickedRadio2();
	if(m_nFld==2)	OnBnClickedRadio3();

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CDlgObservations::OnBnClickedRadioField()
{
	UpdateData();
	m_dSensHeight = m_dSensHeightGrv;
	m_dConstElev = m_dConstElevGrv;
	m_strElevGrid = m_pGrdGrv->GetFilePath();
	m_nObsTag = m_nObsTagGrv;
	UpdateData(FALSE);
}

void CDlgObservations::OnBnClickedRadio2()
{
	UpdateData();
	m_dSensHeight = m_dSensHeightMag;
	m_dConstElev = m_dConstElevMag;
	m_strElevGrid = m_pGrdMag->GetFilePath();
	m_nObsTag = m_nObsTagMag;
	UpdateData(FALSE);
}

void CDlgObservations::OnBnClickedRadio3()
{
	UpdateData();
	m_dSensHeight = m_dSensHeightTns;
	m_dConstElev = m_dConstElevTns;
	m_strElevGrid = m_pGrdTns->GetFilePath();
	m_nObsTag = m_nObsTagTns;
	UpdateData(FALSE);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CDlgObservations::OnBnClickedRadioObsType()
{
	CGrid* pGrd;

	pGrd=GetGrid();
	ASSERT(pGrd!=NULL);
	UpdateMyData();
	m_pGrdRel->CopyData(pGrd);
	*pGrd += m_dSensHeight;
	m_bRecompute = TRUE;
	AfxMessageBox("Observations for constant sensor height over relief were successfully assigned.", MB_OK | MB_ICONINFORMATION);
}

void CDlgObservations::OnBnClickedRadio5()
{
	CGrid* pGrd;

	pGrd=GetGrid();
	ASSERT(pGrd!=NULL);
	UpdateMyData();
	m_pGrdRel->CopyData(pGrd);
	int n = pGrd->SetValueEx( m_dConstElev, 1 );
	int m = n - pGrd->GetNodesNumber();
	CString str;
	int tag;
	if( n>0 ) {
		str.Format("%d values were succesfully assigned. %d values were ingnored.", n-1, m);
		tag = MB_OK | MB_ICONINFORMATION;
	}
	else {
		str = "Wrong elevation value!";
		tag = MB_OK | MB_ICONEXCLAMATION;
	}
	AfxMessageBox(str, tag);
	m_bRecompute = TRUE;
}

void CDlgObservations::OnBnClickedRadio6()
{
	CGrid* pGrd;
	CGrid grd;

	pGrd=GetGrid();
	ASSERT(pGrd!=NULL);
	grd.Open();
	if( pGrd->IsSameSize(&grd) ){
		grd.CopyData(pGrd);
		int n=pGrd->DummyDataEx(m_pGrdRel, DMX_GRATER);
		int tag;
		CString str;
		if(n==0) {
			str = "Observation grid created.";
			tag = MB_OK | MB_ICONINFORMATION;
		}
		else {
			str.Format("Observation grid created with %d dummy values.", n);
			tag = MB_OK | MB_ICONEXCLAMATION;
		}
		AfxMessageBox(str, tag);
	}
	else
		AfxMessageBox("Grid must be of the same size!", MB_OK | MB_ICONEXCLAMATION);
	m_bRecompute = TRUE;
	
}


void CDlgObservations::OnBnClickedButtonBlank()
{
	CGrid* pGrd;

	pGrd=GetGrid();
	ASSERT(pGrd!=NULL);
	pGrd->Blank();
	m_bRecompute = TRUE;
}

CGrid*	CDlgObservations::GetGrid()
{
	UpdateData();
	int n = m_nFld;
	ASSERT(n==0 || n==1 || n==2);

	switch(n) {
		case 0:
			return m_pGrdGrv;
			break;
		case 1:
			return m_pGrdMag;
			break;
		case 2:
			return m_pGrdTns;
			break;
	}

	return NULL;
}

void	CDlgObservations::UpdateMyData(BOOL bFromControls)
{
	int n = m_nFld;
	ASSERT(n==0 || n==1 || n==2);

	if(bFromControls) {
		UpdateData(TRUE);
		switch(n) {
			case 0:
				m_bRecompute |=  SetChange(m_dSensHeightGrv, m_dSensHeight);
				m_bRecompute |=  SetChange(m_dConstElevGrv, m_dConstElev);
				m_bRecompute |=  SetChange(m_nObsTagGrv, m_nObsTag);
				break;
			case 1:
				m_bRecompute |=  SetChange(m_dSensHeightMag, m_dSensHeight);
				m_bRecompute |=  SetChange(m_dConstElevMag, m_dConstElev);
				m_bRecompute |=  SetChange(m_nObsTagMag, m_nObsTag);
				break;
			case 2:
				m_bRecompute |=  SetChange(m_dSensHeightTns, m_dSensHeight);
				m_bRecompute |=  SetChange(m_dConstElevTns, m_dConstElev);
				m_bRecompute |=  SetChange(m_nObsTagTns, m_nObsTag);
				break;
		}
	}
	else {
		switch(n) {
			case 0:
				m_dSensHeight = m_dSensHeightGrv;
				m_dConstElev = m_dConstElevGrv;
				m_nObsTag = m_nObsTagGrv;
				m_strElevGrid = GetGrid()->GetFilePath();
				break;
			case 1:
				m_dSensHeight = m_dSensHeightMag;
				m_dConstElev = m_dConstElevMag;
				m_nObsTag = m_nObsTagMag;
				m_strElevGrid = GetGrid()->GetFilePath();
				break;
			case 2:
				m_dSensHeight = m_dSensHeightTns;
				m_dConstElev = m_dConstElevTns;
				m_nObsTag = m_nObsTagTns;
				m_strElevGrid = GetGrid()->GetFilePath();
				break;
		}
		UpdateData(FALSE);
	}
}


void CDlgObservations::OnEnChangeEditSensorHeight()
{
	UpdateMyData(TRUE);
	OnBnClickedRadioObsType();
}

void CDlgObservations::OnEnChangeEditSensorConst()
{
	UpdateMyData(TRUE);
	OnBnClickedRadio5();
}
