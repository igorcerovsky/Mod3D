#pragma once
#include "afxwin.h"
#include "mod3ddoc.h"


// CDlgSelGridProp dialog

class CDlgSelGridProp : public CDialog
{
	DECLARE_DYNAMIC(CDlgSelGridProp)

public:
	CDlgSelGridProp(CWnd* pParent = NULL);   // standard constructor
	virtual ~CDlgSelGridProp();
	virtual BOOL OnInitDialog();

// Dialog Data
	enum { IDD = IDD_GRID_SEL_PROP };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()

public:
	CStringArray	m_strFld;		// active field

	CMod3DDoc*	m_pDoc;
	CComboBox	m_combo;
	afx_msg void OnCbnSelchangeComboFld();
};
