// DlgEditCheckMarks.cpp : implementation file
//

#include "stdafx.h"
#include "Mod3D.h"
#include "DlgEditCheckMarks.h"


// CDlgEditCheckMarks dialog

IMPLEMENT_DYNAMIC(CDlgEditCheckMarks, CDialog)
CDlgEditCheckMarks::CDlgEditCheckMarks(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgEditCheckMarks::IDD, pParent)
{
	m_pChm = NULL;

	m_txtColor = RGB(0, 51, 102);
	m_randColor = RGB(160, 160, 160);
	m_randPen.CreatePen(PS_SOLID, 1, RGB(160, 160, 160));
	m_randPenCur.CreatePen(PS_SOLID, 1, RGB(200, 200, 200));

}

CDlgEditCheckMarks::~CDlgEditCheckMarks()
{
}

void CDlgEditCheckMarks::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST_BODIES, m_list);
}


BEGIN_MESSAGE_MAP(CDlgEditCheckMarks, CDialog)
	ON_BN_CLICKED(IDC_DELETE, OnBnClickedDelete)
	ON_BN_CLICKED(IDC_PROPRTIES, OnBnClickedProprties)
	ON_BN_CLICKED(IDC_SHOW, OnBnClickedShow)
	ON_BN_CLICKED(IDC_ADD_NEW, OnBnClickedAddNew)
	ON_LBN_SELCHANGE(IDC_LIST_BODIES, OnLbnSelchangeListBodies)
	ON_WM_DRAWITEM()
	ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

BOOL CDlgEditCheckMarks::OnInitDialog()
{
	CDialog::OnInitDialog();

	int i;
	for( i=0; i<m_pObPtrArray->GetSize(); i++) {
		CMyObject* pObj = m_pObPtrArray->GetAt(i);
		if( pObj->IsKindOf( RUNTIME_CLASS( CGuideMark ) )  ) {
			CGuideMark* pChm = (CGuideMark*) pObj;
			m_list.AddString( (pChm->GetID() + " -> " + pChm->GetName()));
		}
	}
	if( m_list.GetCount() < 1 ) {
		AfxMessageBox( "Nothing to edit. Closing.", MB_OK | MB_ICONINFORMATION );
		OnCancel();
		return TRUE;
	}
	m_list.SetCurSel(i);
	
	Invalidate();

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}



// CDlgEditCheckMarks message handlers

void CDlgEditCheckMarks::OnBnClickedDelete()
{
	CMyObject* pObj;

	for( int i=0; i<m_pObPtrArray->GetSize(); i++) {
		pObj = m_pObPtrArray->GetAt(i);
		if( pObj->IsKindOf( RUNTIME_CLASS( CGuideMark ) ) ) {
			CGuideMark* pChm = (CGuideMark*) pObj;
			if( m_pChm == pChm ) {
				int res = AfxMessageBox("Delete? Sure?", MB_YESNO | MB_ICONQUESTION);
				if( res == IDYES ) {
					m_pObPtrArray->RemoveAt( i );
					m_pChm = NULL;
					m_list.ResetContent();
					OnInitDialog();
					return;
				}
			}
		}
	}
}

void CDlgEditCheckMarks::OnBnClickedProprties()
{
	if( m_pChm != NULL ) {
		m_pChm->Properties();
	}
	else {
		AfxMessageBox("No item selected.", MB_OK | MB_ICONINFORMATION);
	}
}

void CDlgEditCheckMarks::OnBnClickedShow()
{
	if( m_pChm != NULL ) {
		m_pChm->Show( !m_pChm->IsVisible() );
	}
	else {
		AfxMessageBox("No item selected.", MB_OK | MB_ICONINFORMATION);
	}
}

void CDlgEditCheckMarks::OnBnClickedAddNew()
{
		AfxMessageBox("Sorry I wanna sleep.", MB_OK | MB_ICONINFORMATION);
}

void CDlgEditCheckMarks::OnLbnSelchangeListBodies()
{
	int nIndex = m_list.GetCurSel();
	if (nIndex != LB_ERR) {
		CString strList;
		m_list.GetText( nIndex, strList );
		for( int i=0; i<m_pObPtrArray->GetSize(); i++) {
			CMyObject* pObj = m_pObPtrArray->GetAt(i);
			if( pObj->IsKindOf( RUNTIME_CLASS( CGuideMark ) ) ) {
				CGuideMark* pChm = (CGuideMark*) pObj;
				CString str;
				str = pChm->GetID() + " -> " + pChm->GetName();
				if( str == strList ) {
					m_pChm = pChm;
					return;
				}
			}
		}
	}
	m_pChm = NULL;
}
