// DlgImpExpFld.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgImpExpFld.h"


// CDlgImpExpFld dialog

IMPLEMENT_DYNAMIC(CDlgImpExpFld, CDialog)
CDlgImpExpFld::CDlgImpExpFld(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgImpExpFld::IDD, pParent)
	, m_nType(0)
{
}

CDlgImpExpFld::~CDlgImpExpFld()
{
}

void CDlgImpExpFld::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Radio(pDX, IDC_RADIO_EXPIMP, m_nType);
	DDX_Control(pDX, IDC_COMBO_FLD, m_combo);
}


BEGIN_MESSAGE_MAP(CDlgImpExpFld, CDialog)
	ON_BN_CLICKED(IDC_RADIO_EXPIMP, OnBnClickedRadioExpimp)
	ON_BN_CLICKED(IDC_RADIO2, OnBnClickedRadio2)
	ON_BN_CLICKED(IDC_BUTTON_PROCEED, OnBnClickedButtonProceed)
END_MESSAGE_MAP()


// CDlgImpExpFld message handlers

void CDlgImpExpFld::OnBnClickedRadioExpimp()
{
	m_nType = dlgExport;
	OnInitDialog();
}

void CDlgImpExpFld::OnBnClickedRadio2()
{
	m_nType = dlgImport;
	OnInitDialog();
}

BOOL CDlgImpExpFld::OnInitDialog()
{
	CDialog::OnInitDialog();

	ASSERT( m_nType == dlgExport || m_nType == dlgImport );

	m_combo.ResetContent();

	int i;
	if( m_nType == dlgExport ) {
		if(!m_strExpAr.IsEmpty()) {
			for(i=0; i<m_strExpAr.GetSize(); i++) {
				m_combo.AddString(m_strExpAr.GetAt(i));
			}
		}
	}

	if( m_nType == dlgImport ) {
		if(!m_strImpAr.IsEmpty()) {
			for(i=0; i<m_strImpAr.GetSize(); i++) {
				CString str = m_strImpAr.GetAt(i);
				m_combo.AddString(str);
			}
		}
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

BOOL CDlgImpExpFld::CheckGrid( GridInfo grdInf )
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

BOOL CDlgImpExpFld::ImportGrid(int nGrid)
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

BOOL CDlgImpExpFld::NewDifferenceGrid( int nGrid ) 
{
	if( m_fldGrd[nGrid].IsEmpty() ) {
		m_fldGrd[nGrid].New(m_obsHdr.nRows, m_obsHdr.nCols, m_obsHdr.x0, m_obsHdr.y0, m_obsHdr.xSize, m_obsHdr.xSize);
		m_fldGrd[nGrid].ZeroData();
		m_fldGrd[nGrid].SetColorTable( CTBL_DIF );
		m_grdActive[nGrid] = TRUE;
	}
	return TRUE;
}

void CDlgImpExpFld::OnBnClickedButtonProceed()
{
	int		nSel;
	CString strSel;
	CGrid*	pGrd;

	nSel = m_combo.GetCurSel();
	if( nSel == CB_ERR ) {
		AfxMessageBox("Select field.", MB_OK | MB_ICONINFORMATION);
		return;
	}

	m_combo.GetLBText(nSel, strSel);

	pGrd = m_pDoc->GetGrid(strSel, nSel);
	if( pGrd != NULL ) {
		if( m_nType == dlgExport && !pGrd->IsEmpty() ) {
			pGrd->SaveAs();
		}
		if( m_nType == dlgImport ) {
			CGrid		grd;
			GridInfo	gi;

			if(pGrd->IsEmpty()) {
				gi = grd.GetInfoBrowse();
				if( CheckGrid(gi) ) {
					pGrd->Open( gi.strFilePath, gi.nType );
					m_pDoc->NewDifferenceField(nSel);
					return;
				}
			}
			else {
				int ret = AfxMessageBox("Grid is not empty. Replace?", MB_ICONQUESTION | MB_YESNO);
				if( ret == IDYES) {
					gi = grd.GetInfoBrowse();
					if( CheckGrid( gi) ) {
						pGrd->Close();
						pGrd->Open( gi.strFilePath, gi.nType );
						m_pDoc->NewDifferenceField(nSel);
						return;
					}
				}
			}
		}
	}
}


