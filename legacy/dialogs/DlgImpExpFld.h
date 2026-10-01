#pragma once
#include "afxwin.h"
#include "Grid.h"
#include "Mod3DDoc.h"


// CDlgImpExpFld dialog

class CDlgImpExpFld : public CDialog
{
	DECLARE_DYNAMIC(CDlgImpExpFld)

	enum dlgType {
		dlgExport = 0,
		dlgImport = 1
	};

public:
	CDlgImpExpFld(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDlgImpExpFld();

// Dialog Data
	enum dlgID { IDD = IDD_DLG_FIELD_IMPEXP };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()

protected:
	BOOL ImportGrid(int nGrid);
	BOOL CheckGrid(GridInfo grdInf);
	BOOL NewDifferenceGrid( int nGrid );

public:
	CMod3DDoc*	m_pDoc;
	CGrid	*m_fldGrd;		// array of grids
	int		*m_grdActive;	// active grids see document
	OBSHDR	m_obsHdr;

	// import or export dialog
	int m_nType;
	CComboBox m_combo;

	CStringArray m_strExpAr;
	CStringArray m_strImpAr;

	afx_msg void OnBnClickedRadioExpimp();
	afx_msg void OnBnClickedRadio2();
	virtual BOOL OnInitDialog();



	afx_msg void OnBnClickedButtonProceed();
};
