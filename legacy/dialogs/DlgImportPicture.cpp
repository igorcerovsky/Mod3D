// DlgImportPicture.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgImportPicture.h"
#include <atlimage.h>
#include <comdef.h>
#include <afxstr.h>
#include "DlgPen.h"


// CDlgImportPicture dialog

IMPLEMENT_DYNAMIC(CDlgImportPicture, CDialog)
CDlgImportPicture::CDlgImportPicture(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgImportPicture::IDD, pParent)
	, m_ys(0)
	, m_xs(0)
	, m_zs(0)
	, m_xe(0)
	, m_ye(0)
	, m_ze(0)
	, m_strName(_T("Enter description!"))
	, m_nProfNum(0)
	, m_nPicType(0)
	, m_nProfOrient(-1)
	, m_nOpacity(150)
	, m_bRow(FALSE)
	, m_bCol(FALSE)
	, m_strFilePath(_T(""))
	, m_bVisible(FALSE)
{
}

CDlgImportPicture::~CDlgImportPicture()
{
}

void CDlgImportPicture::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT_YT, m_ys);
	DDX_Text(pDX, IDC_EDIT_XT, m_xs);
	DDX_Text(pDX, IDC_EDIT_ZT, m_zs);

	DDX_Text(pDX, IDC_EDIT_XB, m_xe);
	DDX_Text(pDX, IDC_EDIT_YB, m_ye);
	DDX_Text(pDX, IDC_EDIT_ZB, m_ze);

	DDX_Text(pDX, IDC_EDIT1, m_strName);
	DDX_Radio(pDX, IDC_RADIO_PIC_TYPE, m_nPicType);
	DDX_Text(pDX, IDC_EDIT_OPACITY, m_nOpacity);
	DDV_MinMaxByte(pDX, m_nOpacity, 0, 255);
	DDX_Check(pDX, IDC_CHECK_SHOW_ROW, m_bRow);
	DDX_Check(pDX, IDC_CHECK_SHOW_COL, m_bCol);
	DDX_Text(pDX, IDC_EDIT_FILE_PATH, m_strFilePath);
	DDX_Check(pDX, IDC_CHECK_VISIBLE, m_bVisible);
}


BEGIN_MESSAGE_MAP(CDlgImportPicture, CDialog)
	ON_BN_CLICKED(IDC_BUTTON_BROWSE, OnBnClickedButtonBrowse)
	ON_BN_CLICKED(IDC_BUTTON_PEN, OnBnClickedButtonPen)
END_MESSAGE_MAP()


// CDlgImportPicture message handlers
BOOL CDlgImportPicture::OnInitDialog()
{
	CDialog::OnInitDialog();

	// TODO:  Add extra initialization here

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}


void CDlgImportPicture::OnOK()
{
	UpdateData();

	CDialog::OnOK();
}

void CDlgImportPicture::OnBnClickedButtonBrowse()
{
	m_pImg->Open();
	m_strFilePath = m_pImg->m_strFilePath;
	//CString strFilter;
	//CSimpleArray<GUID> aguidFileTypes;
	//HRESULT hResult;

	//hResult = m_pImg->GetExporterFilterString(strFilter, aguidFileTypes, _T( "All Image Files" ));
	//if (FAILED(hResult)) {
	//	CString fmt;
	//	fmt.Format("GetExporterFilter failed:\n%x - %s", hResult, _com_error(hResult).ErrorMessage());
	//	::AfxMessageBox(fmt);
	//	return;
	//}

	//CFileDialog dlg(TRUE, NULL, NULL, OFN_FILEMUSTEXIST, strFilter);
	//dlg.m_ofn.nFilterIndex = 1;
	//hResult = (int)dlg.DoModal();
	//if(hResult != IDOK) {
	//	return;
	//}

	//m_pImg->Destroy();
	//hResult = m_pImg->Load( dlg.GetPathName() );
	//m_strFilePath = dlg.GetPathName();
	//if (FAILED(hResult)) {
	//	CString fmt;
	//	fmt.Format("Load image failed:\n%x - %s", hResult, _com_error(hResult).ErrorMessage());
	//	::AfxMessageBox(fmt);
	//	return;
	//}
	//else {
	//	int width = m_pImg->GetWidth();
	//	int height = m_pImg->GetHeight();
	//	COLORREF	color, tmp;

	//	for(int i=0; i < height/2; i++) {
	//		for(int j=0; j < width; j++) {
	//			color = m_pImg->GetPixel(j, i);
	//			tmp = m_pImg->GetPixel(j, (height-1)-i);
	//			m_pImg->SetPixel(j, i, tmp);
	//			m_pImg->SetPixel(j, (height-1)-i, color);
	//		}
	//	}
	//}

	// set new file path
	CEdit* pEdit;
	pEdit = (CEdit*)GetDlgItem( IDC_EDIT_FILE_PATH );
	pEdit->SetWindowText( m_strFilePath );
}

void CDlgImportPicture::OnBnClickedButtonPen()
{
	CDlgPen dlg;

	dlg.m_lp = m_lpLine;
	if(dlg.DoModal() == IDOK) {
		m_lpLine = dlg.m_lp;
	}
}
