// DlgDefineObs.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgDefineObs.h"
#include "Grid.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CDlgDefineObs dialog


CDlgDefineObs::CDlgDefineObs(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgDefineObs::IDD, pParent)
	, m_strGridFilePath(_T(""))
{
	//{{AFX_DATA_INIT(CDlgDefineObs)
	m_dIncrX = 10.0f;
	m_dIncrY = 10.0f;
	m_dMaxX = 100.0f;
	m_dMaxY = 100.0f;
	m_dMaxZ = 0.0f;
	m_dMinX = -100.0f;
	m_dMinY = -100.0f;
	m_dMinZ = -100.0f;
	//}}AFX_DATA_INIT
	m_nTag = obsdNormal;

}


void CDlgDefineObs::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDlgDefineObs)
	DDX_Text(pDX, IDC_EDIT_INCR_X, m_dIncrX);
	DDX_Text(pDX, IDC_EDIT_INCR_Y, m_dIncrY);
	DDX_Text(pDX, IDC_EDIT_MAX_X, m_dMaxX);
	DDX_Text(pDX, IDC_EDIT_MAX_Y, m_dMaxY);
	DDX_Text(pDX, IDC_EDIT_MAX_Z, m_dMaxZ);
	DDX_Text(pDX, IDC_EDIT_MIN_X, m_dMinX);
	DDX_Text(pDX, IDC_EDIT_MIN_Y, m_dMinY);
	DDX_Text(pDX, IDC_EDIT_MIN_Z, m_dMinZ);
	//}}AFX_DATA_MAP
	DDX_Text(pDX, IDC_GRID_FILE, m_strGridFilePath);
}


BEGIN_MESSAGE_MAP(CDlgDefineObs, CDialog)
	ON_WM_DRAWITEM()
	ON_BN_CLICKED(IDC_GTF_FROM_GRID, OnBnClickedGtfFromGrid)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDlgDefineObs message handlers

BOOL CDlgDefineObs::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CDlgDefineObs::OnBnClickedGtfFromGrid()
{
	CGrid grd;
	GridInfo gi;

	if( grd.Open() ) {
		m_nTag = obsdGrid;
		gi = grd.GetInfo();
		m_dMinX = gi.dMinX;
		m_dMaxX = gi.dMaxX;
		m_dMinY = gi.dMinY;
		m_dMaxY = gi.dMaxY;
		m_csx = gi.xSize;
		m_csy = gi.ySize;
		m_dx0= gi.x0;
		m_dy0 = gi.y0;
		m_nRows = gi.nRow;
		m_nCols = gi.nCol;
		m_strGridFilePath = gi.strFilePath;

		GetDlgItem(IDC_EDIT_MAX_X)->EnableWindow(FALSE);
		GetDlgItem(IDC_EDIT_MAX_Y)->EnableWindow(FALSE);
		GetDlgItem(IDC_EDIT_MIN_X)->EnableWindow(FALSE);
		GetDlgItem(IDC_EDIT_MIN_Y)->EnableWindow(FALSE);

		GetDlgItem(IDC_EDIT_INCR_X)->EnableWindow(FALSE);
		GetDlgItem(IDC_EDIT_INCR_Y)->EnableWindow(FALSE);
	}
	UpdateData(FALSE);
}
