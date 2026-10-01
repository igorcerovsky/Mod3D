// DlgCompProp.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgCompProp.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CDlgCompProp dialog


CDlgCompProp::CDlgCompProp(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgCompProp::IDD, pParent)
	, m_dTot(0)
	, m_dIncl(0)
	, m_dDecl(0)
	, m_dRefDens(0)
	, m_dDGx(0)
	, m_dDGy(0)
	, m_dDGz(0)
	, m_dDOx(0)
	, m_dDOy(0)
	, m_dDOz(0)
	, m_nGradElevTag(0)
	, m_fFlightElev(0)
	, m_fFlightHeight(0)
	, m_nComputationType(0)
	, m_bGrvTens(FALSE)
{
	m_bGrv = FALSE;
	m_bMag = FALSE;
	m_bSpherComp = FALSE;
	m_nColEnd = 0;
	m_nColStart = 0;
	m_fSensHigh = 0.0f;
	m_nFormula = -1;
	m_nRowEnd = 0;
	m_nRowStart = 0;
	m_bExtEdg = FALSE;
	m_dExtEdg = 0.0;
	m_nUnits = -1;
	m_dUnits = 0.0;

}


void CDlgCompProp::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_SPIN4, m_sp4);
	DDX_Control(pDX, IDC_SPIN3, m_sp3);
	DDX_Control(pDX, IDC_SPIN2, m_sp2);
	DDX_Control(pDX, IDC_SPIN1, m_sp1);
	DDX_Check(pDX, IDC_CHECK_SPHER_COMP, m_bSpherComp);
	DDX_Text(pDX, IDC_COL_END, m_nColEnd);
	DDX_Text(pDX, IDC_COL_START, m_nColStart);
	DDX_Text(pDX, IDC_EDIT_SENSOR_HIGH, m_fSensHigh);
	DDX_Text(pDX, IDC_ROW_END, m_nRowEnd);
	DDX_Text(pDX, IDC_ROW_START, m_nRowStart);
	DDX_Check(pDX, IDC_CHECK_EX_EDG, m_bExtEdg);
	DDX_Text(pDX, IDC_EDIT_EXTENT, m_dExtEdg);
	DDV_MinMaxDouble(pDX, m_dExtEdg, 0., 1000000.);
	DDX_Radio(pDX, IDC_RADIO_UNITS, m_nUnits);
	DDX_Text(pDX, IDC_EDIT_I_TOT2, m_dTot);
	DDX_Text(pDX, IDC_EDIT_I_INCL2, m_dIncl);
	DDX_Text(pDX, IDC_EDIT_I_DECL2, m_dDecl);
	DDX_Text(pDX, IDC_EDIT_DENSITY, m_dRefDens);
	DDX_Text(pDX, IDC_EDIT_GRD_X, m_dDGx);
	DDX_Text(pDX, IDC_EDIT_GRD_Y, m_dDGy);
	DDX_Text(pDX, IDC_EDIT_GRD_Z, m_dDGz);
	DDX_Text(pDX, IDC_EDIT_DO_X, m_dDOx);
	DDX_Text(pDX, IDC_EDIT_DO_Y, m_dDOy);
	DDX_Text(pDX, IDC_EDIT_DO_Z, m_dDOz);
	DDX_Radio(pDX, IDC_RADIO_GRAD_HEIGHT, m_nGradElevTag);
	DDX_Text(pDX, IDC_EDIT_GRAD_HEIGHT, m_fFlightElev);
	DDX_Text(pDX, IDC_EDIT_GRAD_HEIGHT_OVER_RELIF, m_fFlightHeight);
	DDX_Radio(pDX, IDC_RADIO_COMP_TYPE, m_nComputationType);
}


BEGIN_MESSAGE_MAP(CDlgCompProp, CDialog)
	ON_BN_CLICKED(IDC_BUT_ALL_COLS, OnButAllCols)
	ON_BN_CLICKED(IDC_BUT_ALL_ROWS, OnButAllRows)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDlgCompProp message handlers
BOOL CDlgCompProp::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	// TODO: Add extra initialization here
	m_sp1.SetRange(0, (short) m_nRow);
	m_sp1.SetPos(m_nRowStart);
	m_sp2.SetRange(0, (short) m_nRow);
	m_sp2.SetPos(m_nRowEnd);
	m_sp3.SetRange(0, (short) m_nCol);
	m_sp3.SetPos(m_nColStart);
	m_sp4.SetRange(0, (short) m_nCol);
	m_sp4.SetPos(m_nColEnd);

	m_nUnits = 2;
	if( m_dUnits == 1.0e6 ) 
		m_nUnits = 0;
	if( m_dUnits == 1.0e5 )
		m_nUnits = 1;
	UpdateData(FALSE);
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CDlgCompProp::OnButAllRows() 
{
	m_sp1.SetRange(0, (short) m_nRow);
	m_sp1.SetPos(0);
	m_sp2.SetRange(0, (short) m_nRow);
	m_sp2.SetPos(m_nRow);
}

void CDlgCompProp::OnButAllCols() 
{
	m_sp3.SetRange(0, (short) m_nCol);
	m_sp3.SetPos(0);
	m_sp4.SetRange(0, (short) m_nCol);
	m_sp4.SetPos(m_nCol);
}



void CDlgCompProp::OnOK() 
{
	// TODO: Add extra validation here
	UpdateData(TRUE);

	if(m_nUnits == 0 ) 
		m_dUnits = 1.0e6;
	if(m_nUnits == 1 ) 
		m_dUnits = 1.0e5;
	if(m_nRowStart > m_nRowEnd) {
		AfxMessageBox("RowStart > RowEnd.", MB_OK);
		return;
	}
	if(m_nColStart > m_nColEnd) {
		AfxMessageBox("ColumnStart > ColumnEnd.", MB_OK);
		return;
	}
	CDialog::OnOK();
}

