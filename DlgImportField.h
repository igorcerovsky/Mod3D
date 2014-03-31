#pragma once
// DlgImportField.h : header file
//


#include "Grid.h"
#include "Mod3DDoc.h"
#include "afxwin.h"
/////////////////////////////////////////////////////////////////////////////
// CDlgImportField dialog

class CDlgImportField : public CDialog
{
	enum ImpExp{
		dlgImport = 0,
		dlgExport = 1
	};
// Construction
public:
	CDlgImportField(CWnd* pParent = NULL);   // standard constructor

public:
	CGrid	*m_fldGrd;		// array of grids
	int		*m_grdActive;	// active grids see document
	OBSHDR	m_obsHdr;

// Dialog Data
	enum dlgID{ IDD = IDD_DLG_MODEL_IMPORT_FIELD };
		// NOTE: the ClassWizard will add data members here

// Overrides
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	BOOL ImportGrid(int nGrid);
	BOOL CheckGrid(GridInfo grdInf);
	BOOL NewDifferenceGrid( int nGrid );

	DECLARE_MESSAGE_MAP()
public:
	// export or  import
	int m_nType;
	// export field array
	CStringArray	m_strExpAr;
	// import field array
	CStringArray	m_strImpAr;
	CComboBox m_combo;
	virtual BOOL OnInitDialog();
};

